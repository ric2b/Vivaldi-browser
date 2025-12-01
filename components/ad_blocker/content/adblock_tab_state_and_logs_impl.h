// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_TAB_STATE_AND_LOGS_IMPL_H_
#define COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_TAB_STATE_AND_LOGS_IMPL_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/ad_blocker/content/index/adblock_rules_index.h"
#include "components/ad_blocker/public/content/adblock_tab_state_and_logs.h"
#include "components/ad_blocker/public/core/adblock_request_filter_rule_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace adblock_filter {
class StateAndLogsImpl;

class TabStateAndLogsImpl
    : public TabStateAndLogs,
      public content::WebContentsObserver,
      public content::WebContentsUserData<TabStateAndLogsImpl> {
 public:
  ~TabStateAndLogsImpl() override;
  TabStateAndLogsImpl(const TabStateAndLogsImpl&) = delete;
  TabStateAndLogsImpl& operator=(const TabStateAndLogsImpl&) = delete;

  void OnUrlBlocked(RuleGroup group, GURL url);
  void OnTrackerBlocked(RuleGroup group,
                        const std::string& domain,
                        const GURL& url);
  void OnMatchedAttributionTracker(const GURL& url);

  void SetAdQueryTriggers(const GURL& ad_url,
                          std::vector<std::string> triggers);

  std::optional<RulesIndex::AdAttributionMatchParams>
  GetAdAttributionMatchParams() const;

  // TabStateAndLogs implementation
  const std::string& GetCurrentAdLandingDomain() const override;
  const std::set<std::string>& GetAllowedAttributionTrackers() const override;
  bool IsOnAdLandingSite() const override;
  const TabBlockedUrlInfo& GetBlockedUrlsInfo(RuleGroup group) const override;
  void MaybeAddNavigationThrottle(
      content::NavigationThrottleRegistry& registry) const override;

 private:
  friend class content::WebContentsUserData<TabStateAndLogsImpl>;
  class PotentialPopupRecord;

  struct ActivationsDetails {
    GURL determined_for_url;
    ActivationResults activations;
  };

  TabStateAndLogsImpl(content::WebContents* contents,
                      base::WeakPtr<StateAndLogsImpl> state_and_logs);

  void DoQueryTriggerCheck(const GURL& url);
  void ResetAdAttribution();
  void SetIsOnAdLandingSite(bool is_on_ad_landing_site);

  void SetPotentialPopup(RuleGroup group,
                         GURL initial_url,
                         bool initial_disable_generic_rules_for_popup_check,
                         bool initial_disable_generic_rules_for_popunder_check,
                         content::WebContents* opener,
                         content::RenderFrameHost* opener_frame,
                         base::OnceClosure on_destruct);

  void UpdatePotentialPopup(RuleGroup group,
                            content::NavigationHandle* navigation_handle);

  // content::WebContentsObserver implementation
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override;
  void WebContentsDestroyed() override;

  const base::WeakPtr<StateAndLogsImpl> state_and_logs_;

  std::set<std::string> allowed_attribution_trackers_;
  std::set<std::string> new_allowed_attribution_trackers_;

  bool has_ongoing_navigation_ = false;
  RuleGroupArray<TabBlockedUrlInfo> blocked_urls_;
  RuleGroupArray<TabBlockedUrlInfo> new_blocked_urls_;

  // Should we check if the next load is an ad?
  bool ad_attribution_enabled_ = false;

  // Information related to clicked ad.
  std::string current_ad_click_domain_;
  std::vector<std::string> ad_query_triggers_;
  base::TimeTicks ad_click_time_;

  // Ad attribution settings, once a trigger was matched.
  std::string current_ad_trigger_;
  std::string current_ad_landing_domain_;
  base::TimeTicks last_attributed_navigation_;
  bool is_on_ad_landing_site_ = false;
  base::OneShotTimer ad_attribution_expiration_;

  // Popup detection
  RuleGroupArray<std::unique_ptr<PotentialPopupRecord>> potential_popup_record_;
  RuleGroupArray<std::set<raw_ptr<content::WebContents>>> potential_popups_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};
}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_TAB_STATE_AND_LOGS_IMPL_H_
