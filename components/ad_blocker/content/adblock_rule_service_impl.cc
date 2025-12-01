// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/content/adblock_rule_service_impl.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/ad_blocker/content/adblock_cosmetic_filter.h"
#include "components/ad_blocker/content/adblock_request_filter.h"
#include "components/ad_blocker/content/adblock_rules_index.h"
#include "components/ad_blocker/content/adblock_rules_index_manager.h"
#include "components/ad_blocker/core/adblock_rule_manager_impl.h"
#include "components/ad_blocker/core/adblock_rule_source_handler.h"
#include "components/ad_blocker/core/adblock_stats_store_impl.h"
#include "components/ad_blocker/public/content/adblock_tab_state_and_logs.h"
#include "components/ad_blocker/public/core/adblock_known_sources_handler.h"
#include "components/ad_blocker/public/core/adblock_stats_data.h"
#include "components/ad_blocker/public/core/adblock_types.h"
#include "components/prefs/pref_service.h"
#include "components/request_filter/request_filter_registry.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"

#include "vivaldi/prefs/vivaldi_gen_prefs.h"

namespace adblock_filter {
RuleServiceImpl::RuleServiceImpl(
    content::BrowserContext* context,
    PrefService* prefs,
    vivaldi::RequestFilterRegistry* request_filter_registry,
    RuleSourceHandler::RulesCompiler rules_compiler,
    std::string locale)
    : context_(context),
      prefs_(prefs),
      request_filter_registry_(request_filter_registry),
      rules_compiler_(std::move(rules_compiler)),
      locale_(std::move(locale)) {
  pref_change_registrar_.Init(prefs_);
}

RuleServiceImpl::~RuleServiceImpl() {}

void RuleServiceImpl::AddObserver(RuleService::Observer* observer) {
  observers_.AddObserver(observer);
}

void RuleServiceImpl::RemoveObserver(RuleService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void RuleServiceImpl::Load() {
  CHECK(!is_loaded_ && !state_store_);
  file_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  resources_.emplace(file_task_runner_.get());
  state_and_logs_.emplace(this);

  for (auto group : {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
    auto request_filter = std::make_unique<AdBlockRequestFilter>(
        weak_factory_.GetWeakPtr(), group);
    request_filter->set_allow_blocking_documents(prefs_->GetBoolean(
        vivaldiprefs::kPrivacyAdBlockerEnableDocumentBlocking));
    if (group == RuleGroup::kAdBlockingRules) {
      request_filter->set_block_pings(
          prefs_->GetBoolean(vivaldiprefs::kPrivacyBlockPingsEnabled));
    }
    request_filters_[static_cast<size_t>(group)] = request_filter.get();
    request_filter_registry_->AddFilter(std::move(request_filter));
  }

  pref_change_registrar_.Add(
      vivaldiprefs::kPrivacyAdBlockerEnableDocumentBlocking,
      base::BindRepeating(&RuleServiceImpl::OnEnableDocumentBlockingChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      vivaldiprefs::kPrivacyBlockPingsEnabled,
      base::BindRepeating(&RuleServiceImpl::OnPingBlockingChanged,
                          base::Unretained(this)));

  state_store_.emplace(context_->GetPath(), this, file_task_runner_);

  stats_store_.emplace(context_->GetPath());
  // Unretained is safe because we own the sources store
  state_store_->Load();
}

RulesIndex* RuleServiceImpl::GetRuleIndex(RuleGroup group) {
  if (!is_loaded_) {
    return nullptr;
  }

  return index_managers_[static_cast<size_t>(group)]->rules_index();
}

StateAndLogsImpl& RuleServiceImpl::GetStateAndLogsImpl() {
  CHECK(state_and_logs_);
  return *state_and_logs_;
}

Resources& RuleServiceImpl::GetResources() {
  CHECK(resources_);
  return *resources_;
}

bool RuleServiceImpl::IsLoaded() const {
  return is_loaded_;
}

void RuleServiceImpl::Shutdown() {
  if (is_loaded_) {
    state_store_->OnRuleServiceShutdown();
    rule_manager_->RemoveObserver(this);
  }
}

std::string RuleServiceImpl::GetRulesIndexChecksum(RuleGroup group) {
  CHECK(is_loaded_);
  CHECK(index_managers_[static_cast<size_t>(group)]);
  return index_managers_[static_cast<size_t>(group)]->index_checksum();
}

// All cases of base::Unretained in this method are safe. We are generally
// passing callbacks to objects that we own, calling to either this or other
// objects that we own.
void RuleServiceImpl::OnStorageDoneLoading(
    RuleServiceStorageDelegate::LoadResult load_result) {
  MigrateOldStatsData(&load_result);

  rule_manager_.emplace(
      file_task_runner_, context_->GetPath(),
      context_->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      std::move(load_result.rule_sources),
      std::move(load_result.active_exceptions_lists),
      std::move(load_result.exceptions),
      base::BindRepeating(&RuleServiceStorage::ScheduleSave,
                          base::Unretained(&state_store_.value())),
      rules_compiler_,
      base::BindRepeating(&StateAndLogsImpl::OnTrackerInfosUpdated,
                          base::Unretained(&state_and_logs_.value())));
  rule_manager_->AddObserver(this);

  for (auto group : {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
    index_managers_[static_cast<size_t>(group)].emplace(
        context_, &rule_manager_.value(), group,
        load_result.index_checksums[static_cast<size_t>(group)],
        base::BindRepeating(&RuleServiceImpl::OnRulesIndexChanged,
                            base::Unretained(this), group),
        base::BindRepeating(
            &AdBlockRequestFilter::OnIndexLoaded,
            base::Unretained(request_filters_[static_cast<size_t>(group)])),
        base::BindRepeating(&RuleManager::OnCompiledRulesReadFailCallback,
                            base::Unretained(&rule_manager_.value())),
        file_task_runner_);
  }

  content_injection_provider_.emplace(context_, this, &(resources_.value()));

  known_sources_handler_.emplace(
      &rule_manager_.value(), load_result.storage_version, locale_,
      load_result.known_sources, std::move(load_result.deleted_presets),
      base::BindRepeating(&RuleServiceStorage::ScheduleSave,
                          base::Unretained(&state_store_.value())));

  is_loaded_ = true;

  for (RuleService::Observer& observer : observers_)
    observer.OnRuleServiceStateLoaded(this);
}

void RuleServiceImpl::MigrateOldStatsData(
    const RuleServiceStorageDelegate::LoadResult* load_result) {
  StatsData data;

  const auto add_entries_from_counter_group =
      [&data](const auto& counters, StatsData::EntryType type) {
        const std::map<std::string, int> tracker_map =
            counters[static_cast<int>(RuleGroup::kTrackingRules)];
        const std::map<std::string, int> ad_map =
            counters[static_cast<int>(RuleGroup::kAdBlockingRules)];

        for (const auto& [domain, tracker_count] : tracker_map) {
          const int ad_count = ad_map.contains(domain) ? ad_map.at(domain) : 0;
          StatsData::Entry entry{domain, ad_count, tracker_count};
          data.AddEntry(entry, type);
        }

        for (const auto& [domain, ad_count] : ad_map) {
          if (!tracker_map.contains(domain)) {
            StatsData::Entry entry{domain, ad_count, 0};
            data.AddEntry(entry, type);
          }
        }
      };

  add_entries_from_counter_group(load_result->blocked_domains_counters,
                                 StatsData::EntryType::TRACKER_AND_ADS);
  add_entries_from_counter_group(load_result->blocked_for_origin_counters,
                                 StatsData::EntryType::WEBSITE);

  if (data.TotalAdsBlocked() > 0 || data.TotalTrackersBlocked() > 0) {
    // This function is run on every startup, but we're migrating only when we
    // have some values.
    data.SetReportingStart(load_result->blocked_reporting_start);
    stats_store_->ImportData(std::move(data));
  }
}


RuleManager* RuleServiceImpl::GetRuleManager() {
  CHECK(is_loaded_);
  CHECK(rule_manager_);
  return &rule_manager_.value();
}

KnownRuleSourcesHandler* RuleServiceImpl::GetKnownSourcesHandler() {
  CHECK(is_loaded_);
  CHECK(known_sources_handler_);
  return &known_sources_handler_.value();
}

StateAndLogs* RuleServiceImpl::GetStateAndLogs() {
  CHECK(state_and_logs_);
  return &state_and_logs_.value();
}

StatsStore* RuleServiceImpl::GetStatsStore() {
  CHECK(stats_store_);
  return &stats_store_.value();
}

void RuleServiceImpl::OnExceptionListChanged(RuleGroup group,
                                             RuleManager::ExceptionsList list) {
  request_filter_registry_->ClearCacheOnNavigation();
}

void RuleServiceImpl::OnRulesIndexChanged(RuleGroup group) {
  // The state store will read all checksums when saving. No need to worry about
  // which has changed.
  state_store_->ScheduleSave();
}

void RuleServiceImpl::OnEnableDocumentBlockingChanged() {
  for (auto group : {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
    request_filters_[static_cast<size_t>(group)]->set_allow_blocking_documents(
        prefs_->GetBoolean(
            vivaldiprefs::kPrivacyAdBlockerEnableDocumentBlocking));
  }
}

void RuleServiceImpl::OnPingBlockingChanged() {
  request_filters_[static_cast<size_t>(RuleGroup::kAdBlockingRules)]
      ->set_block_pings(
          prefs_->GetBoolean(vivaldiprefs::kPrivacyBlockPingsEnabled));
}

std::unique_ptr<mojom::CosmeticFilter> RuleServiceImpl::MakeCosmeticFilter(
    content::RenderFrameHost* frame) {
  return std::make_unique<CosmeticFilter>(weak_factory_.GetWeakPtr(),
                                          frame->GetProcess()->GetID(),
                                          frame->GetRoutingID());
}

bool RuleServiceImpl::HasDocumentActivationForRuleSource(
    adblock_filter::RuleGroup group,
    content::WebContents* web_contents,
    base::Uuid preset_id) {
  auto* tab_helper = GetStateAndLogs()->GetTabHelper(web_contents);

  // Tab helper can be null when page is still loading.
  if (!tab_helper)
    return false;

  auto activations = tab_helper->GetTabActivations(group);
  auto rule_activation = activations.by_type.find(
      adblock_filter::RequestFilterRule::kWholeDocument);
  if (rule_activation != activations.by_type.end()) {
    auto& rule_data = rule_activation->second.rule_data;
    if (rule_data) {
      if (known_sources_handler_->GetPresetIdForSourceId(
              group, rule_data->rule_source_id) == preset_id)
        return true;
    }
  }

  return false;
}

}  // namespace adblock_filter
