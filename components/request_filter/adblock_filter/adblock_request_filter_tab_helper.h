// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_BLOCKED_URL_REPORTER_TAB_HELPER_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_BLOCKED_URL_REPORTER_TAB_HELPER_H_

#include <set>

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/ad_blocker/adblock_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace adblock_filter {
class RequestFilterTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<RequestFilterTabHelper> {
 public:
  struct BlockedUrlInfo {
    int blocked_count = 0;

    // TODO(julien): Add informations about which rule blocked it.
  };
  using BlockedUrlInfoMap = std::map<std::string, BlockedUrlInfo>;

  struct BlockedTrackerInfo {
    BlockedTrackerInfo();
    ~BlockedTrackerInfo();
    BlockedTrackerInfo(BlockedTrackerInfo&& other);

    BlockedUrlInfoMap blocked_urls;
    int blocked_count = 0;
  };

  struct TabBlockedUrlInfo {
    TabBlockedUrlInfo();
    ~TabBlockedUrlInfo();

    TabBlockedUrlInfo(TabBlockedUrlInfo&& other);
    TabBlockedUrlInfo& operator=(TabBlockedUrlInfo&& other);

    int total_count = 0;
    BlockedUrlInfoMap blocked_urls;
    std::map<std::string, BlockedTrackerInfo> blocked_trackers;
  };

  ~RequestFilterTabHelper() override;
  RequestFilterTabHelper(const RequestFilterTabHelper&) = delete;
  RequestFilterTabHelper& operator=(const RequestFilterTabHelper&) = delete;

  void SetFrameBlockState(RuleGroup group,
                          content::FrameTreeNodeId frame_tree_node_id);
  void ResetFrameBlockState(RuleGroup group,
                            content::FrameTreeNodeId frame_tree_node_id);
  void OnUrlBlocked(RuleGroup group, GURL url);
  void OnTrackerBlocked(RuleGroup group,
                        const std::string& domain,
                        const GURL& url);

  void SetAdAttributionState(bool enabled);
  void SetAdQueryTriggers(const GURL& ad_url,
                          std::vector<std::string> triggers);
  bool DoesAdAttributionMatch(std::string_view tracker_url_spec,
                              std::string_view ad_domain_and_query_trigger);
  const std::string& current_ad_landing_domain() {
    return current_ad_landing_domain_;
  }
  const std::set<std::string>& allowed_attribution_trackers() {
    return allowed_attribution_trackers_;
  }
  bool is_on_ad_landing_site() { return is_on_ad_landing_site_; }

  const TabBlockedUrlInfo& GetBlockedUrlsInfo(RuleGroup group);
  bool WasFrameBlocked(RuleGroup group,
                       content::FrameTreeNodeId frame_tree_node_id);

  // content::WebContentsObserver implementation
  void FrameDeleted(content::FrameTreeNodeId frame_tree_node_id) override;
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

 private:
  friend class content::WebContentsUserData<RequestFilterTabHelper>;
  explicit RequestFilterTabHelper(content::WebContents* contents);

  void DoQueryTriggerCheck(const GURL& url);
  void ResetAdAttribution();
  void SetIsOnAdLandingSite(bool is_on_ad_landing_site);

  std::array<std::set<content::FrameTreeNodeId>, kRuleGroupCount>
      blocked_frames_;
  std::set<std::string> allowed_attribution_trackers_;
  std::set<std::string> new_allowed_attribution_trackers_;

  std::set<int64_t> ongoing_navigations_;
  std::array<TabBlockedUrlInfo, kRuleGroupCount> blocked_urls_;
  std::array<TabBlockedUrlInfo, kRuleGroupCount> new_blocked_urls_;

  // Should we check if the next load is an ad?
  bool ad_attribution_enabled_;
  bool new_ad_attribution_enabled_;

  // Informations related to clicked ad.
  std::string current_ad_click_domain_;
  std::vector<std::string> ad_query_triggers_;
  base::TimeTicks ad_click_time_;

  // Ad attribution settings, once a trigger was matched.
  std::string current_ad_trigger_;
  std::string current_ad_landing_domain_;
  base::TimeTicks last_attributed_navigation_;
  bool is_on_ad_landing_site_ = false;
  base::OneShotTimer ad_attribution_expiration_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};
}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_BLOCKED_URL_REPORTER_TAB_HELPER_H_
