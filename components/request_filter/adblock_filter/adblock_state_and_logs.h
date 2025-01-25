// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_STATE_AND_LOGS_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_STATE_AND_LOGS_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/ad_blocker/adblock_types.h"
#include "url/gurl.h"
#include "url/origin.h"

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
  using CounterGroup = std::array<std::map<std::string, int>, kRuleGroupCount>;
  using TrackerInfo = std::map<uint32_t, base::Value>;

  virtual ~StateAndLogs();

  virtual const TrackerInfo* GetTrackerInfo(
      RuleGroup group,
      const std::string& domain) const = 0;

  virtual const CounterGroup& GetBlockedDomainCounters() const = 0;
  virtual const CounterGroup& GetBlockedForOriginCounters() const = 0;
  virtual base::Time GetBlockedCountersStart() const = 0;
  virtual void ClearBlockedCounters() = 0;

  virtual bool WasFrameBlocked(RuleGroup group,
                               content::RenderFrameHost* frame) const = 0;

  virtual TabStateAndLogs* GetTabHelper(
      content::WebContents* contents) const = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_STATE_AND_LOGS_H_
