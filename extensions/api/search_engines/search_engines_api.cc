// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/search_engines/search_engines_api.h"

#include <bitset>

#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/ad_blocker/adblock_rule_manager.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_content.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_factory.h"
#include "components/search_engines/template_url_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_set.h"
#include "extensions/schema/search_engines.h"
#include "extensions/tools/vivaldi_tools.h"

namespace extensions {

namespace {
const char kSearchTermsParameterFull[] = "{searchTerms}";
const char kGoogleUnescapedSearchTermsParameterFull[] =
    "{google:unescapedSearchTerms}";
const char kTemplateServiceNotAvailable[] =
    "TemplateURLService not available for profile.";
// Display value for kSearchTermsParameter.
const char kDisplaySearchTerms[] = "%s";

// Display value for kGoogleUnescapedSearchTermsParameter.
const char kDisplayUnescapedSearchTerms[] = "%S";

std::string ToDisplay(const std::string& turl_param) {
  std::string result(turl_param);
  base::ReplaceSubstringsAfterOffset(&result, 0, kSearchTermsParameterFull,
                                     kDisplaySearchTerms);
  base::ReplaceSubstringsAfterOffset(&result, 0,
                                     kGoogleUnescapedSearchTermsParameterFull,
                                     kDisplayUnescapedSearchTerms);
  return result;
}

std::string FromDisplay(const std::string& display_string) {
  std::string result(display_string);
  base::ReplaceSubstringsAfterOffset(&result, 0, kDisplaySearchTerms,
                                     kSearchTermsParameterFull);
  base::ReplaceSubstringsAfterOffset(&result, 0, kDisplayUnescapedSearchTerms,
                                     kGoogleUnescapedSearchTermsParameterFull);
  return result;
}

void AddTemplateURLToResult(
    const TemplateURL* template_url,
    bool force_read_only,
    std::vector<vivaldi::search_engines::TemplateURL>& result) {
  // We currently don't support these at all. Pretend they don't exist.
  if (template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION)
    return;

  vivaldi::search_engines::TemplateURL result_turl;
  result_turl.read_only =
      force_read_only ||
      template_url->type() == TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION;
  if (template_url->type() == TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION)
    result_turl.extension_id = template_url->GetExtensionId();
  result_turl.id = base::NumberToString(template_url->id());
  result_turl.name = base::UTF16ToUTF8(template_url->short_name());
  result_turl.keyword = base::UTF16ToUTF8(template_url->keyword());
  result_turl.favicon_url = template_url->favicon_url().spec();
  result_turl.url = ToDisplay(template_url->url());
  result_turl.post_params = ToDisplay(template_url->search_url_post_params());
  result_turl.suggest_url = ToDisplay(template_url->suggestions_url());
  result_turl.suggest_post_params =
      ToDisplay(template_url->suggestions_url_post_params());
  result_turl.image_url = ToDisplay(template_url->image_url());
  result_turl.image_post_params =
      ToDisplay(template_url->image_url_post_params());
  result_turl.prepopulate_id = template_url->prepopulate_id();

  result.push_back(std::move(result_turl));
}

// Finds and returns the template URL which has the provided |id|. The returned
// template URL is owned by |service| and must not be deleted. It can be
// invalidated by passing it back in some calls to |service| (i.e. Remove, at
// the time of this writing)
TemplateURL* GetTemplateURLById(TemplateURLService* service, TemplateURLID id) {
  TemplateURLService::TemplateURLVector template_urls =
      service->GetTemplateURLs();
  const auto template_url_iter = std::find_if(
      template_urls.begin(), template_urls.end(),
      [id](const auto* template_url) { return template_url->id() == id; });
  if (template_url_iter == template_urls.end())
    return nullptr;
  return *template_url_iter;
}

vivaldi::search_engines::SearchRequest BuildSearchRequest(
    content::BrowserContext* browser_context,
    const TemplateURLRef& template_url_ref,
    const SearchTermsData& search_terms_data,
    std::string search_terms) {
  TemplateURLRef::SearchTermsArgs search_terms_args(
      base::UTF8ToUTF16(search_terms));
  TemplateURLRef::PostContent post_content;

  vivaldi::search_engines::SearchRequest result;

  std::bitset<3> ad_blocking_state;
  auto* adblock_rules_manager =
      adblock_filter::RuleServiceFactory::GetForBrowserContext(browser_context)
          ->GetRuleManager();
  if (adblock_rules_manager->GetActiveExceptionList(
          adblock_filter::RuleGroup::kTrackingRules) ==
      adblock_filter::RuleManager::kExemptList)
    ad_blocking_state.set(0);
  if (adblock_rules_manager->GetActiveExceptionList(
          adblock_filter::RuleGroup::kAdBlockingRules) ==
      adblock_filter::RuleManager::kExemptList)
    ad_blocking_state.set(1);

  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(browser_context);
  DCHECK(extension_registry);
  for (const auto extension : extension_registry->enabled_extensions()) {
    if (extension->is_extension() &&
        extension->location() !=
            extensions::mojom::ManifestLocation::kComponent) {
      ad_blocking_state.set(2);
      break;
    }
  }

  search_terms_args.vivaldi_ad_blocking_state = ad_blocking_state;

  result.url = template_url_ref.ReplaceSearchTerms(
      search_terms_args, search_terms_data, &post_content);
  result.content_type = post_content.first;
  result.post_params = post_content.second;

  return result;
}

bool IsCreatedByExtension(const TemplateURL* template_url) {
  return template_url->type() == TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION ||
         template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION;
}
}  // namespace

SearchEnginesAPI::SearchEnginesAPI(content::BrowserContext* context)
    : browser_context_(context),
      service_(TemplateURLServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context_))) {
  if (service_) {
    service_->AddObserver(this);
  }
}

