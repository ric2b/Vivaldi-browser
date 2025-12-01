// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_IOS_ADBLOCK_RULE_SERVICE_IMPL_H_
#define COMPONENTS_AD_BLOCKER_IOS_ADBLOCK_RULE_SERVICE_IMPL_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/ad_blocker/core/adblock_known_sources_handler_impl.h"
#include "components/ad_blocker/core/adblock_resources.h"
#include "components/ad_blocker/core/adblock_rule_manager_impl.h"
#include "components/ad_blocker/core/adblock_rule_service_storage.h"
#include "components/ad_blocker/core/adblock_rule_source_handler.h"
#include "components/ad_blocker/ios/adblock_organized_rules_manager.h"
#include "components/ad_blocker/public/core/adblock_types.h"
#include "components/ad_blocker/public/ios/adblock_rule_service.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace base {
class SequencedTaskRunner;
}

namespace web {
class BrowserState;
}

namespace adblock_filter {
class ContentInjectionHandler;
class RuleServiceImpl : public RuleService,
                        public RuleServiceStorageDelegate,
                        public RuleManager::Observer {
 public:
  RuleServiceImpl(web::BrowserState* browser_state,
                  PrefService* prefs,
                  RuleSourceHandler::RulesCompiler rules_compiler,
                  std::string locale);
  ~RuleServiceImpl() override;
  RuleServiceImpl(const RuleServiceImpl&) = delete;
  RuleServiceImpl& operator=(const RuleServiceImpl&) = delete;

  void Load();

  // Implementing RuleService
  bool IsLoaded() const override;
  void AddObserver(RuleService::Observer* observer) override;
  void RemoveObserver(RuleService::Observer* observer) override;
  bool IsApplyingRules(RuleGroup group) override;
  void SetIncognitoBrowserState(web::BrowserState* browser_state) override;
  bool IsPartnerListAllowedDocument(RuleGroup group, GURL url) override;
  IndexBuildResult GetRulesIndexBuildResult(RuleGroup group) override;
  RuleManager* GetRuleManager() override;
  KnownRuleSourcesHandler* GetKnownSourcesHandler() override;

  // Implementing RuleServiceStorageDelegate
  void OnStorageDoneLoading(
      RuleServiceStorageDelegate::LoadResult load_result) override;
  std::string GetRulesIndexChecksum(RuleGroup group) override;

  // Implementing KeyedService
  void Shutdown() override;

  // Implementing RuleManager::Observer
  void OnExceptionListChanged(RuleGroup group,
                              RuleManager::ExceptionsList list) override;

 private:
  void OnFullyLoaded(
      RuleGroupArray<std::unique_ptr<AdBlockerContentRuleListProvider>>
          loaded_content_rule_list_providers,
      RuleServiceStorageDelegate::LoadResult load_result);
  void OnRulesIndexChanged(RuleGroup group,
                           RuleService::IndexBuildResult build_result);

  void OnStartApplyingRules(RuleGroup group);
  void OnDoneApplyingRules(RuleGroup group);

  void OnEnableDocumentBlockingChanged();

  const raw_ptr<web::BrowserState> browser_state_;
  raw_ptr<web::BrowserState> incognito_browser_state_ = nullptr;
  const raw_ptr<PrefService> prefs_;
  PrefChangeRegistrar pref_change_registrar_;

  RuleSourceHandler::RulesCompiler rules_compiler_;
  std::string locale_;

  std::optional<RuleServiceStorage> state_store_;

  base::OnceCallback<void(RuleServiceStorageDelegate::LoadResult)>
      on_storage_loaded_;
  bool is_loaded_ = false;
  std::optional<RuleManagerImpl> rule_manager_;
  std::optional<KnownRuleSourcesHandlerImpl> known_sources_handler_;
  RuleGroupArray<std::optional<OrganizedRulesManager>> organized_rules_manager_;

  std::optional<Resources> resources_;
  std::unique_ptr<ContentInjectionHandler> content_injection_handler_;

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  base::ObserverList<RuleService::Observer> observers_;

  base::WeakPtrFactory<RuleServiceImpl> weak_ptr_factory_{this};
};

}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_IOS_ADBLOCK_RULE_SERVICE_IMPL_H_
