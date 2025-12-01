// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_STATE_AND_LOGS_IMPL_H_
#define COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_STATE_AND_LOGS_IMPL_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/ad_blocker/content/index/adblock_rules_index.h"
#include "components/ad_blocker/public/content/adblock_state_and_logs.h"
#include "components/ad_blocker/public/core/adblock_request_filter_rule_types.h"
#include "components/ad_blocker/public/core/adblock_types.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace adblock_filter {
class RuleServiceImpl;
class TabStateAndLogsImpl;
class NavigationTrackerImpl;

class StateAndLogsImpl : public StateAndLogs {
 public:
  StateAndLogsImpl(RuleServiceImpl* rules_service);
  ~StateAndLogsImpl() override;
  StateAndLogsImpl(const StateAndLogsImpl&) = delete;
  StateAndLogsImpl& operator=(const StateAndLogsImpl&) = delete;

  std::optional<ActivationResults> GetLocalActivations(
      RuleGroup group,
      const url::Origin& parent_origin,
      const GURL& url);

  std::optional<RulesIndex::AdAttributionMatchParams>
  GetAdAttributionMatchParams(content::RenderFrameHost* frame) const;

  void OnTrackerInfosUpdated(RuleGroup group,
                             const ActiveRuleSource& source,
                             base::Value::Dict new_tracker_infos);

  void OnUrlBlocked(RuleGroup group,
                    url::Origin origin,
                    GURL url,
                    content::RenderFrameHost* frame);
  void OnTabRemoved(content::WebContents* contents);
  void OnAllowAttributionChanged(content::WebContents* contents);
  void OnMatchedAttributionTracker(content::RenderFrameHost* frame,
                                   const GURL& url);

  void SetTabAdQueryTriggers(const GURL& ad_url,
                             std::vector<std::string> ad_query_triggers,
                             content::RenderFrameHost* frame);

  bool IsPopup(RuleGroup group,
               url::Origin opener_frame_url,
               GURL target_url,
               bool disable_generic_rules);
  bool IsPopunder(RuleGroup group,
                  GURL opener_url,
                  url::Origin target_origin,
                  bool disable_generic_rules);

  void OnNavigationTrackerCreated(NavigationTrackerImpl* tracker);
  void OnNavigationTrackerDestroyed(NavigationTrackerImpl* tracker);
  NavigationTrackerImpl* GetNavigationTrackerFromNavigationId(
      int64_t navigation_id) const;

  // StateAndLogs implementation
  const TrackerInfo* GetTrackerInfo(RuleGroup group,
                                    const std::string& domain) const override;
  void CreateTabHelper(content::WebContents* contents) override;
  TabStateAndLogs* GetTabHelper(content::WebContents* contents) const override;
  NavigationTracker* GetNavigationTracker(
      content::NavigationHandle& navigation_handle) const override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 private:
  void PrepareNewNotifications();
  void SendNotifications();

  raw_ptr<RuleServiceImpl> rules_service_;

  RuleGroupArray<std::set<content::WebContents*>> tabs_with_new_blocks_;
  std::set<content::WebContents*> tabs_with_new_attribution_trackers_;

  RuleGroupArray<std::map<std::string, TrackerInfo>> tracker_infos_;

  absl::flat_hash_map<int64_t, raw_ptr<NavigationTrackerImpl>>
      navigation_trackers_;

  base::Time last_notification_time_;
  base::OneShotTimer next_notification_timer_;

  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<StateAndLogsImpl> weak_factory_{this};
};

}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_STATE_AND_LOGS_IMPL_H_
