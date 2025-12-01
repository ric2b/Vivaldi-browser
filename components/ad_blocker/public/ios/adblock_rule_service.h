// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_PUBLIC_IOS_ADBLOCK_RULE_SERVICE_H_
#define COMPONENTS_AD_BLOCKER_PUBLIC_IOS_ADBLOCK_RULE_SERVICE_H_

#include <memory>

#include "base/observer_list_types.h"
#include "components/ad_blocker/public/core/adblock_rule_service_core.h"
#include "components/ad_blocker/public/core/adblock_types.h"
#include "url/origin.h"

namespace web {
class BrowserState;
}

namespace adblock_filter {

class RuleService : public RuleServiceCore {
 public:
  enum IndexBuildResult {
    kBuildSuccess = 0,
    kTooManyAllowRules = 1,
  };

  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override;
    virtual void OnRuleServiceStateLoaded(RuleService* rule_service) {}

    virtual void OnRulesIndexBuilt(RuleGroup group, IndexBuildResult status) {}
    virtual void OnStartApplyingRules(RuleGroup group) {}
    virtual void OnDoneApplyingRules(RuleGroup group) {}
  };

  ~RuleService() override;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  virtual bool IsApplyingRules(RuleGroup group) = 0;

  virtual void SetIncognitoBrowserState(web::BrowserState* browser_state) = 0;

  // Reports whether the provided URL matches a document allow rule on our
  // partner list. This is only a rough match and may report incorrect result.
  virtual bool IsPartnerListAllowedDocument(RuleGroup group, GURL url) = 0;

  virtual IndexBuildResult GetRulesIndexBuildResult(RuleGroup group) = 0;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_PUBLIC_IOS_ADBLOCK_RULE_SERVICE_H_
