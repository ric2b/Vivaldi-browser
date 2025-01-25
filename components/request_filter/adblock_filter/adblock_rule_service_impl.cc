// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/adblock_rule_service_impl.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/ad_blocker/adblock_known_sources_handler.h"
#include "components/ad_blocker/adblock_rule_manager_impl.h"
#include "components/ad_blocker/adblock_rule_source_handler.h"
#include "components/request_filter/adblock_filter/adblock_cosmetic_filter.h"
#include "components/request_filter/adblock_filter/adblock_request_filter.h"
#include "components/request_filter/adblock_filter/adblock_rules_index.h"
#include "components/request_filter/adblock_filter/adblock_rules_index_manager.h"
#include "components/request_filter/adblock_filter/adblock_tab_state_and_logs.h"
#include "components/request_filter/request_filter_manager.h"
#include "components/request_filter/request_filter_manager_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace adblock_filter {
RuleServiceImpl::RuleServiceImpl(
    content::BrowserContext* context,
    RuleSourceHandler::RulesCompiler rules_compiler,
    std::string locale)
    : context_(context),
      rules_compiler_(std::move(rules_compiler)),
      locale_(std::move(locale)) {}
RuleServiceImpl::~RuleServiceImpl() {}

void RuleServiceImpl::AddObserver(RuleService::Observer* observer) {
  observers_.AddObserver(observer);
}

void RuleServiceImpl::RemoveObserver(RuleService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void RuleServiceImpl::Load() {
  DCHECK(!is_loaded_ && !state_store_);
  file_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  resources_.emplace(file_task_runner_.get());

  state_store_.emplace(context_->GetPath(), this, file_task_runner_);

  // Unretained is safe because we own the sources store
  state_store_->Load(
      base::BindOnce(&RuleServiceImpl::OnStateLoaded, base::Unretained(this)));
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

void RuleServiceImpl::AddRequestFilter(RuleGroup group) {
  auto request_filter = std::make_unique<AdBlockRequestFilter>(
      index_managers_[static_cast<size_t>(group)]->AsWeakPtr(),
      state_and_logs_->AsWeakPtr(), resources_->AsWeakPtr());
  request_filters_[static_cast<size_t>(group)] = request_filter.get();
  vivaldi::RequestFilterManagerFactory::GetForBrowserContext(context_)
      ->AddFilter(std::move(request_filter));
}

bool RuleServiceImpl::IsRuleGroupEnabled(RuleGroup group) const {
  return request_filters_[static_cast<size_t>(group)] != nullptr;
}

void RuleServiceImpl::SetRuleGroupEnabled(RuleGroup group, bool enabled) {
  DCHECK(is_loaded_);
  if (IsRuleGroupEnabled(group) == enabled)
    return;

  if (!enabled) {
    vivaldi::RequestFilterManagerFactory::GetForBrowserContext(context_)
        ->RemoveFilter(request_filters_[static_cast<size_t>(group)]);
    request_filters_[static_cast<size_t>(group)] = nullptr;
  } else {
    AddRequestFilter(group);
  }

  for (RuleService::Observer& observer : observers_)
    observer.OnGroupStateChanged(group);

  state_store_->ScheduleSave();
}

std::string RuleServiceImpl::GetRulesIndexChecksum(RuleGroup group) {
  DCHECK(index_managers_[static_cast<size_t>(group)]);
  return index_managers_[static_cast<size_t>(group)]->index_checksum();
}

RuleServiceImpl::IndexBuildResult RuleServiceImpl::GetRulesIndexBuildResult(
    RuleGroup group) {
  return kBuildSuccess;
}

void RuleServiceImpl::OnStateLoaded(
    RuleServiceStorage::LoadResult load_result) {
  // All cases of base::Unretained here are safe. We are generally passing
  // callbacks to objects that we own, calling to either this or other objects
  // that we own.
  state_and_logs_.emplace(
      load_result.blocked_reporting_start,
      std::move(load_result.blocked_domains_counters),
      std::move(load_result.blocked_for_origin_counters),
      base::BindRepeating(&RuleServiceStorage::ScheduleSave,
                          base::Unretained(&state_store_.value())));

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
        context_, this, group,
        load_result.index_checksums[static_cast<size_t>(group)],
        base::BindRepeating(&RuleServiceImpl::OnRulesIndexChanged,
                            base::Unretained(this), group),
        base::BindRepeating(&RuleServiceImpl::OnRulesIndexLoaded,
                            base::Unretained(this), group),
        base::BindRepeating(&RuleManager::OnCompiledRulesReadFailCallback,
                            base::Unretained(&rule_manager_.value())),
        file_task_runner_);

    if (load_result.groups_enabled[static_cast<size_t>(group)]) {
      AddRequestFilter(group);
    }
  }

  std::array<RulesIndexManager*, kRuleGroupCount> index_manager_ptrs;
  for (size_t i = 0; i < kRuleGroupCount; i++) {
    if (index_managers_[i])
      index_manager_ptrs[i] = &(index_managers_[i].value());
  }
  content_injection_provider_.emplace(context_, index_manager_ptrs,
                                      &(resources_.value()));

  known_sources_handler_.emplace(
      this, load_result.storage_version, locale_, load_result.known_sources,
      std::move(load_result.deleted_presets),
      base::BindRepeating(&RuleServiceStorage::ScheduleSave,
                          base::Unretained(&state_store_.value())));

  is_loaded_ = true;
  for (RuleService::Observer& observer : observers_)
    observer.OnRuleServiceStateLoaded(this);
}

