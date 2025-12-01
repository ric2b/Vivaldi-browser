// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_NAVIGATION_TRACKER_IMPL_H_
#define COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_NAVIGATION_TRACKER_IMPL_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/ad_blocker/public/content/adblock_navigation_tracker.h"
#include "components/ad_blocker/public/core/adblock_request_filter_rule_types.h"
#include "components/ad_blocker/public/core/adblock_types.h"
#include "content/public/browser/navigation_handle_user_data.h"

namespace adblock_filter {
class StateAndLogsImpl;

class NavigationTrackerImpl
    : public content::NavigationHandleUserData<NavigationTrackerImpl>,
      public NavigationTracker {
 public:
  ~NavigationTrackerImpl() override;

  void OnBlockedByRule(RuleGroup group, const RequestFilterRuleStub& stub);

  const ActivationResults& GetActivations(RuleGroup group) override;
  std::optional<std::pair<RuleGroup, RequestFilterRuleStub>> GetBlockedByRule()
      const override;

  int64_t navigation_id() { return navigation_id_; }

 private:
  bool UpdateActivationsIfNeeded(RuleGroup group);

  friend NavigationHandleUserData;

  NavigationTrackerImpl(content::NavigationHandle& navigation_handle,
                        base::WeakPtr<StateAndLogsImpl> state_and_logs_);

  raw_ref<content::NavigationHandle> navigation_handle_;
  // We need this to be available at destruction time, when the rest of the
  // `navigation_handle_` is already gone.
  int64_t navigation_id_;
  const base::WeakPtr<StateAndLogsImpl> state_and_logs_;
  RuleGroupArray<GURL> determined_for_url_;
  RuleGroupArray<ActivationResults> activations_;

  std::optional<std::pair<RuleGroup, RequestFilterRuleStub>> blocked_by_rule_;

  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};
}  // namespace adblock_filter
#endif  // COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_NAVIGATION_TRACKER_IMPL_H_
