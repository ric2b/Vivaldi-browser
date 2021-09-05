// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULE_SERVICE_IMPL_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULE_SERVICE_IMPL_H_

#include <array>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/request_filter/adblock_filter/adblock_metadata.h"
#include "components/request_filter/adblock_filter/adblock_resources.h"
#include "components/request_filter/adblock_filter/adblock_rule_service.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_storage.h"
#include "components/request_filter/adblock_filter/adblock_rules_index_manager.h"
#include "components/request_filter/adblock_filter/blocked_urls_reporter.h"

namespace base {
class SequencedTaskRunner;
}

namespace content {
class BrowserContext;
}

namespace adblock_filter {
class RuleSourceHandler;
class AdBlockRequestFilter;

class RuleServiceImpl : public RuleService {
 public:
  explicit RuleServiceImpl(content::BrowserContext* context);
  ~RuleServiceImpl() override;

  void Load();
  Delegate* delegate() { return delegate_; }

  // Implementing RuleService
  void SetDelegate(Delegate* delegate) override;
  bool IsLoaded() const override;
  bool IsRuleGroupEnabled(RuleGroup group) const override;
  void SetRuleGroupEnabled(RuleGroup group, bool enabled) override;
  base::Optional<uint32_t> AddRulesFromURL(RuleGroup group,
                                           const GURL& url) override;
  base::Optional<uint32_t> AddRulesFromFile(
      RuleGroup group,
      const base::FilePath& file) override;
  base::Optional<RuleSource> GetRuleSource(RuleGroup group,
                                           uint32_t source_id) override;
  std::map<uint32_t, RuleSource> GetRuleSources(RuleGroup group) const override;
  bool FetchRuleSourceNow(RuleGroup group, uint32_t source_id) override;
  void DeleteRuleSource(RuleGroup group, uint32_t source_id) override;
  void SetActiveExceptionList(RuleGroup group, ExceptionsList list) override;
  ExceptionsList GetActiveExceptionList(RuleGroup group) const override;

  void AddExceptionForDomain(RuleGroup group,
                             ExceptionsList list,
                             const std::string& domain) override;
  void RemoveExceptionForDomain(RuleGroup group,
                                ExceptionsList list,
                                const std::string& domain) override;
  void RemoveAllExceptions(RuleGroup group, ExceptionsList list) override;
  const std::set<std::string>& GetExceptions(
      RuleGroup group,
      ExceptionsList list) const override;
  bool IsExemptOfFiltering(RuleGroup group, url::Origin origin) const override;
  bool IsDocumentBlocked(RuleGroup group,
                         content::RenderFrameHost* frame,
                         const GURL& url) const override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  std::string GetRulesIndexChecksum(RuleGroup group) override;
  KnownRuleSourcesHandler* GetKnownSourcesHandler() override;
  BlockedUrlsReporter* GetBlockerUrlsReporter() override;
  void InitializeCosmeticFilter(CosmeticFilter* filter) override;

  // Implementing KeyedService
  void Shutdown() override;

 private:
  void OnStateLoaded(
      std::unique_ptr<RuleServiceStorage::LoadResult> load_result);

  void OnSourceUpdated(RuleSourceHandler* rule_source_handler);

  void OnRulesIndexChanged();
  void OnRulesIndexLoaded(RuleGroup group);
  void OnRulesBufferReadFailCallback(RuleGroup rule_group, uint32_t source_id);

  Delegate* delegate_ = nullptr;

  std::map<int64_t, std::unique_ptr<RuleSourceHandler>>& GetSourceMap(
      RuleGroup group);

  const std::map<int64_t, std::unique_ptr<RuleSourceHandler>>& GetSourceMap(
      RuleGroup group) const;

  void AddRequestFilter(RuleGroup group);

  content::BrowserContext* context_;
  std::array<std::map<int64_t, std::unique_ptr<RuleSourceHandler>>,
             kRuleGroupCount>
      rule_sources_;
  std::array<ExceptionsList, kRuleGroupCount> active_exceptions_lists_ = {
      kProcessList, kProcessList};

  std::array<std::array<std::set<std::string>, kExceptionListCount>,
             kRuleGroupCount>
      exceptions_;

  std::array<base::Optional<RulesIndexManager>, kRuleGroupCount>
      index_managers_;

  // Keeps track of the request filters we have set up, to allow tearing them
  // down if needed. These pointers are not guaranteed to be valid at any time.
  std::array<AdBlockRequestFilter*, kRuleGroupCount> request_filters_ = {
      nullptr, nullptr};

  base::Optional<BlockedUrlsReporter> blocked_urls_reporter_;
  base::Optional<RuleServiceStorage> state_store_;
  base::Optional<Resources> resources_;

  bool is_loaded_ = false;
  base::Optional<KnownRuleSourcesHandler> known_sources_handler_;

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(RuleServiceImpl);
};

}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULE_SERVICE_IMPL_H_