bool RuleServiceImpl::IsDocumentBlocked(RuleGroup group,
                                        content::RenderFrameHost* frame,
                                        const GURL& url) const {
  DCHECK(is_loaded_);
  if (!url.SchemeIs(url::kFtpScheme) && !url.SchemeIsHTTPOrHTTPS())
    return false;

  if (!state_and_logs_) {
    return false;
  }

  return state_and_logs_->WasFrameBlocked(group, frame);
}

RuleManager* RuleServiceImpl::GetRuleManager() {
  DCHECK(rule_manager_);
  return &rule_manager_.value();
}

KnownRuleSourcesHandler* RuleServiceImpl::GetKnownSourcesHandler() {
  DCHECK(known_sources_handler_);
  return &known_sources_handler_.value();
}

StateAndLogs* RuleServiceImpl::GetStateAndLogs() {
  DCHECK(state_and_logs_);
  return &state_and_logs_.value();
}

void RuleServiceImpl::OnExceptionListChanged(RuleGroup group,
                                             RuleManager::ExceptionsList list) {
  if (request_filters_[static_cast<size_t>(group)]) {
    vivaldi::RequestFilterManagerFactory::GetForBrowserContext(context_)
        ->ClearCacheOnNavigation();
  }
}

void RuleServiceImpl::OnRulesIndexChanged(RuleGroup group) {
  // The state store will read all checksums when saving. No need to worry about
  // which has changed.
  state_store_->ScheduleSave();
  for (RuleService::Observer& observer : observers_)
    observer.OnRulesIndexBuilt(group, RuleService::kBuildSuccess);
}

void RuleServiceImpl::OnRulesIndexLoaded(RuleGroup group) {
  if (request_filters_[static_cast<size_t>(group)]) {
    vivaldi::RequestFilterManagerFactory::GetForBrowserContext(context_)
        ->ClearCacheOnNavigation();
  }
}

void RuleServiceImpl::InitializeCosmeticFilter(CosmeticFilter* filter) {
  std::array<base::WeakPtr<RulesIndexManager>, kRuleGroupCount>
      weak_index_managers;

  for (size_t i = 0; i < kRuleGroupCount; i++) {
    if (index_managers_[i])
      weak_index_managers[i] = index_managers_[i]->AsWeakPtr();
  }

  filter->Initialize(weak_index_managers);
}

bool RuleServiceImpl::IsApplyingIosRules(RuleGroup group) {
  // Only meaningful on iOS/WebKit
  return false;
}

bool RuleServiceImpl::HasDocumentActivationForRuleSource(
    adblock_filter::RuleGroup group,
    content::WebContents* web_contents,
    uint32_t rule_source_id) {
  auto *tab_helper = GetStateAndLogs()->GetTabHelper(web_contents);

  // Tab helper can be null when page is still loading.
  if (!tab_helper)
    return false;

  auto& activations = tab_helper->GetTabActivations(group);
  auto rule_activation = activations.find(adblock_filter::RequestFilterRule::kWholeDocument);
  if (rule_activation != activations.end()) {
    auto& rule_data = rule_activation->second.rule_data;
    if (rule_data) {
      if (rule_data->rule_source_id == rule_source_id)
        return true;
    }
  }

  return false;
}

}  // namespace adblock_filter
