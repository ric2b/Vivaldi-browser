// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_BLOCKED_URL_REPORTER_TAB_HELPER_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_BLOCKED_URL_REPORTER_TAB_HELPER_H_

#include <set>

#include "components/ad_blocker/adblock_metadata.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace adblock_filter {
class BlockedUrlsReporterTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<BlockedUrlsReporterTabHelper> {
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

  ~BlockedUrlsReporterTabHelper() override;
  BlockedUrlsReporterTabHelper(const BlockedUrlsReporterTabHelper&) = delete;
  BlockedUrlsReporterTabHelper& operator=(const BlockedUrlsReporterTabHelper&) =
      delete;

  void OnUrlBlocked(RuleGroup group, GURL url);
  void OnTrackerBlocked(RuleGroup group,
                        const std::string& domain,
                        const GURL& url);

  const TabBlockedUrlInfo& GetBlockedUrlsInfo(RuleGroup group);

  // content::WebContentsObserver implementation
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

 private:
  friend class content::WebContentsUserData<BlockedUrlsReporterTabHelper>;
  explicit BlockedUrlsReporterTabHelper(content::WebContents* contents);

  std::set<int64_t> ongoing_navigations_;
  std::array<TabBlockedUrlInfo, kRuleGroupCount> blocked_urls_;
  std::array<TabBlockedUrlInfo, kRuleGroupCount> new_blocked_urls_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};
}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_BLOCKED_URL_REPORTER_TAB_HELPER_H_