// static
BrowserContextKeyedAPIFactory<SearchEnginesAPI>*
SearchEnginesAPI::GetFactoryInstance() {
  static base::NoDestructor<BrowserContextKeyedAPIFactory<SearchEnginesAPI>>
      instance;
  return instance.get();
}

SearchEnginesAPI::~SearchEnginesAPI() {}

// KeyedService implementation.
void SearchEnginesAPI::Shutdown() {
  if (service_) {
    service_->RemoveObserver(this);
  }
}

void SearchEnginesAPI::OnTemplateURLServiceChanged() {
  ::vivaldi::BroadcastEvent(
      vivaldi::search_engines::OnTemplateUrlsChanged::kEventName,
      vivaldi::search_engines::OnTemplateUrlsChanged::Create(),
      browser_context_);
}

void SearchEnginesAPI::OnTemplateURLServiceShuttingDown() {
  if (service_) {
    service_->RemoveObserver(this);
  }
  service_ = nullptr;
}

ExtensionFunction::ResponseAction SearchEnginesGetKeywordForUrlFunction::Run() {
  absl::optional<vivaldi::search_engines::GetKeywordForUrl::Params> params(
      vivaldi::search_engines::GetKeywordForUrl::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  return RespondNow(
      ArgumentList(vivaldi::search_engines::GetKeywordForUrl::Results::Create(
          base::UTF16ToUTF8(TemplateURL::GenerateKeyword(GURL(params->url))))));
}

ExtensionFunction::ResponseAction SearchEnginesGetTemplateUrlsFunction::Run() {
  auto* service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));
  if (!service) {
    return RespondNow(Error(kTemplateServiceNotAvailable));
  }
  vivaldi::search_engines::AllTemplateURLs result;
  if (service->loaded()) {
    TemplateURLService::TemplateURLVector template_urls =
        service->GetTemplateURLs();

    // Stable sort preserves the order of search engines added by extensions,
    // which have no position.
    std::stable_sort(template_urls.begin(), template_urls.end(),
                     [](const auto* template_url1, const auto* template_url2) {
                       // Important to have the false-returning test first, so
                       // that if both are invalid, they'll be evaluated as
                       // equal.
                       if (!template_url1->data().vivaldi_position.IsValid())
                         return false;
                       if (!template_url2->data().vivaldi_position.IsValid())
                         return true;
                       return template_url1->data().vivaldi_position.LessThan(
                           template_url2->data().vivaldi_position);
                     });
    for (const auto* template_url : template_urls) {
      // We abuse is_active to hide "removed" prepopulated searches.
      if (template_url->is_active() != TemplateURLData::ActiveStatus::kFalse)
        AddTemplateURLToResult(template_url, false, result.template_urls);
    }
  }

  const TemplateURL* default_search;
  default_search =
      service->GetDefaultSearchProvider(TemplateURLService::kDefaultSearchMain);
  if (default_search) {
    if (!service->loaded())
      AddTemplateURLToResult(default_search, true, result.template_urls);
    result.default_search = base::NumberToString(default_search->id());
  }

  default_search = service->GetDefaultSearchProvider(
      TemplateURLService::kDefaultSearchPrivate);
  if (default_search) {
    if (!service->loaded())
      AddTemplateURLToResult(default_search, true, result.template_urls);
    result.default_private = base::NumberToString(default_search->id());
  }

  default_search = service->GetDefaultSearchProvider(
      TemplateURLService::kDefaultSearchField);
  if (default_search) {
    if (!service->loaded())
      AddTemplateURLToResult(default_search, true, result.template_urls);
    result.default_search_field = base::NumberToString(default_search->id());
  }

  default_search = service->GetDefaultSearchProvider(
      TemplateURLService::kDefaultSearchFieldPrivate);
  if (default_search) {
    if (!service->loaded())
      AddTemplateURLToResult(default_search, true, result.template_urls);
    result.default_search_field_private =
        base::NumberToString(default_search->id());
  }

  default_search = service->GetDefaultSearchProvider(
      TemplateURLService::kDefaultSearchSpeeddials);
  if (default_search) {
    if (!service->loaded())
      AddTemplateURLToResult(default_search, true, result.template_urls);
    result.default_speeddials = base::NumberToString(default_search->id());
  }

  default_search = service->GetDefaultSearchProvider(
      TemplateURLService::kDefaultSearchSpeeddialsPrivate);
  if (default_search) {
    if (!service->loaded())
      AddTemplateURLToResult(default_search, true, result.template_urls);
    result.default_speeddials_private =
        base::NumberToString(default_search->id());
  }

  default_search = service->GetDefaultSearchProvider(
      TemplateURLService::kDefaultSearchImage);
  if (default_search) {
    if (!service->loaded())
      AddTemplateURLToResult(default_search, true, result.template_urls);
    result.default_image = base::NumberToString(default_search->id());
  }

  return RespondNow(ArgumentList(
      vivaldi::search_engines::GetTemplateUrls::Results::Create(result)));
}

