// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_STATE_AND_LOGS_IMPL_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_STATE_AND_LOGS_IMPL_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/ad_blocker/adblock_types.h"
#include "components/request_filter/adblock_filter/adblock_rules_index.h"
#include "components/request_filter/adblock_filter/adblock_state_and_logs.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace adblock_filter {
class StateAndLogsImpl : public StateAndLogs {
 public:
  StateAndLogsImpl(base::Time reporting_start,
                   CounterGroup blocked_domains,
                   CounterGroup blocked_for_origin,
                   base::RepeatingClosure schedule_save);
  ~StateAndLogsImpl() override;
  StateAndLogsImpl(const StateAndLogsImpl&) = delete;
  StateAndLogsImpl& operator=(const StateAndLogsImpl&) = delete;

  base::WeakPtr<StateAndLogsImpl> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void OnTrackerInfosUpdated(RuleGroup group,
                             const ActiveRuleSource& source,
                             base::Value::Dict new_tracker_infos);

  void SetFrameBlockState(RuleGroup group, content::RenderFrameHost* frame);
  void ResetFrameBlockState(RuleGroup group, content::RenderFrameHost* frame);

  void LogTabActivations(RuleGroup group,
                         content::RenderFrameHost* frame,
                         const RulesIndex::ActivationResults& activations);

  void OnUrlBlocked(RuleGroup group,
                    url::Origin origin,
                    GURL url,
                    content::RenderFrameHost* frame);
  void OnTabRemoved(content::WebContents* contents);
  void OnAllowAttributionChanged(content::WebContents* contents);

  void ArmAdAttribution(content::RenderFrameHost* frame);

  void SetTabAdQueryTriggers(const GURL& ad_url,
                             std::vector<std::string> ad_query_triggers,
                             content::RenderFrameHost* frame);
  bool DoesAdAttributionMatch(content::RenderFrameHost* frame,
                              std::string_view tracker_url_spec,
                              std::string_view ad_domain_and_query_trigger);

  // StateAndLogs implementation
  const TrackerInfo* GetTrackerInfo(RuleGroup group,
                                    const std::string& domain) const override;
  const CounterGroup& GetBlockedDomainCounters() const override {
    return blocked_domains_;
  }
  const CounterGroup& GetBlockedForOriginCounters() const override {
    return blocked_for_origin_;
  }
  base::Time GetBlockedCountersStart() const override {
    return reporting_start_;
  }
  void ClearBlockedCounters() override;
  bool WasFrameBlocked(RuleGroup group,
                       content::RenderFrameHost* frame) const override;
  TabStateAndLogs* GetTabHelper(content::WebContents* contents) const override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 private:
  void PrepareNewNotifications();
  void SendNotifications();
  void AddToCounter(CounterGroup& counter_group,
                    RuleGroup group,
                    std::string domain);

  std::array<std::set<content::WebContents*>, kRuleGroupCount>
      tabs_with_new_blocks_;
  std::set<content::WebContents*> tabs_with_new_attribution_trackers_;

  std::array<std::map<std::string, TrackerInfo>, kRuleGroupCount>
      tracker_infos_;

  base::Time reporting_start_;
  CounterGroup blocked_domains_;
  CounterGroup blocked_for_origin_;

  base::Time last_notification_time_;
  base::OneShotTimer next_notification_timer_;
  base::RepeatingClosure schedule_save_;

  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<StateAndLogsImpl> weak_factory_;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_STATE_AND_LOGS_IMPL_H_
