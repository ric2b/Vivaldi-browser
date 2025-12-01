// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_PUBLIC_CONTENT_ADBLOCK_TAB_STATE_AND_LOGS_H_
#define COMPONENTS_AD_BLOCKER_PUBLIC_CONTENT_ADBLOCK_TAB_STATE_AND_LOGS_H_

#include <set>

#include "base/containers/flat_map.h"
#include "components/ad_blocker/public/core/adblock_request_filter_rule.h"
#include "components/ad_blocker/public/core/adblock_types.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/navigation_throttle_registry.h"

namespace content {
class WebContents;
}

namespace adblock_filter {
class TabStateAndLogs {
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

  struct RuleData {
    RequestFilterRule::Decision decision = RequestFilterRule::Decision::kPass;

    std::string_view rule_text;
    uint32_t rule_source_id;
  };

  struct TabActivationState {
    bool from_parent;
    std::optional<RuleData> rule_data;
  };

  struct TabActivations {
    bool document_exception;
    base::flat_map<RequestFilterRule::ActivationTypes, TabActivationState>
        by_type;
  };

  virtual ~TabStateAndLogs();

  virtual const std::string& GetCurrentAdLandingDomain() const = 0;
  virtual const std::set<std::string>& GetAllowedAttributionTrackers()
      const = 0;
  virtual bool IsOnAdLandingSite() const = 0;

  virtual const TabBlockedUrlInfo& GetBlockedUrlsInfo(
      RuleGroup group) const = 0;
  virtual std::array<std::optional<TabStateAndLogs::RuleData>, kRuleGroupCount>
  WasNavigationBlocked(const content::NavigationHandle* navigation) const = 0;

  virtual TabActivations GetTabActivations(RuleGroup group) const = 0;

  virtual void MaybeAddNavigationThrottle(
      content::NavigationThrottleRegistry& registry) const = 0;
};
}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_PUBLIC_CONTENT_ADBLOCK_TAB_STATE_AND_LOGS_H_