ExtensionFunction::ResponseAction SearchEnginesAddTemplateUrlFunction::Run() {
  absl::optional<vivaldi::search_engines::AddTemplateUrl::Params> params(
      vivaldi::search_engines::AddTemplateUrl::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  auto* service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));
  if (!service) {
    return RespondNow(Error(kTemplateServiceNotAvailable));
  }

  if (params->template_url.keyword.empty() || params->template_url.url.empty())
    return RespondNow(ArgumentList(
        vivaldi::search_engines::AddTemplateUrl::Results::Create("")));

  TemplateURLData data;
  data.SetShortName(base::UTF8ToUTF16(params->template_url.name));
  data.SetKeyword(base::UTF8ToUTF16(params->template_url.keyword));
  data.SetURL(FromDisplay(params->template_url.url));
  data.suggestions_url = FromDisplay(params->template_url.suggest_url);
  data.image_url = FromDisplay(params->template_url.image_url);
  data.search_url_post_params = FromDisplay(params->template_url.post_params);
  data.suggestions_url_post_params =
      FromDisplay(params->template_url.suggest_post_params);
  data.image_url_post_params =
      FromDisplay(params->template_url.image_post_params);
  data.favicon_url = GURL(params->template_url.favicon_url);
  data.safe_for_autoreplace = false;

  TemplateURL* template_url = service->Add(std::make_unique<TemplateURL>(data));

  return RespondNow(
      ArgumentList(vivaldi::search_engines::AddTemplateUrl::Results::Create(
          template_url ? base::NumberToString(template_url->id()) : "")));
}

