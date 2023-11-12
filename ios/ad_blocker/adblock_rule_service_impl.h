// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_AD_BLOCKER_ADBLOCK_RULE_SERVICE_IMPL_H_
#define IOS_AD_BLOCKER_ADBLOCK_RULE_SERVICE_IMPL_H_

#include <array>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/ad_blocker/adblock_known_sources_handler_impl.h"
#include "components/ad_blocker/adblock_metadata.h"
#include "components/ad_blocker/adblock_resources.h"
#include "components/ad_blocker/adblock_rule_manager_impl.h"
#include "components/ad_blocker/adblock_rule_service.h"
#include "components/ad_blocker/adblock_rule_service_storage.h"
#include "components/ad_blocker/adblock_rule_source_handler.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ios/ad_blocker/adblock_organized_rules_manager.h"

namespace base {
class SequencedTaskRunner;
}

namespace web {
class BrowserState;
}

namespace adblock_filter {
class ContentInjectionHandler;
class RuleServiceImpl : public RuleService, public RuleManager::Observer {
 public:
  explicit RuleServiceImpl(web::BrowserState* browser_state,
                           RuleSourceHandler::RulesCompiler rules_compiler,
                           std::string locale);
  ~RuleServiceImpl() override;
  RuleServiceImpl(const RuleServiceImpl&) = delete;
  RuleServiceImpl& operator=(const RuleServiceImpl&) = delete;

  void Load();

  // Implementing RuleService
  bool IsLoaded() const override;
  bool IsRuleGroupEnabled(RuleGroup group) const override;
  void SetRuleGroupEnabled(RuleGroup group, bool enabled) override;
  void AddObserver(RuleService::Observer* observer) override;
  void RemoveObserver(RuleService::Observer* observer) override;
  bool IsApplyingIosRules(RuleGroup group) override;
  std::string GetRulesIndexChecksum(RuleGroup group) override;
  IndexBuildResult GetRulesIndexBuildResult(RuleGroup group) override;
  RuleManager* GetRuleManager() override;
  KnownRuleSourcesHandler* GetKnownSourcesHandler() override;
  BlockedUrlsReporter* GetBlockerUrlsReporter() override;

  // Implementing KeyedService
  void Shutdown() override;

  // Implementing RuleManager::Observer
  void OnExceptionListChanged(RuleGroup group,
                              RuleManager::ExceptionsList list) override;

 private:
  struct LoadData;

  void OnStateLoaded(std::unique_ptr<LoadData> load_data);

  void OnRulesIndexChanged(RuleGroup group,
                           RuleService::IndexBuildResult build_result);

  void OnStartApplyingRules(RuleGroup group);
  void OnDoneApplyingRules(RuleGroup group);

  web::BrowserState* browser_state_;
  RuleSourceHandler::RulesCompiler rules_compiler_;
  std::string locale_;

  absl::optional<RuleServiceStorage> state_store_;

  bool is_loaded_ = false;
  absl::optional<RuleManagerImpl> rule_manager_;
  absl::optional<KnownRuleSourcesHandlerImpl> known_sources_handler_;
  std::array<absl::optional<OrganizedRulesManager>, kRuleGroupCount>
      organized_rules_manager_;

  absl::optional<Resources> resources_;
  std::unique_ptr<ContentInjectionHandler> content_injection_handler_;

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  base::ObserverList<RuleService::Observer> observers_;

  base::WeakPtrFactory<RuleServiceImpl> weak_ptr_factory_{this};
};

}  // namespace adblock_filter

#endif  // IOS_AD_BLOCKER_ADBLOCK_RULE_SERVICE_IMPL_H_
