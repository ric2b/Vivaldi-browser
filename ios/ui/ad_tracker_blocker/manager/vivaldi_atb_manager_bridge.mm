// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ad_tracker_blocker/manager/vivaldi_atb_manager_bridge.h"

#import <Foundation/Foundation.h>

#import "base/check.h"
#import "base/notreached.h"
#import "ios/ad_blocker/adblock_rule_service_factory.h"


#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace vivaldi_adblocker {

VivaldiATBManagerBridge::VivaldiATBManagerBridge(
    id<VivaldiATBConsumer> observer, RuleService* ruleService)
    : observer_(observer), rule_service_(ruleService) {
  DCHECK(observer_);
  DCHECK(rule_service_);
  rule_service_->AddObserver(this);
  if (rule_service_->IsLoaded()) {
    this->StartObservingRuleSourceManager();
  }
}

VivaldiATBManagerBridge::~VivaldiATBManagerBridge() {
  if (rule_service_) {
    if (rule_service_->IsLoaded()) {
      rule_service_->GetRuleManager()->RemoveObserver(this);
      rule_service_->GetKnownSourcesHandler()->RemoveObserver(this);
    }
    rule_service_->RemoveObserver(this);
  }
}

void VivaldiATBManagerBridge::OnRuleServiceStateLoaded(
  RuleService* rule_service) {
  this->StartObservingRuleSourceManager();
}

void VivaldiATBManagerBridge::OnStartApplyingIosRules(RuleGroup group) {
  id<VivaldiATBConsumer> observer = observer_;
  if (!observer)
    return;
  [observer rulesListDidStartApplying:group];
}

void VivaldiATBManagerBridge::OnDoneApplyingIosRules(RuleGroup group) {
  id<VivaldiATBConsumer> observer = observer_;
  if (!observer)
    return;
  [observer rulesListDidEndApplying:group];
}

void VivaldiATBManagerBridge::OnRuleSourceUpdated(RuleGroup group,
   const ActiveRuleSource& rule_source) {
  id<VivaldiATBConsumer> observer = observer_;
  if (!observer)
    return;
  ATBFetchResult fetchResult =
      this->FlattenFetchResult(rule_source.last_fetch_result);
  [observer ruleSourceDidUpdate:rule_source.core.id()
                          group:group
                    fetchResult:fetchResult];
}

void VivaldiATBManagerBridge::OnRuleSourceDeleted(uint32_t source_id,
                                                  RuleGroup group) {
  id<VivaldiATBConsumer> observer = observer_;
  if (!observer)
    return;
  [observer ruleSourceDidRemove:source_id
                          group:group];
}

void VivaldiATBManagerBridge::OnExceptionListStateChanged(RuleGroup group) {
  id<VivaldiATBConsumer> observer = observer_;
  if (!observer)
    return;
  [observer exceptionListStateDidChange:group];
}

void VivaldiATBManagerBridge::OnExceptionListChanged(
   RuleGroup group,
   RuleManager::ExceptionsList list) {

  id<VivaldiATBConsumer> observer = observer_;
  if (!observer)
    return;
  [observer exceptionListDidChange:group list:list];
}

void VivaldiATBManagerBridge::OnKnownSourceAdded(
  RuleGroup group,
  const KnownRuleSource& rule_source) {
  id<VivaldiATBConsumer> observer = observer_;
  if (!observer)
    return;
  [observer knownSourceDidAdd:group key:rule_source.core.id()];
}

void VivaldiATBManagerBridge::OnKnownSourceRemoved(RuleGroup group,
                                                   uint32_t source_id) {
  id<VivaldiATBConsumer> observer = observer_;
  if (!observer)
    return;
  [observer knownSourceDidRemove:group key:source_id];
}

void VivaldiATBManagerBridge::OnKnownSourceEnabled(RuleGroup group,
                                                   uint32_t source_id) {
  id<VivaldiATBConsumer> observer = observer_;
  if (!observer)
    return;
  [observer knownSourceDidEnable:group key:source_id];
}

void VivaldiATBManagerBridge::OnKnownSourceDisabled(RuleGroup group,
                                                    uint32_t source_id) {
  id<VivaldiATBConsumer> observer = observer_;
  if (!observer)
    return;
  [observer knownSourceDidDisable:group key:source_id];
}

void VivaldiATBManagerBridge::StartObservingRuleSourceManager() {
  if (rule_service_) {
    rule_service_->GetRuleManager()->AddObserver(this);
    rule_service_->GetKnownSourcesHandler()->AddObserver(this);

    id<VivaldiATBConsumer> observer = observer_;
    if (!observer)
      return;
    [observer ruleServiceStateDidLoad];
  }
}

ATBFetchResult VivaldiATBManagerBridge::FlattenFetchResult(FetchResult
                                                           fetchResult) {
  switch (fetchResult) {
    case FetchResult::kSuccess:
      return FetchResultSuccess;
    case FetchResult::kDownloadFailed:
      return FetchResultDownloadFailed;
    case FetchResult::kFileNotFound:
      return FetchResultFileNotFound;
    case FetchResult::kFileReadError:
      return FetchResultFileReadError;
    case FetchResult::kFileUnsupported:
      return FetchResultFileUnsupported;
    case FetchResult::kFailedSavingParsedRules:
      return FetchResultFailedSavingParsedRules;
    case FetchResult::kUnknown:
      return FetchResultUnknown;
    default:
      return FetchResultUnknown;
  }
}

}  // namespace vivaldi_adblocker