ExtensionFunction::ResponseAction
SearchEnginesRemoveTemplateUrlFunction::Run() {
  absl::optional<vivaldi::search_engines::RemoveTemplateUrl::Params> params(
      vivaldi::search_engines::RemoveTemplateUrl::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  auto* service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));
  if (!service) {
    return RespondNow(Error(kTemplateServiceNotAvailable));
  }

  TemplateURLID id;
  if (!base::StringToInt64(params->id, &id))
    return RespondNow(Error("id parameter invalid"));

  TemplateURL* turl_to_remove = GetTemplateURLById(service, id);

  if (!turl_to_remove)
    return RespondNow(ArgumentList(
        vivaldi::search_engines::RemoveTemplateUrl::Results::Create(false)));

  for (int i = 0; i < TemplateURLService::kDefaultSearchTypeCount; i++) {
    const TemplateURL* default_turl = service->GetDefaultSearchProvider(
        TemplateURLService::DefaultSearchType(i));
    if (default_turl && turl_to_remove->id() == default_turl->id())
      switch (i) {
        case TemplateURLService::kDefaultSearchMain:
        case TemplateURLService::kDefaultSearchPrivate:
        case TemplateURLService::kDefaultSearchImage:
          return RespondNow(ArgumentList(
              vivaldi::search_engines::RemoveTemplateUrl::Results::Create(
                  false)));
        case TemplateURLService::kDefaultSearchField:
        case TemplateURLService::kDefaultSearchSpeeddials:
          service->SetUserSelectedDefaultSearchProvider(
              const_cast<TemplateURL*>(service->GetDefaultSearchProvider(
                  TemplateURLService::kDefaultSearchMain)),
              TemplateURLService::DefaultSearchType(i));
          break;
        case TemplateURLService::kDefaultSearchFieldPrivate:
        case TemplateURLService::kDefaultSearchSpeeddialsPrivate:
          service->SetUserSelectedDefaultSearchProvider(
              const_cast<TemplateURL*>(service->GetDefaultSearchProvider(
                  TemplateURLService::kDefaultSearchPrivate)),
              TemplateURLService::DefaultSearchType(i));
          break;
      }
  }

  if (turl_to_remove->prepopulate_id() != 0) {
    // Instead of removing prepopulated turls and then needing to add support to
    // chromium for keeping track of which one was removed and preventing it to
    // be readded either by sync or when an update to prepopulate data comes up,
    // we mis-use the is_active flag (which we don't really use otherwise) to
    // signify that they shouldn't show up.
    service->SetIsActiveTemplateURL(turl_to_remove, false);
  } else {
    // This will CHECK if we are not allowed to remove the search engine.
    service->Remove(turl_to_remove);
  }

  return RespondNow(ArgumentList(
      vivaldi::search_engines::RemoveTemplateUrl::Results::Create(true)));
}

