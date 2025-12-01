// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_PUBLIC_CONTENT_ADBLOCK_STATE_AND_LOGS_H_
#define COMPONENTS_AD_BLOCKER_PUBLIC_CONTENT_ADBLOCK_STATE_AND_LOGS_H_

#include <map>
#include <set>
#include <string>

#include "base/observer_list_types.h"
#include "base/values.h"
#include "components/ad_blocker/public/content/adblock_navigation_tracker.h"
#include "components/ad_blocker/public/content/adblock_tab_state_and_logs.h"
#include "components/ad_blocker/public/core/adblock_types.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace adblock_filter {
class TabStateAndLogs;
class StateAndLogs {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override;

    virtual void OnNewBlockedUrlsReported(
        RuleGroup group,
        std::set<content::WebContents*> tabs_with_new_blocks) {}

    virtual void OnAllowAttributionChanged(content::WebContents* web_contents) {
    }
    virtual void OnNewAttributionTrackerAllowed(
        std::set<content::WebContents*> tabs_with_new_attribution_trackers) {}
  };
  using TrackerInfo = std::map<uint32_t, base::Value>;

  virtual ~StateAndLogs();

  virtual const TrackerInfo* GetTrackerInfo(
      RuleGroup group,
      const std::string& domain) const = 0;

  virtual void CreateTabHelper(content::WebContents* contents) = 0;
  virtual TabStateAndLogs* GetTabHelper(
      content::WebContents* contents) const = 0;
  virtual NavigationTracker* GetNavigationTracker(
      content::NavigationHandle& navigation_handle) const = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_PUBLIC_CONTENT_ADBLOCK_STATE_AND_LOGS_H_
