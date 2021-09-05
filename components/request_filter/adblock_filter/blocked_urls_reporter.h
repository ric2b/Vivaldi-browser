// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_BLOCKED_URLS_REPORTER_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_BLOCKED_URLS_REPORTER_H_

#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/request_filter/adblock_filter/adblock_metadata.h"
#include "url/gurl.h"

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
  using BlockedDomains =
      std::array<std::map<std::string, int>, kRuleGroupCount>;
  using TrackerInfo = std::map<uint32_t, base::Value>;

  BlockedUrlsReporter(BlockedDomains blocked_domains,
                      base::RepeatingClosure schedule_save);
  ~BlockedUrlsReporter();

  base::WeakPtr<BlockedUrlsReporter> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void OnTrackerInfosUpdated(const RuleSource& source,
                             base::Value new_tracker_infos);

  void OnUrlBlocked(RuleGroup group, GURL url, content::RenderFrameHost* frame);
  void OnTabRemoved(content::WebContents* contents);

  const TrackerInfo* GetTrackerInfo(RuleGroup group, const std::string& domain);
  const BlockedDomains& GetBlockedDomains() { return blocked_domains_; }
  void ClearBlockedDomains();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  void NotifyOfNewBlockedUrls();
  void AddToBlockedDomains(RuleGroup group, std::string domain);

  std::array<std::set<content::WebContents*>, kRuleGroupCount>
      tabs_with_new_blocks_;

  std::array<std::map<std::string, TrackerInfo>, kRuleGroupCount>
      tracker_infos_;

  BlockedDomains blocked_domains_;

  base::Time last_notification_time_;
  base::OneShotTimer next_notifiaction_timer_;
  base::RepeatingClosure schedule_save_;

  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<BlockedUrlsReporter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlockedUrlsReporter);
};
}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_BLOCKED_URLS_REPORTER_H_