ExtensionFunction::ResponseAction
SearchEnginesUpdateTemplateUrlFunction::Run() {
  absl::optional<vivaldi::search_engines::UpdateTemplateUrl::Params> params(
      vivaldi::search_engines::UpdateTemplateUrl::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  auto* service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));
  if (!service) {
    return RespondNow(Error(kTemplateServiceNotAvailable));
  }

  TemplateURLID id;
  if (!base::StringToInt64(params->template_url.id, &id))
    return RespondNow(Error("id parameter invalid"));

  TemplateURL* turl_to_update = GetTemplateURLById(service, id);

  if (!turl_to_update || IsCreatedByExtension(turl_to_update) ||
      params->template_url.keyword.empty() || params->template_url.url.empty())
    return RespondNow(ArgumentList(
        vivaldi::search_engines::UpdateTemplateUrl::Results::Create(false)));

  service->ResetTemplateURL(
      turl_to_update, base::UTF8ToUTF16(params->template_url.name),
      base::UTF8ToUTF16(params->template_url.keyword),
      FromDisplay(params->template_url.url),
      FromDisplay(params->template_url.post_params),
      FromDisplay(params->template_url.suggest_url),
      FromDisplay(params->template_url.suggest_post_params),
      FromDisplay(params->template_url.image_url),
      FromDisplay(params->template_url.image_post_params),
      GURL(params->template_url.favicon_url));
  return RespondNow(ArgumentList(
      vivaldi::search_engines::UpdateTemplateUrl::Results::Create(true)));
}

ExtensionFunction::ResponseAction SearchEnginesMoveTemplateUrlFunction::Run() {
  absl::optional<vivaldi::search_engines::MoveTemplateUrl::Params> params(
      vivaldi::search_engines::MoveTemplateUrl::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  auto* service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));
  if (!service) {
    return RespondNow(Error(kTemplateServiceNotAvailable));
  }

  TemplateURLID id;
  if (!base::StringToInt64(params->id, &id))
    return RespondNow(Error("id parameter invalid"));

  TemplateURL* turl_to_move = GetTemplateURLById(service, id);

  if (!turl_to_move || IsCreatedByExtension(turl_to_move))
    return RespondNow(ArgumentList(
        vivaldi::search_engines::MoveTemplateUrl::Results::Create(false)));

  TemplateURL* successor = nullptr;
  if (params->successor_id) {
    TemplateURLID successor_id;
    if (!base::StringToInt64(*params->successor_id, &successor_id))
      return RespondNow(Error("successor_id parameter invalid"));
    if (successor_id == id)
      return RespondNow(ArgumentList(
          vivaldi::search_engines::MoveTemplateUrl::Results::Create(false)));
    successor = GetTemplateURLById(service, successor_id);
  }

  service->VivaldiMoveTemplateURL(turl_to_move, successor);

  return RespondNow(ArgumentList(
      vivaldi::search_engines::MoveTemplateUrl::Results::Create(true)));
}

ExtensionFunction::ResponseAction SearchEnginesSetDefaultFunction::Run() {
  absl::optional<vivaldi::search_engines::SetDefault::Params> params(
      vivaldi::search_engines::SetDefault::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  auto* service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));
  if (!service) {
    return RespondNow(Error(kTemplateServiceNotAvailable));
  }

  TemplateURLID id;
  if (!base::StringToInt64(params->id, &id))
    return RespondNow(Error("id parameter invalid"));

  TemplateURL* new_default = GetTemplateURLById(service, id);

  if (!new_default || IsCreatedByExtension(new_default))
    return RespondNow(ArgumentList(
        vivaldi::search_engines::SetDefault::Results::Create(false)));

  auto default_type = [](vivaldi::search_engines::DefaultType default_type)
      -> absl::optional<TemplateURLService::DefaultSearchType> {
    switch (default_type) {
      case vivaldi::search_engines::DEFAULT_TYPE_DEFAULTSEARCH:
        return TemplateURLService::kDefaultSearchMain;
      case vivaldi::search_engines::DEFAULT_TYPE_DEFAULTPRIVATE:
        return TemplateURLService::kDefaultSearchPrivate;
      case vivaldi::search_engines::DEFAULT_TYPE_DEFAULTSEARCHFIELD:
        return TemplateURLService::kDefaultSearchField;
      case vivaldi::search_engines::DEFAULT_TYPE_DEFAULTSEARCHFIELDPRIVATE:
        return TemplateURLService::kDefaultSearchFieldPrivate;
      case vivaldi::search_engines::DEFAULT_TYPE_DEFAULTSPEEDDIALS:
        return TemplateURLService::kDefaultSearchSpeeddials;
      case vivaldi::search_engines::DEFAULT_TYPE_DEFAULTSPEEDDIALSPRIVATE:
        return TemplateURLService::kDefaultSearchSpeeddialsPrivate;
      case vivaldi::search_engines::DEFAULT_TYPE_DEFAULTIMAGE:
        return TemplateURLService::kDefaultSearchImage;
      default:
        NOTREACHED();
        return absl::nullopt;
    }
  }(params->default_type);

  service->SetUserSelectedDefaultSearchProvider(new_default,
                                                default_type.value());
  return RespondNow(
      ArgumentList(vivaldi::search_engines::SetDefault::Results::Create(true)));
}

