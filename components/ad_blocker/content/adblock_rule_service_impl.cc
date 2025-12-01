// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/content/adblock_rule_service_impl.h"

#include <memory>
#include <utility>

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/ad_blocker/content/adblock_cosmetic_filter.h"
#include "components/ad_blocker/content/adblock_document_state.h"
#include "components/ad_blocker/content/adblock_request_filter.h"
#include "components/ad_blocker/content/index/adblock_rules_index.h"
#include "components/ad_blocker/content/index/adblock_rules_index_manager.h"
#include "components/ad_blocker/core/adblock_rule_manager_impl.h"
#include "components/ad_blocker/core/adblock_rule_source_handler.h"
#include "components/ad_blocker/core/adblock_stats_store_impl.h"
#include "components/ad_blocker/public/content/adblock_tab_state_and_logs.h"
#include "components/ad_blocker/public/core/adblock_known_sources_handler.h"
#include "components/ad_blocker/public/core/adblock_request_filter_rule_types.h"
#include "components/ad_blocker/public/core/adblock_stats_data.h"
#include "components/ad_blocker/public/core/adblock_types.h"
#include "components/request_filter/request_filter_registry.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"

namespace adblock_filter {
RuleServiceImpl::RuleServiceImpl(
    std::unique_ptr<Client> client,
    content::BrowserContext* context,
    RuleSourceHandler::RulesCompiler rules_compiler,
    std::string locale)
    : client_(std::move(client)),
      context_(context),
      rules_compiler_(std::move(rules_compiler)),
      locale_(std::move(locale)) {}

RuleServiceImpl::~RuleServiceImpl() {}

void RuleServiceImpl::AddObserver(RuleService::Observer* observer) {
  observers_.AddObserver(observer);
}

void RuleServiceImpl::RemoveObserver(RuleService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void RuleServiceImpl::Load(
    vivaldi::RequestFilterRegistry* request_filter_registry,
    PrefService* prefs) {
  CHECK(request_filter_registry);
  CHECK(!is_loaded_ && !state_store_);
  file_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  resources_.emplace(file_task_runner_.get());
  state_and_logs_.emplace(this);

  for (auto group : {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
    auto request_filter = std::make_unique<AdBlockRequestFilter>(
        weak_factory_.GetWeakPtr(), group, prefs);
    request_filter_registry->AddFilter(std::move(request_filter));
  }

  state_store_.emplace(context_->GetPath(), this, file_task_runner_);

  stats_store_.emplace(context_->GetPath());
  // Unretained is safe because we own the sources store
  state_store_->Load();
}

RulesIndex* RuleServiceImpl::GetRuleIndex(RuleGroup group) {
  if (!is_loaded_) {
    return nullptr;
  }

  return index_managers_[group]->rules_index();
}

StateAndLogsImpl& RuleServiceImpl::GetStateAndLogsImpl() {
  CHECK(state_and_logs_);
  return *state_and_logs_;
}

Resources& RuleServiceImpl::GetResources() {
  CHECK(resources_);
  return *resources_;
}

const std::optional<std::string_view> RuleServiceImpl::GetBrowserOwnedFrameUrlPrefix() {
  return client_->GetBrowserOwnedFrameUrlPrefix();
}

bool RuleServiceImpl::IsLoaded() const {
  return is_loaded_;
}

void RuleServiceImpl::Shutdown() {
  if (!is_loaded_) {
    return;
  }

  state_store_->OnRuleServiceShutdown();
  rule_manager_->RemoveObserver(this);
  for (auto [group, index_manager] : index_managers_) {
    index_manager->Shutdown();
  }
}

std::string RuleServiceImpl::GetRulesIndexChecksum(RuleGroup group) {
  CHECK(is_loaded_);
  CHECK(index_managers_[group]);
  return index_managers_[group]->index_checksum();
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

  for (auto [group, index_manager] : index_managers_) {
    index_manager.emplace(
        context_, &rule_manager_.value(), group,
        load_result.index_checksums[group],
        base::BindRepeating(&RuleServiceImpl::OnRulesIndexChanged,
                            base::Unretained(this), group),
        base::BindRepeating(&RuleServiceImpl::OnRulesIndexLoaded,
                            base::Unretained(this), group),
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
            counters[RuleGroup::kTrackingRules];
        const std::map<std::string, int> ad_map =
            counters[RuleGroup::kAdBlockingRules];

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
  vivaldi::RequestFilterRegistry::ClearCacheOnNavigation();
}

void RuleServiceImpl::OnRulesIndexLoaded(RuleGroup group) {
  for (RuleService::Observer& observer : observers_)
    observer.OnRulesIndexLoaded(group);
}

void RuleServiceImpl::OnRulesIndexChanged(RuleGroup group) {
  // The state store will read all checksums when saving. No need to worry about
  // which has changed.
  state_store_->ScheduleSave();
  vivaldi::RequestFilterRegistry::ClearCacheOnNavigation();
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
  auto activations =
      DocumentState::GetActivations(group, web_contents->GetPrimaryMainFrame());
  auto& rule_stub =
      activations.by_type[ActivationType::kWholeDocument].rule_stub;
  if (rule_stub) {
    if (known_sources_handler_->GetPresetIdForSourceId(
            group, rule_stub->rule_source_id) == preset_id)
      return true;
  }

  return false;
}

}  // namespace adblock_filter
