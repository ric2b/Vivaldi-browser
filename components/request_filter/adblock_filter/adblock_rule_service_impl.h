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
#include "base/uuid.h"
#include "components/ad_blocker/adblock_known_sources_handler_impl.h"
#include "components/ad_blocker/adblock_resources.h"
#include "components/ad_blocker/adblock_rule_manager_impl.h"
#include "components/ad_blocker/adblock_rule_service_storage.h"
#include "components/ad_blocker/adblock_rule_source_handler.h"
#include "components/ad_blocker/adblock_stats_store.h"
#include "components/ad_blocker/adblock_stats_store_impl.h"
#include "components/ad_blocker/adblock_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/request_filter/adblock_filter/adblock_content_injection_provider.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_content.h"
#include "components/request_filter/adblock_filter/adblock_rules_index_manager.h"
#include "components/request_filter/adblock_filter/adblock_state_and_logs_impl.h"

class PrefService;

namespace base {
class SequencedTaskRunner;
}

namespace content {
class BrowserContext;
}

namespace vivaldi {
class RequestFilterRegistry;
}

namespace adblock_filter {
class AdBlockRequestFilter;
class RulesIndex;

class RuleServiceImpl : public RuleServiceContent,
                        public RuleManager::Observer {
 public:
  explicit RuleServiceImpl(
      content::BrowserContext* context,
      PrefService* prefs,
      vivaldi::RequestFilterRegistry* request_filter_registry,
      RuleSourceHandler::RulesCompiler rules_compiler,
      std::string locale);
  ~RuleServiceImpl() override;
  RuleServiceImpl(const RuleServiceImpl&) = delete;
  RuleServiceImpl& operator=(const RuleServiceImpl&) = delete;

  void Load();

  RulesIndex* GetRuleIndex(RuleGroup group);
  StateAndLogsImpl& GetStateAndLogsImpl();
  Resources& GetResources();

  // Implementing RuleService
  bool IsLoaded() const override;
  bool IsRuleGroupEnabled(RuleGroup group) const override;
  void SetRuleGroupEnabled(RuleGroup group, bool enabled) override;
  std::array<std::optional<TabStateAndLogs::RuleData>, kRuleGroupCount>
  IsDocumentBlocked(content::RenderFrameHost* frame) const override;
  void AddObserver(RuleService::Observer* observer) override;
  void RemoveObserver(RuleService::Observer* observer) override;
  bool IsApplyingIosRules(RuleGroup group) override;
  bool HasDocumentActivationForRuleSource(adblock_filter::RuleGroup group,
                                          content::WebContents* web_contents,
                                          base::Uuid preset_id) override;
  std::string GetRulesIndexChecksum(RuleGroup group) override;
  IndexBuildResult GetRulesIndexBuildResult(RuleGroup group) override;
  RuleManager* GetRuleManager() override;
  KnownRuleSourcesHandler* GetKnownSourcesHandler() override;
  StateAndLogs* GetStateAndLogs() override;
  StatsStore* GetStatsStore() override;
  std::unique_ptr<CosmeticFilter> MakeCosmeticFilter(
      content::RenderFrameHost* frame) override;

  // Implementing KeyedService
  void Shutdown() override;

  // Implementing RuleManager::Observer
  void OnExceptionListChanged(RuleGroup group,
                              RuleManager::ExceptionsList list) override;

  bool IsLoaded() { return is_loaded_; }

 private:
  void OnStateLoaded(RuleServiceStorage::LoadResult load_result);
  void MigrateOldStatsData(const RuleServiceStorage::LoadResult* load_result);

  void OnRulesIndexChanged(RuleGroup group);
  void OnRulesIndexLoaded(RuleGroup group);

  void OnEnableDocumentBlockingChanged();
  void OnPingBlockingChanged();

  void AddRequestFilter(RuleGroup group);

  const raw_ptr<content::BrowserContext> context_;
  const raw_ptr<PrefService> prefs_;
  const raw_ptr<vivaldi::RequestFilterRegistry> request_filter_registry_;
  PrefChangeRegistrar pref_change_registrar_;

  RuleSourceHandler::RulesCompiler rules_compiler_;
  std::string locale_;

  std::array<std::optional<RulesIndexManager>, kRuleGroupCount> index_managers_;

  // We can't have one injection manager per rule group, because they all use
  // the same resources and we only want to provide one copy of the static
  // injections to the content injection module.
  std::optional<ContentInjectionProvider> content_injection_provider_;

  // Keeps track of the request filters we have set up, to allow tearing them
  // down if needed. These pointers are not guaranteed to be valid at any time.
  std::array<AdBlockRequestFilter*, kRuleGroupCount> request_filters_ = {
      nullptr, nullptr};

  std::optional<StateAndLogsImpl> state_and_logs_;
  std::optional<RuleServiceStorage> state_store_;
  std::unique_ptr<StatsStoreImpl> stats_store_;
  std::optional<Resources> resources_;

  bool is_loaded_ = false;
  std::optional<RuleManagerImpl> rule_manager_;
  std::optional<KnownRuleSourcesHandlerImpl> known_sources_handler_;

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  base::ObserverList<RuleService::Observer> observers_;

  base::WeakPtrFactory<RuleServiceImpl> weak_factory_{this};
};

}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULE_SERVICE_IMPL_H_