ExtensionFunction::ResponseAction SearchEnginesGetSearchRequestFunction::Run() {
  absl::optional<vivaldi::search_engines::GetSearchRequest::Params> params(
      vivaldi::search_engines::GetSearchRequest::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  auto* service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));
  if (!service) {
    return RespondNow(Error(kTemplateServiceNotAvailable));
  }

  TemplateURLID id;
  if (!base::StringToInt64(params->id, &id))
    return RespondNow(Error("id parameter invalid"));

  TemplateURL* template_url = GetTemplateURLById(service, id);

  if (!template_url)
    return RespondNow(Error("No search engine with this id"));

  return RespondNow(
      ArgumentList(vivaldi::search_engines::GetSearchRequest::Results::Create(
          BuildSearchRequest(browser_context(), template_url->url_ref(),
                             service->search_terms_data(),
                             params->search_terms))));
}

ExtensionFunction::ResponseAction
SearchEnginesGetSuggestRequestFunction::Run() {
  absl::optional<vivaldi::search_engines::GetSuggestRequest::Params> params(
      vivaldi::search_engines::GetSuggestRequest::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  auto* service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));
  if (!service) {
    return RespondNow(Error(kTemplateServiceNotAvailable));
  }

  TemplateURLID id;
  if (!base::StringToInt64(params->id, &id))
    return RespondNow(Error("id parameter invalid"));

  TemplateURL* template_url = GetTemplateURLById(service, id);

  if (!template_url)
    return RespondNow(Error("No search engine with this id"));

  return RespondNow(
      ArgumentList(vivaldi::search_engines::GetSuggestRequest::Results::Create(
          BuildSearchRequest(
              browser_context(), template_url->suggestions_url_ref(),
              service->search_terms_data(), params->search_terms))));
}

ExtensionFunction::ResponseAction
SearchEnginesRepairPrepopulatedTemplateUrlsFunction::Run() {
  absl::optional<
      vivaldi::search_engines::RepairPrepopulatedTemplateUrls::Params>
      params(vivaldi::search_engines::RepairPrepopulatedTemplateUrls::Params::
                 Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  auto* service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));
  if (!service) {
    return RespondNow(Error(kTemplateServiceNotAvailable));
  }
  service->RepairPrepopulatedSearchEngines();

  if (!params->only_keep_prepopulated)
    return RespondNow(NoArguments());

  // After repair, all defaults have been set back to prepopulated engines, so
  // it's safe to remove anything that's not prepopulated;
  TemplateURLService::TemplateURLVector template_urls =
      service->GetTemplateURLs();
  for (const auto* template_url : template_urls) {
    if (template_url->prepopulate_id() == 0)
      service->Remove(template_url);
  }

  return RespondNow(NoArguments());
}
}  // namespace extensions
