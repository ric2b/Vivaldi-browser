// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_RULE_SERVICE_IMPL_H_
#define COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_RULE_SERVICE_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/uuid.h"
#include "components/ad_blocker/content/adblock_content_injection_provider.h"
#include "components/ad_blocker/content/adblock_state_and_logs_impl.h"
#include "components/ad_blocker/content/index/adblock_rules_index_manager.h"
#include "components/ad_blocker/core/adblock_known_sources_handler_impl.h"
#include "components/ad_blocker/core/adblock_resources.h"
#include "components/ad_blocker/core/adblock_rule_manager_impl.h"
#include "components/ad_blocker/core/adblock_rule_service_storage.h"
#include "components/ad_blocker/core/adblock_rule_service_storage_delegate.h"
#include "components/ad_blocker/core/adblock_rule_source_handler.h"
#include "components/ad_blocker/core/adblock_stats_store_impl.h"
#include "components/ad_blocker/public/content/adblock_rule_service.h"
#include "components/ad_blocker/public/core/adblock_stats_store.h"
#include "components/ad_blocker/public/core/adblock_types.h"

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

class RuleServiceImpl : public RuleService,
                        public RuleServiceStorageDelegate,
                        public RuleManager::Observer {
 public:
  RuleServiceImpl(std::unique_ptr<Client> client,
                  content::BrowserContext* context,
                  RuleSourceHandler::RulesCompiler rules_compiler,
                  std::string locale);
  ~RuleServiceImpl() override;
  RuleServiceImpl(const RuleServiceImpl&) = delete;
  RuleServiceImpl& operator=(const RuleServiceImpl&) = delete;

  void Load(vivaldi::RequestFilterRegistry* request_filter_registry,
            PrefService* prefs);

  RulesIndex* GetRuleIndex(RuleGroup group);
  StateAndLogsImpl& GetStateAndLogsImpl();
  Resources& GetResources();

  const std::optional<std::string_view> GetBrowserOwnedFrameUrlPrefix();

  // Implementing RuleService
  bool IsLoaded() const override;
  void AddObserver(RuleService::Observer* observer) override;
  void RemoveObserver(RuleService::Observer* observer) override;
  bool HasDocumentActivationForRuleSource(adblock_filter::RuleGroup group,
                                          content::WebContents* web_contents,
                                          base::Uuid preset_id) override;
  std::unique_ptr<mojom::CosmeticFilter> MakeCosmeticFilter(
      content::RenderFrameHost* frame) override;
  RuleManager* GetRuleManager() override;
  KnownRuleSourcesHandler* GetKnownSourcesHandler() override;
  StateAndLogs* GetStateAndLogs() override;
  StatsStore* GetStatsStore() override;

  // Implementing RuleServiceStorageDelegate
  std::string GetRulesIndexChecksum(RuleGroup group) override;
  void OnStorageDoneLoading(
      RuleServiceStorageDelegate::LoadResult load_result) override;

  // Implementing KeyedService
  void Shutdown() override;

  // Implementing RuleManager::Observer
  void OnExceptionListChanged(RuleGroup group,
                              RuleManager::ExceptionsList list) override;

 private:
  void MigrateOldStatsData(
      const RuleServiceStorageDelegate::LoadResult* load_result);

  void OnRulesIndexLoaded(RuleGroup group);
  void OnRulesIndexChanged(RuleGroup group);

  void AddRequestFilter(RuleGroup group);

  const std::unique_ptr<Client> client_;
  const raw_ptr<content::BrowserContext> context_;

  RuleSourceHandler::RulesCompiler rules_compiler_;
  std::string locale_;

  // We can't have one injection manager per rule group, because they all use
  // the same resources and we only want to provide one copy of the static
  // injections to the content injection module.
  std::optional<ContentInjectionProvider> content_injection_provider_;

  std::optional<StateAndLogsImpl> state_and_logs_;
  std::optional<RuleServiceStorage> state_store_;
  std::optional<StatsStoreImpl> stats_store_;
  std::optional<Resources> resources_;

  bool is_loaded_ = false;
  std::optional<RuleManagerImpl> rule_manager_;
  std::optional<KnownRuleSourcesHandlerImpl> known_sources_handler_;

  RuleGroupArray<std::optional<RulesIndexManager>> index_managers_;

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  base::ObserverList<RuleService::Observer> observers_;

  base::WeakPtrFactory<RuleServiceImpl> weak_factory_{this};
};

}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_RULE_SERVICE_IMPL_H_
