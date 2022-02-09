// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/adblock_rule_service_impl.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/request_filter/adblock_filter/adblock_cosmetic_filter.h"
#include "components/request_filter/adblock_filter/adblock_known_sources_handler.h"
#include "components/request_filter/adblock_filter/adblock_request_filter.h"
#include "components/request_filter/adblock_filter/adblock_rule_source_handler.h"
#include "components/request_filter/adblock_filter/adblock_rules_index.h"
#include "components/request_filter/adblock_filter/adblock_rules_index_manager.h"
#include "components/request_filter/request_filter_manager.h"
#include "components/request_filter/request_filter_manager_factory.h"

namespace adblock_filter {
RuleServiceImpl::RuleServiceImpl(content::BrowserContext* context)
    : context_(context) {}
RuleServiceImpl::~RuleServiceImpl() {
  if (delegate_)
    delegate_->RuleServiceDeleted();
}

void RuleServiceImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void RuleServiceImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void RuleServiceImpl::Load() {
  DCHECK(!is_loaded_ && !state_store_);
  file_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  resources_.emplace(file_task_runner_.get());

  state_store_.emplace(context_, this, file_task_runner_.get());

  // Unretained is safe because we own the sources store
  state_store_->Load(
      base::BindOnce(&RuleServiceImpl::OnStateLoaded, base::Unretained(this)));
}

void RuleServiceImpl::SetDelegate(Delegate* delegate) {
  DCHECK(!delegate_);
  delegate_ = delegate;
}

bool RuleServiceImpl::IsLoaded() const {
  return is_loaded_;
}

void RuleServiceImpl::Shutdown() {
  if (is_loaded_) {
    state_store_->OnRuleServiceShutdown();
  }
}

void RuleServiceImpl::AddRequestFilter(RuleGroup group) {
  auto request_filter = std::make_unique<AdBlockRequestFilter>(
      index_managers_[static_cast<size_t>(group)]->AsWeakPtr(),
      blocked_urls_reporter_->AsWeakPtr(), resources_->AsWeakPtr());
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

  for (Observer& observer : observers_)
    observer.OnGroupStateChanged(group);

  state_store_->ScheduleSave();
}

std::map<int64_t, std::unique_ptr<RuleSourceHandler>>&
RuleServiceImpl::GetSourceMap(RuleGroup group) {
  return rule_sources_[static_cast<size_t>(group)];
}

const std::map<int64_t, std::unique_ptr<RuleSourceHandler>>&
RuleServiceImpl::GetSourceMap(RuleGroup group) const {
  return rule_sources_[static_cast<size_t>(group)];
}

std::string RuleServiceImpl::GetRulesIndexChecksum(RuleGroup group) {
  DCHECK(index_managers_[static_cast<size_t>(group)]);
  return index_managers_[static_cast<size_t>(group)]->index_checksum();
}

void RuleServiceImpl::OnStateLoaded(
    std::unique_ptr<RuleServiceStorage::LoadResult> load_result) {
  // All cases of base::Unretained here are safe. We are generally passing
  // callbacks to objects that we own, calling to either this or other objects
  // that we own.

  active_exceptions_lists_ = load_result->active_exceptions_lists;
  blocked_urls_reporter_.emplace(
      std::move(load_result->blocked_counters),
      base::BindRepeating(&RuleServiceStorage::ScheduleSave,
                          base::Unretained(&state_store_.value())));

  for (auto group : {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
    for (const auto& rule_source :
         load_result->rule_sources[static_cast<size_t>(group)]) {
      GetSourceMap(group)[rule_source.id] = std::make_unique<RuleSourceHandler>(
          context_, rule_source, file_task_runner_,
          base::BindRepeating(&RuleServiceImpl::OnSourceUpdated,
                              base::Unretained(this)),
          base::BindRepeating(
              &BlockedUrlsReporter::OnTrackerInfosUpdated,
              base::Unretained(&blocked_urls_reporter_.value())));
    }
    index_managers_[static_cast<size_t>(group)].emplace(
        context_, this, group,
        load_result->index_checksums[static_cast<size_t>(group)],
        base::BindRepeating(&RuleServiceImpl::OnRulesIndexChanged,
                            base::Unretained(this)),
        base::BindRepeating(&RuleServiceImpl::OnRulesIndexLoaded,
                            base::Unretained(this), group),
        base::BindRepeating(&RuleServiceImpl::OnRulesBufferReadFailCallback,
                            base::Unretained(this)),
        file_task_runner_);

    if (load_result->groups_enabled[static_cast<size_t>(group)]) {
      AddRequestFilter(group);
    }

    exceptions_[static_cast<size_t>(group)].swap(
        load_result->exceptions[static_cast<size_t>(group)]);
  }

  std::array<RulesIndexManager*, kRuleGroupCount> index_manager_ptrs;
  for (size_t i = 0; i < kRuleGroupCount; i++) {
    if (index_managers_[i])
      index_manager_ptrs[i] = &(index_managers_[i].value());
  }
  content_injection_provider_.emplace(context_, index_manager_ptrs,
                                      &(resources_.value()));

  known_sources_handler_.emplace(
      this, load_result->storage_version, load_result->known_sources,
      std::move(load_result->deleted_presets),
      base::BindRepeating(&RuleServiceStorage::ScheduleSave,
                          base::Unretained(&state_store_.value())));

  is_loaded_ = true;
  for (Observer& observer : observers_)
    observer.OnRuleServiceStateLoaded(this);
}

bool RuleServiceImpl::AddRulesSource(const KnownRuleSource& known_source) {
  // If a source with the same id exists, that means the corresponding known
  // source was already added
  auto& rule_sources = GetSourceMap(known_source.group);
  if (rule_sources.find(known_source.id) != rule_sources.end())
    return false;
  // base::Unretained is safe. We own both the rule_sources and the
  // blocked_urls_reporter
  rule_sources[known_source.id] = std::make_unique<RuleSourceHandler>(
      context_, RuleSource(known_source), file_task_runner_,
      base::BindRepeating(&RuleServiceImpl::OnSourceUpdated,
                          base::Unretained(this)),
      base::BindRepeating(&BlockedUrlsReporter::OnTrackerInfosUpdated,
                          base::Unretained(&blocked_urls_reporter_.value())));
  rule_sources[known_source.id]->FetchNow();
  return known_source.id;
}

std::map<uint32_t, RuleSource> RuleServiceImpl::GetRuleSources(
    RuleGroup group) const {
  std::map<uint32_t, RuleSource> output;
  for (const auto& context : GetSourceMap(group)) {
    output.insert({context.first, context.second->rule_source()});
  }

  return output;
}

absl::optional<RuleSource> RuleServiceImpl::GetRuleSource(RuleGroup group,
                                                          uint32_t source_id) {
  const auto& rule_sources = GetSourceMap(group);
  const auto& source_context = rule_sources.find(source_id);
  if (source_context == rule_sources.end())
    return absl::nullopt;
  return source_context->second->rule_source();
}

bool RuleServiceImpl::FetchRuleSourceNow(RuleGroup group, uint32_t source_id) {
  auto& rule_sources = GetSourceMap(group);

  const auto& rule_source = rule_sources.find(source_id);
  if (rule_source == rule_sources.end())
    return false;

  rule_source->second->FetchNow();
  return true;
}

void RuleServiceImpl::DeleteRuleSource(const KnownRuleSource& known_source) {
  auto& rule_sources = GetSourceMap(known_source.group);

  const auto& rule_source = rule_sources.find(known_source.id);
  if (rule_source == rule_sources.end())
    return;

  rule_source->second->Clear();
  rule_sources.erase(rule_source);

  state_store_->ScheduleSave();

  for (Observer& observer : observers_)
    observer.OnRuleSourceDeleted(known_source.id, known_source.group);
}

void RuleServiceImpl::SetActiveExceptionList(RuleGroup group,
                                             ExceptionsList list) {
  DCHECK(is_loaded_);
  active_exceptions_lists_[static_cast<size_t>(group)] = list;

  for (Observer& observer : observers_)
    observer.OnGroupStateChanged(group);

  state_store_->ScheduleSave();
}

RuleService::ExceptionsList RuleServiceImpl::GetActiveExceptionList(
    RuleGroup group) const {
  return active_exceptions_lists_[static_cast<size_t>(group)];
}

void RuleServiceImpl::AddExceptionForDomain(RuleGroup group,
                                            ExceptionsList list,
                                            const std::string& domain) {
  DCHECK(is_loaded_);

  base::StringPiece canonicalized_domain(domain);
  if (canonicalized_domain.back() == '.')
    canonicalized_domain.remove_suffix(1);

  exceptions_[static_cast<size_t>(group)][list].insert(
      std::string(canonicalized_domain));

  if (request_filters_[static_cast<size_t>(group)]) {
    vivaldi::RequestFilterManagerFactory::GetForBrowserContext(context_)
        ->ClearCacheOnNavigation();
  }

  for (Observer& observer : observers_)
    observer.OnExceptionListChanged(group, list);

  state_store_->ScheduleSave();
}
void RuleServiceImpl::RemoveExceptionForDomain(RuleGroup group,
                                               ExceptionsList list,
                                               const std::string& domain) {
  DCHECK(is_loaded_);

  base::StringPiece canonicalized_domain(domain);
  if (canonicalized_domain.back() == '.')
    canonicalized_domain.remove_suffix(1);

  for (size_t position = 0;; ++position) {
    const base::StringPiece subdomain = canonicalized_domain.substr(position);
    exceptions_[static_cast<size_t>(group)][list].erase(std::string(subdomain));

    position = canonicalized_domain.find('.', position);
    if (position == base::StringPiece::npos)
      break;
  }

  if (request_filters_[static_cast<size_t>(group)]) {
    vivaldi::RequestFilterManagerFactory::GetForBrowserContext(context_)
        ->ClearCacheOnNavigation();
  }

  for (Observer& observer : observers_)
    observer.OnExceptionListChanged(group, list);

  state_store_->ScheduleSave();
}

void RuleServiceImpl::RemoveAllExceptions(RuleGroup group,
                                          ExceptionsList list) {
  DCHECK(is_loaded_);
  exceptions_[static_cast<size_t>(group)][list].clear();

  if (request_filters_[static_cast<size_t>(group)]) {
    vivaldi::RequestFilterManagerFactory::GetForBrowserContext(context_)
        ->ClearCacheOnNavigation();
  }

  for (Observer& observer : observers_)
    observer.OnExceptionListChanged(group, list);

  state_store_->ScheduleSave();
}

const std::set<std::string>& RuleServiceImpl::GetExceptions(
    RuleGroup group,
    ExceptionsList list) const {
  return exceptions_[static_cast<size_t>(group)][list];
}

bool RuleServiceImpl::IsExemptOfFiltering(RuleGroup group,
                                          url::Origin origin) const {
  bool default_exempt =
      active_exceptions_lists_[static_cast<size_t>(group)] == kProcessList;
  if (origin.opaque())
    return default_exempt;

  base::StringPiece canonicalized_host(origin.host());
  if (canonicalized_host.empty())
    return default_exempt;

  // If the host name ends with a dot, then ignore it.
  if (canonicalized_host.back() == '.')
    canonicalized_host.remove_suffix(1);

  for (size_t position = 0;; ++position) {
    const base::StringPiece subdomain = canonicalized_host.substr(position);

    if (exceptions_[static_cast<size_t>(group)]
                   [active_exceptions_lists_[static_cast<size_t>(group)]]
                       .count(std::string(subdomain)) != 0)
      return !default_exempt;

    position = canonicalized_host.find('.', position);
    if (position == base::StringPiece::npos)
      break;
  }

  return default_exempt;
}

bool RuleServiceImpl::IsDocumentBlocked(RuleGroup group,
                                        content::RenderFrameHost* frame,
                                        const GURL& url) const {
  if (!url.SchemeIs(url::kFtpScheme) && !url.SchemeIsHTTPOrHTTPS())
    return false;

  url::Origin origin = url::Origin::Create(url);
  if (IsExemptOfFiltering(group, origin))
    return false;

  auto* index = index_managers_[static_cast<size_t>(group)]->rules_index();
  if (!index)
    return false;

  RulesIndex::ActivationsFound activations =
      index->FindMatchingActivationsRules(url, origin, false, frame);

  return (activations.in_block_rules & flat::ActivationType_DOCUMENT) != 0;
}

KnownRuleSourcesHandler* RuleServiceImpl::GetKnownSourcesHandler() {
  DCHECK(known_sources_handler_);
  return &known_sources_handler_.value();
}

BlockedUrlsReporter* RuleServiceImpl::GetBlockerUrlsReporter() {
  DCHECK(blocked_urls_reporter_);
  return &blocked_urls_reporter_.value();
}

void RuleServiceImpl::OnSourceUpdated(RuleSourceHandler* rule_source_handler) {
  state_store_->ScheduleSave();

  for (Observer& observer : observers_)
    observer.OnRulesSourceUpdated(rule_source_handler->rule_source());
}

void RuleServiceImpl::OnRulesIndexChanged() {
  // The state store will read all checksums when saving. No need to worry about
  // which has changed.
  state_store_->ScheduleSave();
}

void RuleServiceImpl::OnRulesIndexLoaded(RuleGroup group) {
  if (request_filters_[static_cast<size_t>(group)]) {
    vivaldi::RequestFilterManagerFactory::GetForBrowserContext(context_)
        ->ClearCacheOnNavigation();
  }
}

void RuleServiceImpl::OnRulesBufferReadFailCallback(RuleGroup rule_group,
                                                    uint32_t source_id) {
  const auto& rule_sources = GetSourceMap(rule_group);
  const auto& source_context = rule_sources.find(source_id);
  DCHECK(source_context != rule_sources.end());
  // If the last fetch attempt failed, don't try again immediately to make sure
  // that we don't end up in an infinite rety loop.
  if (source_context->second->rule_source().last_fetch_result ==
      FetchResult::kSuccess)
    source_context->second->FetchNow();
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

}  // namespace adblock_filter
