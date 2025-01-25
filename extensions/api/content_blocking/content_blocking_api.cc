// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/content_blocking/content_blocking_api.h"

#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "components/ad_blocker/adblock_known_sources_handler.h"
#include "components/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_content.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_factory.h"
#include "components/request_filter/adblock_filter/adblock_state_and_logs.h"
#include "components/request_filter/adblock_filter/adblock_tab_state_and_logs.h"
#include "extensions/schema/content_blocking.h"
#include "extensions/tools/vivaldi_tools.h"

namespace extensions {

namespace {

constexpr const char *kPartnerListURL = "https://downloads.vivaldi.com/lists/vivaldi/partners-current.txt";

std::optional<adblock_filter::RuleGroup> FromVivaldiContentBlockingRuleGroup(
    vivaldi::content_blocking::RuleGroup rule_group) {
  switch (rule_group) {
    case vivaldi::content_blocking::RuleGroup::kTracking:
      return adblock_filter::RuleGroup::kTrackingRules;
    case vivaldi::content_blocking::RuleGroup::kAdBlocking:
      return adblock_filter::RuleGroup::kAdBlockingRules;
    default:
      NOTREACHED();
  }
}

adblock_filter::RuleSourceSettings FromVivaldiContentBlockingRuleSourceSettings(
    const vivaldi::content_blocking::RuleSourceSettings& settings) {
  return adblock_filter::RuleSourceSettings{
      .allow_abp_snippets = settings.allow_abp_snippets,
      .naked_hostname_is_pure_host = settings.naked_hostname_is_pure_host,
      .use_whole_document_allow = settings.use_whole_document_allow};
}

vivaldi::content_blocking::RuleGroup ToVivaldiContentBlockingRuleGroup(
    adblock_filter::RuleGroup rule_group) {
  switch (rule_group) {
    case adblock_filter::RuleGroup::kTrackingRules:
      return vivaldi::content_blocking::RuleGroup::kTracking;
    case adblock_filter::RuleGroup::kAdBlockingRules:
      return vivaldi::content_blocking::RuleGroup::kAdBlocking;
  }
}

std::optional<adblock_filter::RuleManager::ExceptionsList>
FromVivaldiContentBlockingExceptionList(
    vivaldi::content_blocking::ExceptionList exception_list) {
  switch (exception_list) {
    case vivaldi::content_blocking::ExceptionList::kProcessList:
      return adblock_filter::RuleManager::kProcessList;
    case vivaldi::content_blocking::ExceptionList::kExemptList:
      return adblock_filter::RuleManager::kExemptList;
    default:
      NOTREACHED();
  }
}

vivaldi::content_blocking::ExceptionList ToVivaldiContentBlockingExceptionList(
    adblock_filter::RuleManager::ExceptionsList exception_list) {
  switch (exception_list) {
    case adblock_filter::RuleManager::kProcessList:
      return vivaldi::content_blocking::ExceptionList::kProcessList;
    case adblock_filter::RuleManager::kExemptList:
      return vivaldi::content_blocking::ExceptionList::kExemptList;
  }
}

vivaldi::content_blocking::FetchResult ToVivaldiContentBlockingFetchResult(
    adblock_filter::FetchResult fetch_result) {
  switch (fetch_result) {
    case adblock_filter::FetchResult::kSuccess:
      return vivaldi::content_blocking::FetchResult::kSuccess;
    case adblock_filter::FetchResult::kDownloadFailed:
      return vivaldi::content_blocking::FetchResult::kDownloadFailed;
    case adblock_filter::FetchResult::kFileNotFound:
      return vivaldi::content_blocking::FetchResult::kFileNotFound;
    case adblock_filter::FetchResult::kFileReadError:
      return vivaldi::content_blocking::FetchResult::kFileReadError;
    case adblock_filter::FetchResult::kFileUnsupported:
      return vivaldi::content_blocking::FetchResult::kFileUnsupported;
    case adblock_filter::FetchResult::kFailedSavingParsedRules:
      return vivaldi::content_blocking::FetchResult::kFailedSavingParsedRules;
    case adblock_filter::FetchResult::kUnknown:
      return vivaldi::content_blocking::FetchResult::kUnknown;
  }
}

vivaldi::content_blocking::RuleSource
ToVivaldiContentBlockingRuleSourceFromCore(
    const adblock_filter::RuleSourceCore& core) {
  vivaldi::content_blocking::RuleSource result;
  if (core.is_from_url())
    result.source_url = core.source_url().spec();
  else
    result.source_file = core.source_file().AsUTF8Unsafe();
  result.is_from_url = core.is_from_url();
  result.id = core.id();
  result.loaded = false;

  result.settings.allow_abp_snippets = core.settings().allow_abp_snippets;
  result.settings.naked_hostname_is_pure_host =
      core.settings().naked_hostname_is_pure_host;
  result.settings.use_whole_document_allow =
      core.settings().use_whole_document_allow;

  result.removable = true;
  result.rules_list_checksum = "";
  result.unsafe_adblock_metadata.homepage = "";
  result.unsafe_adblock_metadata.title = "";
  result.unsafe_adblock_metadata.expires = 0;
  result.unsafe_adblock_metadata.license = "";
  result.unsafe_adblock_metadata.version = 0;
  result.last_update = 0;
  result.next_fetch = 0;
  result.last_fetch_result = vivaldi::content_blocking::FetchResult::kUnknown;
  result.rules_info.valid_rules = 0;
  result.rules_info.unsupported_rules = 0;
  result.rules_info.invalid_rules = 0;

  return result;
}

void UpdateVivaldiContentBlockingRuleSourceWithLoadedSource(
    const adblock_filter::ActiveRuleSource& rule_source,
    vivaldi::content_blocking::RuleSource* result) {
  result->rules_list_checksum = rule_source.rules_list_checksum;
  result->unsafe_adblock_metadata.homepage =
      rule_source.unsafe_adblock_metadata.homepage.is_valid()
          ? rule_source.unsafe_adblock_metadata.homepage.spec()
          : "";
  result->unsafe_adblock_metadata.title =
      rule_source.unsafe_adblock_metadata.title;
  result->unsafe_adblock_metadata.expires =
      rule_source.unsafe_adblock_metadata.expires.InHours();
  result->unsafe_adblock_metadata.license =
      rule_source.unsafe_adblock_metadata.license.is_valid()
          ? rule_source.unsafe_adblock_metadata.license.spec()
          : "";
  result->unsafe_adblock_metadata.version =
      rule_source.unsafe_adblock_metadata.version;
  result->last_update = rule_source.last_update.InMillisecondsFSinceUnixEpoch();
  result->next_fetch = rule_source.next_fetch.InMillisecondsFSinceUnixEpoch();
  result->is_fetching = rule_source.is_fetching;
  result->last_fetch_result =
      ToVivaldiContentBlockingFetchResult(rule_source.last_fetch_result);
  result->rules_info.valid_rules = rule_source.rules_info.valid_rules;
  result->rules_info.unsupported_rules =
      rule_source.rules_info.unsupported_rules;
  result->rules_info.invalid_rules = rule_source.rules_info.invalid_rules;

  result->loaded = true;
}

vivaldi::content_blocking::RuleSource ToVivaldiContentBlockingRuleSource(
    const adblock_filter::KnownRuleSource& known_source) {
  vivaldi::content_blocking::RuleSource result =
      ToVivaldiContentBlockingRuleSourceFromCore(known_source.core);
  result.removable = known_source.removable;

  return result;
}

vivaldi::content_blocking::RuleSource ToVivaldiContentBlockingRuleSource(
    const adblock_filter::ActiveRuleSource& rule_source) {
  vivaldi::content_blocking::RuleSource result =
      ToVivaldiContentBlockingRuleSourceFromCore(rule_source.core);
  UpdateVivaldiContentBlockingRuleSourceWithLoadedSource(rule_source, &result);

  return result;
}

void RecordBLockedUrls(
    const adblock_filter::TabStateAndLogs::BlockedUrlInfoMap& blocked_urls,
    std::vector<vivaldi::content_blocking::BlockedUrlsInfo>*
        blocked_urls_info) {
  for (const auto& blocked_url : blocked_urls) {
    blocked_urls_info->emplace_back();
    blocked_urls_info->back().url = blocked_url.first;
    blocked_urls_info->back().blocked_count = blocked_url.second.blocked_count;
  }
}

template <class T>
std::vector<T> CopySetToVector(const std::set<T> set) {
  return std::vector<T>(set.begin(), set.end());
}

}  // namespace

ContentBlockingEventRouter::ContentBlockingEventRouter(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  adblock_filter::RuleService* rules_service =
      adblock_filter::RuleServiceFactory::GetForBrowserContext(
          browser_context_);
  if (rules_service) {
    rules_service->AddObserver(this);
    if (rules_service->IsLoaded()) {
      rules_service->GetKnownSourcesHandler()->AddObserver(this);
      rules_service->GetStateAndLogs()->AddObserver(this);
      rules_service->GetRuleManager()->AddObserver(this);
    }
  }
}

void ContentBlockingEventRouter::OnRuleServiceStateLoaded(
    adblock_filter::RuleService* rule_service) {
  rule_service->GetKnownSourcesHandler()->AddObserver(this);
  rule_service->GetStateAndLogs()->AddObserver(this);
  rule_service->GetRuleManager()->AddObserver(this);
}

void ContentBlockingEventRouter::OnKnownSourceAdded(
    adblock_filter::RuleGroup group,
    const adblock_filter::KnownRuleSource& rule_source) {
  ::vivaldi::BroadcastEvent(
      vivaldi::content_blocking::OnRuleSourceAdded::kEventName,
      vivaldi::content_blocking::OnRuleSourceAdded::Create(
          ToVivaldiContentBlockingRuleSource(rule_source),
          ToVivaldiContentBlockingRuleGroup(group)),
      browser_context_);
}

void ContentBlockingEventRouter::OnKnownSourceRemoved(
    adblock_filter::RuleGroup group,
    uint32_t source_id) {
  ::vivaldi::BroadcastEvent(
      vivaldi::content_blocking::OnRuleSourceRemoved::kEventName,
      vivaldi::content_blocking::OnRuleSourceRemoved::Create(
          source_id, ToVivaldiContentBlockingRuleGroup(group)),
      browser_context_);
}

void ContentBlockingEventRouter::OnKnownSourceEnabled(
    adblock_filter::RuleGroup group,
    uint32_t source_id) {
  ::vivaldi::BroadcastEvent(
      vivaldi::content_blocking::OnRuleSourceEnabled::kEventName,
      vivaldi::content_blocking::OnRuleSourceEnabled::Create(
          source_id, ToVivaldiContentBlockingRuleGroup(group)),
      browser_context_);
}
void ContentBlockingEventRouter::OnKnownSourceDisabled(
    adblock_filter::RuleGroup group,
    uint32_t source_id) {
  ::vivaldi::BroadcastEvent(
      vivaldi::content_blocking::OnRuleSourceDisabled::kEventName,
      vivaldi::content_blocking::OnRuleSourceDisabled::Create(
          source_id, ToVivaldiContentBlockingRuleGroup(group)),
      browser_context_);
}

void ContentBlockingEventRouter::OnRuleSourceUpdated(
    adblock_filter::RuleGroup group,
    const adblock_filter::ActiveRuleSource& rule_source) {
  ::vivaldi::BroadcastEvent(
      vivaldi::content_blocking::OnRuleSourceUpdated::kEventName,
      vivaldi::content_blocking::OnRuleSourceUpdated::Create(
          ToVivaldiContentBlockingRuleSource(rule_source),
          ToVivaldiContentBlockingRuleGroup(group)),
      browser_context_);
}

void ContentBlockingEventRouter::OnExceptionListStateChanged(
    adblock_filter::RuleGroup group) {
  ::vivaldi::BroadcastEvent(
      vivaldi::content_blocking::OnStateChanged::kEventName,
      vivaldi::content_blocking::OnStateChanged::Create(
          ToVivaldiContentBlockingRuleGroup(group)),
      browser_context_);
}

void ContentBlockingEventRouter::OnGroupStateChanged(
    adblock_filter::RuleGroup group) {
  ::vivaldi::BroadcastEvent(
      vivaldi::content_blocking::OnStateChanged::kEventName,
      vivaldi::content_blocking::OnStateChanged::Create(
          ToVivaldiContentBlockingRuleGroup(group)),
      browser_context_);
}

void ContentBlockingEventRouter::OnExceptionListChanged(
    adblock_filter::RuleGroup group,
    adblock_filter::RuleManager::ExceptionsList list) {
  ::vivaldi::BroadcastEvent(
      vivaldi::content_blocking::OnExceptionsChanged::kEventName,
      vivaldi::content_blocking::OnExceptionsChanged::Create(
          ToVivaldiContentBlockingRuleGroup(group),
          ToVivaldiContentBlockingExceptionList(list)),
      browser_context_);
}

void ContentBlockingEventRouter::OnNewBlockedUrlsReported(
    adblock_filter::RuleGroup group,
    std::set<content::WebContents*> tabs_with_new_blocks) {
  std::vector<int> tab_ids;
  for (const auto* web_content : tabs_with_new_blocks) {
    tab_ids.push_back(ExtensionTabUtil::GetTabId(web_content));
  }
  ::vivaldi::BroadcastEvent(
      vivaldi::content_blocking::OnUrlsBlocked::kEventName,
      vivaldi::content_blocking::OnUrlsBlocked::Create(
          ToVivaldiContentBlockingRuleGroup(group), tab_ids),
      browser_context_);
}

void ContentBlockingEventRouter::OnAllowAttributionChanged(
    content::WebContents* web_contents) {
  ::vivaldi::BroadcastEvent(
      vivaldi::content_blocking::OnAdAttributionDomainChanged::kEventName,
      vivaldi::content_blocking::OnAdAttributionDomainChanged::Create(
          ExtensionTabUtil::GetTabId(web_contents)),
      browser_context_);
}

void ContentBlockingEventRouter::OnNewAttributionTrackerAllowed(
    std::set<content::WebContents*> tabs_with_new_attribution_trackers) {
  std::vector<int> tab_ids;
  for (const auto* web_content : tabs_with_new_attribution_trackers) {
    tab_ids.push_back(ExtensionTabUtil::GetTabId(web_content));
  }
  ::vivaldi::BroadcastEvent(
      vivaldi::content_blocking::OnAdAttributionTrackersAllowed::kEventName,
      vivaldi::content_blocking::OnAdAttributionTrackersAllowed::Create(
          tab_ids),
      browser_context_);
}

void ContentBlockingEventRouter::Shutdown() {
  adblock_filter::RuleService* rules_service =
      adblock_filter::RuleServiceFactory::GetForBrowserContext(
          browser_context_);
  if (rules_service) {
    rules_service->RemoveObserver(this);
    if (rules_service->IsLoaded()) {
      rules_service->GetKnownSourcesHandler()->RemoveObserver(this);
      rules_service->GetStateAndLogs()->RemoveObserver(this);
      rules_service->GetRuleManager()->RemoveObserver(this);
    }
  }
}

ContentBlockingEventRouter::~ContentBlockingEventRouter() {}

ContentBlockingAPI::ContentBlockingAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(
      this, vivaldi::content_blocking::OnStateChanged::kEventName);
  event_router->RegisterObserver(
      this, vivaldi::content_blocking::OnExceptionsChanged::kEventName);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<ContentBlockingAPI>>::
    DestructorAtExit g_factory_content_blocking = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<ContentBlockingAPI>*
ContentBlockingAPI::GetFactoryInstance() {
  return g_factory_content_blocking.Pointer();
}

ContentBlockingAPI::~ContentBlockingAPI() {}

void ContentBlockingAPI::OnListenerAdded(const EventListenerInfo& details) {
  content_blocking_event_router_.reset(
      new ContentBlockingEventRouter(browser_context_));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

// KeyedService implementation.
void ContentBlockingAPI::Shutdown() {
  if (content_blocking_event_router_)
    content_blocking_event_router_->Shutdown();
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

ExtensionFunction::ResponseAction AdBlockFunction::Run() {
  adblock_filter::RuleService* rules_service =
      adblock_filter::RuleServiceFactory::GetForBrowserContext(
          browser_context());

  if (!rules_service->IsLoaded()) {
    rules_service->AddObserver(this);
    return RespondLater();
  }

  return RespondNow(RunWithService(rules_service));
}

void AdBlockFunction::OnRuleServiceStateLoaded(
    adblock_filter::RuleService* rule_service) {
  rule_service->RemoveObserver(this);
  return Respond(RunWithService(rule_service));
}

/*static*/
ExtensionFunction::ResponseValue AdBlockFunction::ValidationFailure(
    AdBlockFunction* function) {
  return function->BadMessage();
}

ExtensionFunction::ResponseValue
ContentBlockingSetRuleGroupEnabledFunction::RunWithService(
    adblock_filter::RuleService* rules_service) {
  using vivaldi::content_blocking::SetRuleGroupEnabled::Params;
  std::optional<Params> params(Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  rules_service->SetRuleGroupEnabled(
      FromVivaldiContentBlockingRuleGroup(params->rule_group).value(),
      params->enabled);

  return NoArguments();
}

ExtensionFunction::ResponseValue
ContentBlockingIsRuleGroupEnabledFunction::RunWithService(
    adblock_filter::RuleService* rules_service) {
  using vivaldi::content_blocking::IsRuleGroupEnabled::Params;
  namespace Results = vivaldi::content_blocking::IsRuleGroupEnabled::Results;
  std::optional<Params> params(Params::Create(args()));

  return ArgumentList(Results::Create(rules_service->IsRuleGroupEnabled(
      FromVivaldiContentBlockingRuleGroup(params->rule_group).value())));
}

ExtensionFunction::ResponseValue
ContentBlockingAddKnownSourceFromURLFunction::RunWithService(
    adblock_filter::RuleService* rules_service) {
  using vivaldi::content_blocking::AddKnownSourceFromURL::Params;
  namespace Results = vivaldi::content_blocking::AddKnownSourceFromURL::Results;
  std::optional<Params> params(Params::Create(args()));

  std::optional<adblock_filter::RuleSourceCore> source_core =
      adblock_filter::RuleSourceCore::FromUrl(GURL(params->url));

  if (!source_core) {
    return Error("Invalid url");
  }

  if (params->source_settings) {
    source_core->set_settings(
        FromVivaldiContentBlockingRuleSourceSettings(*params->source_settings));
  }

  int32_t source_id = source_core->id();

  if (!rules_service->GetKnownSourcesHandler()->AddSource(
          FromVivaldiContentBlockingRuleGroup(params->rule_group).value(),
          std::move(*source_core)))
    return Error("Failed to add rule source");

  return ArgumentList(Results::Create(source_id));
}

ExtensionFunction::ResponseValue
ContentBlockingAddKnownSourceFromFileFunction::RunWithService(
    adblock_filter::RuleService* rules_service) {
  using vivaldi::content_blocking::AddKnownSourceFromFile::Params;
  namespace Results =
      vivaldi::content_blocking::AddKnownSourceFromFile::Results;
  std::optional<Params> params(Params::Create(args()));

  std::optional<adblock_filter::RuleSourceCore> source_core =
      adblock_filter::RuleSourceCore::FromFile(
          base::FilePath::FromUTF8Unsafe(params->file));

  if (!source_core) {
    return Error("Invalid file path");
  }

  if (params->source_settings) {
    source_core->set_settings(
        FromVivaldiContentBlockingRuleSourceSettings(*params->source_settings));
  }

  int32_t source_id = source_core->id();

  if (!rules_service->GetKnownSourcesHandler()->AddSource(
          FromVivaldiContentBlockingRuleGroup(params->rule_group).value(),
          std::move(*source_core))) {
    return Error("Failed to add rule source");
  }

  return ArgumentList(Results::Create(source_id));
}

ExtensionFunction::ResponseValue
ContentBlockingSetKnownSourceSettingsFunction::RunWithService(
    adblock_filter::RuleService* rules_service) {
  using vivaldi::content_blocking::SetKnownSourceSettings::Params;
  namespace Results =
      vivaldi::content_blocking::SetKnownSourceSettings::Results;
  std::optional<Params> params(Params::Create(args()));

  bool success = rules_service->GetKnownSourcesHandler()->SetSourceSettings(
      FromVivaldiContentBlockingRuleGroup(params->rule_group).value(),
      params->source_id,
      FromVivaldiContentBlockingRuleSourceSettings(params->source_settings));
  return ArgumentList(Results::Create(success));
}

ExtensionFunction::ResponseValue
ContentBlockingEnableSourceFunction::RunWithService(
    adblock_filter::RuleService* rules_service) {
  using vivaldi::content_blocking::EnableSource::Params;
  std::optional<Params> params(Params::Create(args()));

  if (!rules_service->GetKnownSourcesHandler()->EnableSource(
          FromVivaldiContentBlockingRuleGroup(params->rule_group).value(),
          params->source_id))
    return Error("Source not found");

  return NoArguments();
}

ExtensionFunction::ResponseValue
ContentBlockingDisableSourceFunction::RunWithService(
    adblock_filter::RuleService* rules_service) {
  using vivaldi::content_blocking::DisableSource::Params;
  std::optional<Params> params(Params::Create(args()));

  rules_service->GetKnownSourcesHandler()->DisableSource(
      FromVivaldiContentBlockingRuleGroup(params->rule_group).value(),
      params->source_id);

  return NoArguments();
}

ExtensionFunction::ResponseValue
ContentBlockingFetchSourceNowFunction::RunWithService(
    adblock_filter::RuleService* rules_service) {
  using vivaldi::content_blocking::FetchSourceNow::Params;
  std::optional<Params> params(Params::Create(args()));

  if (!rules_service->GetRuleManager()->FetchRuleSourceNow(
          FromVivaldiContentBlockingRuleGroup(params->rule_group).value(),
          params->source_id))
    return Error("Source not found");

  return NoArguments();
}

ExtensionFunction::ResponseValue
ContentBlockingDeleteKnownSourceFunction::RunWithService(
    adblock_filter::RuleService* rules_service) {
  using vivaldi::content_blocking::DeleteKnownSource::Params;
  std::optional<Params> params(Params::Create(args()));

  rules_service->GetKnownSourcesHandler()->RemoveSource(
      FromVivaldiContentBlockingRuleGroup(params->rule_group).value(),
      params->source_id);

  return NoArguments();
}

ExtensionFunction::ResponseValue
ContentBlockingResetPresetSourcesFunction::RunWithService(
    adblock_filter::RuleService* rules_service) {
  using vivaldi::content_blocking::ResetPresetSources::Params;
  std::optional<Params> params(Params::Create(args()));

  rules_service->GetKnownSourcesHandler()->ResetPresetSources(
      FromVivaldiContentBlockingRuleGroup(params->rule_group).value());

  return NoArguments();
}

ExtensionFunction::ResponseValue
ContentBlockingGetRuleSourceFunction::RunWithService(
    adblock_filter::RuleService* rules_service) {
  using vivaldi::content_blocking::GetRuleSource::Params;
  namespace Results = vivaldi::content_blocking::GetRuleSource::Results;
  std::optional<Params> params(Params::Create(args()));

  adblock_filter::RuleGroup group =
      FromVivaldiContentBlockingRuleGroup(params->rule_group).value();
  auto known_source = rules_service->GetKnownSourcesHandler()->GetSource(
      group, params->source_id);
  if (!known_source)
    return (Error("Rule source not found"));
  auto result = ToVivaldiContentBlockingRuleSource(known_source.value());

  auto rule_source = rules_service->GetRuleManager()->GetRuleSource(
      FromVivaldiContentBlockingRuleGroup(params->rule_group).value(),
      params->source_id);

  if (rule_source)
    UpdateVivaldiContentBlockingRuleSourceWithLoadedSource(rule_source.value(),
                                                           &result);

  return ArgumentList(Results::Create(result));
}

ExtensionFunction::ResponseValue
ContentBlockingGetRuleSourcesFunction::RunWithService(
    adblock_filter::RuleService* rules_service) {
  using vivaldi::content_blocking::GetRuleSources::Params;
  namespace Results = vivaldi::content_blocking::GetRuleSources::Results;
  std::optional<Params> params(Params::Create(args()));

  adblock_filter::RuleGroup group =
      FromVivaldiContentBlockingRuleGroup(params->rule_group).value();
  auto known_sources =
      rules_service->GetKnownSourcesHandler()->GetSources(group);

  std::vector<vivaldi::content_blocking::RuleSource> result;
  for (const auto& [id, known_source] : known_sources) {
    auto rule_source = ToVivaldiContentBlockingRuleSource(known_source);
    auto loaded_source =
        rules_service->GetRuleManager()->GetRuleSource(group, id);
    if (loaded_source)
      UpdateVivaldiContentBlockingRuleSourceWithLoadedSource(
          loaded_source.value(), &rule_source);
    result.push_back(std::move(rule_source));
  }

  return ArgumentList(Results::Create(result));
}

ExtensionFunction::ResponseValue
ContentBlockingSetActiveExceptionsListFunction::RunWithService(
    adblock_filter::RuleService* rules_service) {
  using vivaldi::content_blocking::SetActiveExceptionsList::Params;
  std::optional<Params> params(Params::Create(args()));

  rules_service->GetRuleManager()->SetActiveExceptionList(
      FromVivaldiContentBlockingRuleGroup(params->rule_group).value(),
      FromVivaldiContentBlockingExceptionList(params->state).value());

  return NoArguments();
}

ExtensionFunction::ResponseValue
ContentBlockingGetActiveExceptionsListFunction::RunWithService(
    adblock_filter::RuleService* rules_service) {
  using vivaldi::content_blocking::GetActiveExceptionsList::Params;
  namespace Results =
      vivaldi::content_blocking::GetActiveExceptionsList::Results;
  std::optional<Params> params(Params::Create(args()));

  return ArgumentList(Results::Create(ToVivaldiContentBlockingExceptionList(
      rules_service->GetRuleManager()->GetActiveExceptionList(
          FromVivaldiContentBlockingRuleGroup(params->rule_group).value()))));
}

ExtensionFunction::ResponseValue
ContentBlockingAddExceptionForDomainFunction::RunWithService(
    adblock_filter::RuleService* rules_service) {
  using vivaldi::content_blocking::AddExceptionForDomain::Params;
  std::optional<Params> params(Params::Create(args()));

  rules_service->GetRuleManager()->AddExceptionForDomain(
      FromVivaldiContentBlockingRuleGroup(params->rule_group).value(),
      FromVivaldiContentBlockingExceptionList(params->exception_list).value(),
      params->domain);

  return NoArguments();
}

ExtensionFunction::ResponseValue
ContentBlockingRemoveExceptionForDomainFunction::RunWithService(
    adblock_filter::RuleService* rules_service) {
  using vivaldi::content_blocking::RemoveExceptionForDomain::Params;
  std::optional<Params> params(Params::Create(args()));

  rules_service->GetRuleManager()->RemoveExceptionForDomain(
      FromVivaldiContentBlockingRuleGroup(params->rule_group).value(),
      FromVivaldiContentBlockingExceptionList(params->exception_list).value(),
      params->domain);

  return NoArguments();
}

ExtensionFunction::ResponseValue
ContentBlockingRemoveAllExceptionsFunction::RunWithService(
    adblock_filter::RuleService* rules_service) {
  using vivaldi::content_blocking::RemoveAllExceptions::Params;
  std::optional<Params> params(Params::Create(args()));

  rules_service->GetRuleManager()->RemoveAllExceptions(
      FromVivaldiContentBlockingRuleGroup(params->rule_group).value(),
      FromVivaldiContentBlockingExceptionList(params->exception_list).value());

  return NoArguments();
}

ExtensionFunction::ResponseValue
ContentBlockingGetExceptionsFunction::RunWithService(
    adblock_filter::RuleService* rules_service) {
  using vivaldi::content_blocking::GetExceptions::Params;
  namespace Results = vivaldi::content_blocking::GetExceptions::Results;
  std::optional<Params> params(Params::Create(args()));

  return ArgumentList(Results::Create(
      CopySetToVector(rules_service->GetRuleManager()->GetExceptions(
          FromVivaldiContentBlockingRuleGroup(params->rule_group).value(),
          FromVivaldiContentBlockingExceptionList(params->exception_list)
              .value()))));
}

ExtensionFunction::ResponseValue
ContentBlockingGetAllExceptionListsFunction::RunWithService(
    adblock_filter::RuleService* rules_service) {
  namespace Results = vivaldi::content_blocking::GetAllExceptionLists::Results;

  Results::Origins result;
  result.ad_blocking.exempt_list =
      CopySetToVector(rules_service->GetRuleManager()->GetExceptions(
          adblock_filter::RuleGroup::kAdBlockingRules,
          adblock_filter::RuleManager::kExemptList));
  result.ad_blocking.process_list =
      CopySetToVector(rules_service->GetRuleManager()->GetExceptions(
          adblock_filter::RuleGroup::kAdBlockingRules,
          adblock_filter::RuleManager::kProcessList));
  result.tracking.exempt_list =
      CopySetToVector(rules_service->GetRuleManager()->GetExceptions(
          adblock_filter::RuleGroup::kTrackingRules,
          adblock_filter::RuleManager::kExemptList));
  result.tracking.process_list =
      CopySetToVector(rules_service->GetRuleManager()->GetExceptions(
          adblock_filter::RuleGroup::kTrackingRules,
          adblock_filter::RuleManager::kProcessList));

  return ArgumentList(Results::Create(result));
}

ExtensionFunction::ResponseValue
ContentBlockingGetBlockedUrlsInfoFunction::RunWithService(
    adblock_filter::RuleService* rules_service) {
  using vivaldi::content_blocking::GetBlockedUrlsInfo::Params;
  namespace Results = vivaldi::content_blocking::GetBlockedUrlsInfo::Results;
  std::optional<Params> params(Params::Create(args()));

  adblock_filter::RuleGroup group =
      FromVivaldiContentBlockingRuleGroup(params->rule_group).value();

  std::vector<vivaldi::content_blocking::TabBlockedUrlsInfo>
      tab_blocked_urls_infos;

  for (auto tab_id : params->tab_ids) {
    content::WebContents* web_contents;
    if (ExtensionTabUtil::GetTabById(tab_id, browser_context(), true,
                                     &web_contents) &&
        web_contents) {
      adblock_filter::TabStateAndLogs* tab_state_and_logs =
          rules_service->GetStateAndLogs()->GetTabHelper(web_contents);
      if (!tab_state_and_logs)
        continue;

      const adblock_filter::TabStateAndLogs::TabBlockedUrlInfo&
          tab_blocked_urls_info = tab_state_and_logs->GetBlockedUrlsInfo(group);
      if (tab_blocked_urls_info.blocked_trackers.empty() &&
          tab_blocked_urls_info.blocked_urls.empty()) {
        continue;
      }
      tab_blocked_urls_infos.emplace_back();
      tab_blocked_urls_infos.back().tab_id =
          ExtensionTabUtil::GetTabId(web_contents);
      tab_blocked_urls_infos.back().total_blocked_count =
          tab_blocked_urls_info.total_count;

      for (const auto& blocked_tracker :
           tab_blocked_urls_info.blocked_trackers) {
        auto* source_to_info_map =
            rules_service->GetStateAndLogs()->GetTrackerInfo(
                group, blocked_tracker.first);
        if (!source_to_info_map) {
          // The information forthis tracker went away since the blocking was
          // recorded. Just record the blocked urls as not part of a known
          // tracker
          RecordBLockedUrls(blocked_tracker.second.blocked_urls,
                            &tab_blocked_urls_infos.back().blocked_urls_info);
          continue;
        }

        tab_blocked_urls_infos.back().blocked_trackers_info.emplace_back();
        auto& blocked_tracker_info =
            tab_blocked_urls_infos.back().blocked_trackers_info.back();
        blocked_tracker_info.domain = blocked_tracker.first;
        blocked_tracker_info.blocked_count =
            blocked_tracker.second.blocked_count;
        RecordBLockedUrls(blocked_tracker.second.blocked_urls,
                          &blocked_tracker_info.blocked_urls);
        for (const auto& source_to_info : *source_to_info_map) {
          blocked_tracker_info.tracker_info.emplace_back();
          blocked_tracker_info.tracker_info.back().source_id =
              source_to_info.first;
          blocked_tracker_info.tracker_info.back().info =
              source_to_info.second.Clone();
        }
      }

      RecordBLockedUrls(tab_blocked_urls_info.blocked_urls,
                        &tab_blocked_urls_infos.back().blocked_urls_info);
    }
  }

  return ArgumentList(Results::Create(tab_blocked_urls_infos));
}

std::vector<vivaldi::content_blocking::BlockedCounter> ToVivaldiBlockedCounter(
    std::map<std::string, int> counters) {
  std::vector<vivaldi::content_blocking::BlockedCounter> result;
  for (const auto& counter : counters) {
    result.emplace_back();
    result.back().domain = counter.first;
    result.back().blocked_count = counter.second;
  }

  return result;
}

ExtensionFunction::ResponseValue
ContentBlockingGetBlockedCountersFunction::RunWithService(
    adblock_filter::RuleService* rules_service) {
  namespace Results = vivaldi::content_blocking::GetBlockedCounters::Results;
  const auto* reporter = rules_service->GetStateAndLogs();
  Results::Counters counters;
  counters.blocked_domains.tracking = ToVivaldiBlockedCounter(
      reporter->GetBlockedDomainCounters()[static_cast<size_t>(
          adblock_filter::RuleGroup::kTrackingRules)]);
  counters.blocked_domains.ad_blocking = ToVivaldiBlockedCounter(
      reporter->GetBlockedDomainCounters()[static_cast<size_t>(
          adblock_filter::RuleGroup::kAdBlockingRules)]);
  counters.blocked_for_origin.tracking = ToVivaldiBlockedCounter(
      reporter->GetBlockedForOriginCounters()[static_cast<size_t>(
          adblock_filter::RuleGroup::kTrackingRules)]);
  counters.blocked_for_origin.ad_blocking = ToVivaldiBlockedCounter(
      reporter->GetBlockedForOriginCounters()[static_cast<size_t>(
          adblock_filter::RuleGroup::kAdBlockingRules)]);
  return ArgumentList(Results::Create(
      reporter->GetBlockedCountersStart().InMillisecondsFSinceUnixEpoch(),
      counters));
}

ExtensionFunction::ResponseValue
ContentBlockingClearBlockedCountersFunction::RunWithService(
    adblock_filter::RuleService* rules_service) {
  rules_service->GetStateAndLogs()->ClearBlockedCounters();
  return NoArguments();
}

ExtensionFunction::ResponseValue
ContentBlockingIsExemptOfFilteringFunction::RunWithService(
    adblock_filter::RuleService* rules_service) {
  using vivaldi::content_blocking::IsExemptOfFiltering::Params;
  namespace Results = vivaldi::content_blocking::IsExemptOfFiltering::Results;
  std::optional<Params> params(Params::Create(args()));

  return ArgumentList(
      Results::Create(rules_service->GetRuleManager()->IsExemptOfFiltering(
          FromVivaldiContentBlockingRuleGroup(params->rule_group).value(),
          url::Origin::Create(GURL(params->url)))));
}

ExtensionFunction::ResponseValue
ContentBlockingIsExemptByPartnerURLFunction::RunWithService(
    adblock_filter::RuleService* rules_service) {
  using vivaldi::content_blocking::IsExemptByPartnerURL::Params;
  namespace Results = vivaldi::content_blocking::IsExemptByPartnerURL::Results;
  std::optional<Params> params(Params::Create(args()));

  GURL url{params->url};
  int tab_id{params->tab_id};
  vivaldi::content_blocking::URLPartnerInfo url_partner_info;
  url_partner_info.status = false;

  content::WebContents* web_contents;
  if (ExtensionTabUtil::GetTabById(tab_id, browser_context(), true,
                                     &web_contents) &&
      web_contents) {
    uint32_t rule_source_id = adblock_filter::RuleSourceCore::FromUrl(GURL{kPartnerListURL})->id();

    url_partner_info.status =
        rules_service->HasDocumentActivationForRuleSource(
              adblock_filter::RuleGroup::kAdBlockingRules, web_contents, rule_source_id);
  }

  // detect template name based on url
  auto *templateURLService = TemplateURLServiceFactory::GetForProfile(Profile::FromBrowserContext(browser_context()));
  TemplateURL* templateURL =
    templateURLService->GetTemplateURLForHost(url.host());

  if (templateURL) url_partner_info.name = base::UTF16ToUTF8(templateURL->short_name());

  return ArgumentList(Results::Create(url_partner_info));
}

ExtensionFunction::ResponseValue
ContentBlockingGetAdAttributionDomainFunction::RunWithService(
    adblock_filter::RuleService* rules_service) {
  using vivaldi::content_blocking::GetAdAttributionDomain::Params;
  namespace Results =
      vivaldi::content_blocking::GetAdAttributionDomain::Results;
  std::optional<Params> params(Params::Create(args()));
  std::vector<vivaldi::content_blocking::AdAttributionDomain>
      tab_attribution_domains;

  for (auto tab_id : params->tab_ids) {
    content::WebContents* web_contents;
    if (!ExtensionTabUtil::GetTabById(tab_id, browser_context(), true,
                                      &web_contents) ||
        !web_contents) {
      continue;
    }
    adblock_filter::TabStateAndLogs* tab_state_and_logs =
        rules_service->GetStateAndLogs()->GetTabHelper(web_contents);
    if (!tab_state_and_logs) {
      continue;
    }
    vivaldi::content_blocking::AdAttributionDomain attribution_for_tab;
    attribution_for_tab.tab_id = tab_id;
    attribution_for_tab.domain =
        tab_state_and_logs->GetCurrentAdLandingDomain();
    attribution_for_tab.active = tab_state_and_logs->IsOnAdLandingSite();
    tab_attribution_domains.push_back(std::move(attribution_for_tab));
  }
  return ArgumentList(Results::Create(tab_attribution_domains));
}
ExtensionFunction::ResponseValue
ContentBlockingGetAdAttributionAllowedTrackersFunction::RunWithService(
    adblock_filter::RuleService* rules_service) {
  using vivaldi::content_blocking::GetAdAttributionAllowedTrackers::Params;
  namespace Results =
      vivaldi::content_blocking::GetAdAttributionAllowedTrackers::Results;
  std::optional<Params> params(Params::Create(args()));
  std::vector<vivaldi::content_blocking::AllowedAdAttributionTrackers>
      allowed_ad_attribution_trackers;

  for (auto tab_id : params->tab_ids) {
    content::WebContents* web_contents;
    if (!ExtensionTabUtil::GetTabById(tab_id, browser_context(), true,
                                      &web_contents) ||
        !web_contents) {
      continue;
    }
    adblock_filter::TabStateAndLogs* tab_state_and_logs =
        rules_service->GetStateAndLogs()->GetTabHelper(web_contents);

    if (!tab_state_and_logs) {
      continue;
    }
    vivaldi::content_blocking::AllowedAdAttributionTrackers
        allowed_ad_attribution_trackers_for_tab;
    allowed_ad_attribution_trackers_for_tab.tab_id = tab_id;
    allowed_ad_attribution_trackers_for_tab.tracker_urls =
        std::vector<std::string>(
            tab_state_and_logs->GetAllowedAttributionTrackers().begin(),
            tab_state_and_logs->GetAllowedAttributionTrackers().end());
    allowed_ad_attribution_trackers.push_back(
        std::move(allowed_ad_attribution_trackers_for_tab));
  }

  return ArgumentList(Results::Create(allowed_ad_attribution_trackers));
}
}  // namespace extensions
