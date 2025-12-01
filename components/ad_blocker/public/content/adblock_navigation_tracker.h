// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_PUBLIC_CONTENT_ADBLOCK_NAVIGATION_TRACKER_H_
#define COMPONENTS_AD_BLOCKER_PUBLIC_CONTENT_ADBLOCK_NAVIGATION_TRACKER_H_

#include "components/ad_blocker/public/core/adblock_request_filter_rule_types.h"
#include "components/ad_blocker/public/core/adblock_types.h"

namespace adblock_filter {
class NavigationTracker {
 public:
  virtual ~NavigationTracker();

  virtual const ActivationResults& GetActivations(RuleGroup group) = 0;
  virtual std::optional<std::pair<RuleGroup, RequestFilterRuleStub>>
  GetBlockedByRule() const = 0;
};
}  // namespace adblock_filter
#endif  // COMPONENTS_AD_BLOCKER_PUBLIC_CONTENT_ADBLOCK_NAVIGATION_TRACKER_H_
