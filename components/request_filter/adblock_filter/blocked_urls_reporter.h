// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_BLOCKED_URLS_REPORTER_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_BLOCKED_URLS_REPORTER_H_

#include <set>
#include <vector>

#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/ad_blocker/adblock_metadata.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace adblock_filter {
class BlockedUrlsReporter {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override;

    virtual void OnNewBlockedUrlsReported(
        RuleGroup group,
        std::set<content::WebContents*> tabs_with_new_blocks) {}
  };
  using CounterGroup = std::array<std::map<std::string, int>, kRuleGroupCount>;
  using TrackerInfo = std::map<uint32_t, base::Value>;

  BlockedUrlsReporter(base::Time reporting_start,
                      CounterGroup blocked_domains,
                      CounterGroup blocked_for_origin,
                      base::RepeatingClosure schedule_save);
  ~BlockedUrlsReporter();
  BlockedUrlsReporter(const BlockedUrlsReporter&) = delete;
  BlockedUrlsReporter& operator=(const BlockedUrlsReporter&) = delete;

  base::WeakPtr<BlockedUrlsReporter> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void OnTrackerInfosUpdated(const RuleSource& source,
                             base::Value::Dict new_tracker_infos);

  void OnUrlBlocked(RuleGroup group,
                    url::Origin origin,
                    GURL url,
                    content::RenderFrameHost* frame);
  void OnTabRemoved(content::WebContents* contents);

  const TrackerInfo* GetTrackerInfo(RuleGroup group,
                                    const std::string& domain) const;
  const CounterGroup& GetBlockedDomains() const { return blocked_domains_; }
  const CounterGroup& GetBlockedForOrigin() const {
    return blocked_for_origin_;
  }
  base::Time GetReportingStart() const { return reporting_start_; }
  void ClearBlockedCounters();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  void NotifyOfNewBlockedUrls();
  void AddToCounter(CounterGroup& counter_group,
                    RuleGroup group,
                    std::string domain);

  std::array<std::set<content::WebContents*>, kRuleGroupCount>
      tabs_with_new_blocks_;

  std::array<std::map<std::string, TrackerInfo>, kRuleGroupCount>
      tracker_infos_;

  base::Time reporting_start_;
  CounterGroup blocked_domains_;
  CounterGroup blocked_for_origin_;

  base::Time last_notification_time_;
  base::OneShotTimer next_notifiaction_timer_;
  base::RepeatingClosure schedule_save_;

  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<BlockedUrlsReporter> weak_factory_;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_BLOCKED_URLS_REPORTER_H_
