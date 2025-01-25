// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_AD_TRACKER_BLOCKER_MANAGER_VIVALDI_ATB_MANAGER_BRIDGE_H_
#define IOS_UI_AD_TRACKER_BLOCKER_MANAGER_VIVALDI_ATB_MANAGER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/compiler_specific.h"
#import "components/ad_blocker/adblock_known_sources_handler.h"
#import "components/ad_blocker/adblock_rule_manager.h"
#import "components/ad_blocker/adblock_rule_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_consumer.h"

using adblock_filter::FetchResult;
using adblock_filter::KnownRuleSource;
using adblock_filter::KnownRuleSourcesHandler;
using adblock_filter::RuleManager;
using adblock_filter::RuleService;
using adblock_filter::ActiveRuleSource;

namespace vivaldi_adblocker {
// A bridge that translates AdBlocker Observers C++ callbacks into ObjC
// callbacks.
class VivaldiATBManagerBridge : public RuleService::Observer,
                                public RuleManager::Observer,
                                public KnownRuleSourcesHandler::Observer {
 public:
  explicit VivaldiATBManagerBridge(id<VivaldiATBConsumer> observer,
                                   RuleService* ruleService);
  ~VivaldiATBManagerBridge() override;

 private:
  void OnRuleServiceStateLoaded(RuleService* rule_service) override;
  void OnStartApplyingIosRules(RuleGroup group) override;
  void OnDoneApplyingIosRules(RuleGroup group) override;
  void OnRuleSourceUpdated(RuleGroup group,
                            const ActiveRuleSource& rule_source) override;
  void OnRuleSourceDeleted(uint32_t source_id, RuleGroup group) override;
  void OnExceptionListStateChanged(RuleGroup group) override;
  void OnExceptionListChanged(RuleGroup group,
                              RuleManager::ExceptionsList list) override;
  void OnKnownSourceAdded(RuleGroup group,
                          const KnownRuleSource& rule_source) override;
  void OnKnownSourceRemoved(RuleGroup group, uint32_t source_id) override;
  void OnKnownSourceEnabled(RuleGroup group, uint32_t source_id) override;
  void OnKnownSourceDisabled(RuleGroup group, uint32_t source_id) override;
  void StartObservingRuleSourceManager();
  ATBFetchResult FlattenFetchResult(FetchResult fetchResult);

  __weak id<VivaldiATBConsumer> observer_;
  adblock_filter::RuleService* rule_service_ = nullptr;
};
}  // namespace vivaldi_adblocker

#endif  // IOS_UI_AD_TRACKER_BLOCKER_MANAGER_VIVALDI_ATB_MANAGER_BRIDGE_H_
