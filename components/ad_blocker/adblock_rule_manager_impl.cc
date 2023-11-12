// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/adblock_rule_manager_impl.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/ad_blocker/adblock_known_sources_handler.h"
#include "components/ad_blocker/adblock_rule_source_handler.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace adblock_filter {
RuleManagerImpl::RuleManagerImpl(
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    const base::FilePath& profile_path,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::array<RuleSources, kRuleGroupCount> rule_sources,
    ActiveExceptionsLists active_exceptions_lists,
    Exceptions exceptions,
    base::RepeatingClosure schedule_save,
    RuleSourceHandler::RulesCompiler rules_compiler,
    RuleSourceHandler::OnTrackerInfosUpdateCallback
        on_tracker_infos_update_callback)
    : active_exceptions_lists_(std::move(active_exceptions_lists)),
      exceptions_((std::move(exceptions))),
      schedule_save_(schedule_save),
      profile_path_(profile_path),
      url_loader_factory_(std::move(url_loader_factory)),
      rules_compiler_(std::move(rules_compiler)),
      on_tracker_infos_update_callback_(
          std::move(on_tracker_infos_update_callback)),
      file_task_runner_(std::move(file_task_runner)) {
  // All cases of base::Unretained here are safe. We are generally passing
  // callbacks to objects that we own, calling to either this or other objects
  // that we own.

  for (auto group : {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
    for (const auto& rule_source : rule_sources[static_cast<size_t>(group)]) {
      GetSourceMap(group)[rule_source.id] = std::make_unique<RuleSourceHandler>(
          rule_source, profile_path_, url_loader_factory_, file_task_runner_,
          rules_compiler_,
          base::BindRepeating(&RuleManagerImpl::OnSourceUpdated,
                              base::Unretained(this)),
          on_tracker_infos_update_callback_);
    }
  }
}
RuleManagerImpl::~RuleManagerImpl() = default;

void RuleManagerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void RuleManagerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::map<int64_t, std::unique_ptr<RuleSourceHandler>>&
RuleManagerImpl::GetSourceMap(RuleGroup group) {
  return rule_sources_[static_cast<size_t>(group)];
}

const std::map<int64_t, std::unique_ptr<RuleSourceHandler>>&
RuleManagerImpl::GetSourceMap(RuleGroup group) const {
  return rule_sources_[static_cast<size_t>(group)];
}

bool RuleManagerImpl::AddRulesSource(const KnownRuleSource& known_source) {
  // If a source with the same id exists, that means the corresponding known
  // source was already added
  auto& rule_sources = GetSourceMap(known_source.group);
  if (rule_sources.find(known_source.id) != rule_sources.end())
    return false;
  // base::Unretained is safe. We own both the rule_sources and the
  // blocked_urls_reporter
  rule_sources[known_source.id] = std::make_unique<RuleSourceHandler>(
      RuleSource(known_source), profile_path_, url_loader_factory_,
      file_task_runner_, rules_compiler_,
      base::BindRepeating(&RuleManagerImpl::OnSourceUpdated,
                          base::Unretained(this)),
      on_tracker_infos_update_callback_);
  rule_sources[known_source.id]->FetchNow();
  return known_source.id;
}

std::map<uint32_t, RuleSource> RuleManagerImpl::GetRuleSources(
    RuleGroup group) const {
  std::map<uint32_t, RuleSource> output;
  for (const auto& context : GetSourceMap(group)) {
    output.insert({context.first, context.second->rule_source()});
  }

  return output;
}

absl::optional<RuleSource> RuleManagerImpl::GetRuleSource(RuleGroup group,
                                                          uint32_t source_id) {
  const auto& rule_sources = GetSourceMap(group);
  const auto& source_context = rule_sources.find(source_id);
  if (source_context == rule_sources.end())
    return absl::nullopt;
  return source_context->second->rule_source();
}

bool RuleManagerImpl::FetchRuleSourceNow(RuleGroup group, uint32_t source_id) {
  auto& rule_sources = GetSourceMap(group);

  const auto& rule_source = rule_sources.find(source_id);
  if (rule_source == rule_sources.end())
    return false;

  rule_source->second->FetchNow();
  return true;
}

void RuleManagerImpl::DeleteRuleSource(const KnownRuleSource& known_source) {
  auto& rule_sources = GetSourceMap(known_source.group);

  const auto& rule_source = rule_sources.find(known_source.id);
  if (rule_source == rule_sources.end())
    return;

  rule_source->second->Clear();
  rule_sources.erase(rule_source);

  schedule_save_.Run();

  for (Observer& observer : observers_)
    observer.OnRuleSourceDeleted(known_source.id, known_source.group);
}

void RuleManagerImpl::SetActiveExceptionList(RuleGroup group,
                                             ExceptionsList list) {
  active_exceptions_lists_[static_cast<size_t>(group)] = list;

  for (Observer& observer : observers_)
    observer.OnExceptionListStateChanged(group);

  schedule_save_.Run();
}

RuleManagerImpl::ExceptionsList RuleManagerImpl::GetActiveExceptionList(
    RuleGroup group) const {
  return active_exceptions_lists_[static_cast<size_t>(group)];
}

void RuleManagerImpl::AddExceptionForDomain(RuleGroup group,
                                            ExceptionsList list,
                                            const std::string& domain) {
  base::StringPiece canonicalized_domain(domain);
  if (canonicalized_domain.back() == '.')
    canonicalized_domain.remove_suffix(1);

  exceptions_[static_cast<size_t>(group)][list].insert(
      std::string(canonicalized_domain));

  for (Observer& observer : observers_)
    observer.OnExceptionListChanged(group, list);

  schedule_save_.Run();
}
void RuleManagerImpl::RemoveExceptionForDomain(RuleGroup group,
                                               ExceptionsList list,
                                               const std::string& domain) {
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

  for (Observer& observer : observers_)
    observer.OnExceptionListChanged(group, list);

  schedule_save_.Run();
}

void RuleManagerImpl::RemoveAllExceptions(RuleGroup group,
                                          ExceptionsList list) {
  exceptions_[static_cast<size_t>(group)][list].clear();

  for (Observer& observer : observers_)
    observer.OnExceptionListChanged(group, list);

  schedule_save_.Run();
}

const std::set<std::string>& RuleManagerImpl::GetExceptions(
    RuleGroup group,
    ExceptionsList list) const {
  return exceptions_[static_cast<size_t>(group)][list];
}

bool RuleManagerImpl::IsExemptOfFiltering(RuleGroup group,
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

void RuleManagerImpl::OnSourceUpdated(RuleSourceHandler* rule_source_handler) {
  schedule_save_.Run();

  for (Observer& observer : observers_)
    observer.OnRulesSourceUpdated(rule_source_handler->rule_source());
}

void RuleManagerImpl::OnCompiledRulesReadFailCallback(RuleGroup rule_group,
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
}  // namespace adblock_filter
