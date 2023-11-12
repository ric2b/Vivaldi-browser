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
#include "components/ad_blocker/adblock_rule_manager_impl.h"
#include "components/ad_blocker/adblock_rule_service.h"
#include "components/ad_blocker/adblock_rule_service_storage.h"
#include "components/ad_blocker/adblock_rule_source_handler.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ios/ad_blocker/adblock_content_rule_list_provider.h"
#include "ios/web/web_state/ui/wk_web_view_configuration_provider_observer.h"

namespace base {
class SequencedTaskRunner;
}

namespace web {
class BrowserState;
}

namespace adblock_filter {
class RuleServiceImpl : public RuleService,
                        public RuleManager::Observer,
                        public web::WKWebViewConfigurationProviderObserver {
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
  std::string GetRulesIndexChecksum(RuleGroup group) override;
  RuleManager* GetRuleManager() override;
  KnownRuleSourcesHandler* GetKnownSourcesHandler() override;
  BlockedUrlsReporter* GetBlockerUrlsReporter() override;

  // Implementing KeyedService
  void Shutdown() override;

  // Implementing RuleManager::Observer
  void OnExceptionListChanged(RuleGroup group,
                              RuleManager::ExceptionsList list) override;

  // Implementing WKWebViewConfigurationProviderObserver
  void DidCreateNewConfiguration(
      web::WKWebViewConfigurationProvider* config_provider,
      WKWebViewConfiguration* new_config) override;

 private:
  void OnStateLoaded(RuleServiceStorage::LoadResult load_result);

  web::BrowserState* browser_state_;
  RuleSourceHandler::RulesCompiler rules_compiler_;
  std::string locale_;

  absl::optional<RuleServiceStorage> state_store_;

  bool is_loaded_ = false;
  absl::optional<RuleManagerImpl> rule_manager_;
  absl::optional<KnownRuleSourcesHandlerImpl> known_sources_handler_;
  absl::optional<AdBlockerContentRuleListProvider> content_rule_list_provider_;

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  base::ObserverList<RuleService::Observer> observers_;
};

}  // namespace adblock_filter

#endif  // IOS_AD_BLOCKER_ADBLOCK_RULE_SERVICE_IMPL_H_
