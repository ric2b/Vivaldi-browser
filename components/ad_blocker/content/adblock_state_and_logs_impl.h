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
#include "components/ad_blocker/content/adblock_rules_index.h"
#include "components/ad_blocker/public/content/adblock_state_and_logs.h"
#include "components/ad_blocker/public/core/adblock_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace adblock_filter {
class RuleServiceImpl;
class TabStateAndLogsImpl;

class StateAndLogsImpl : public StateAndLogs {
 public:
  StateAndLogsImpl(RuleServiceImpl* rules_service);
  ~StateAndLogsImpl() override;
  StateAndLogsImpl(const StateAndLogsImpl&) = delete;
  StateAndLogsImpl& operator=(const StateAndLogsImpl&) = delete;

  TabStateAndLogsImpl* CreateTabHelperImpl(content::RenderFrameHost* frame);
  TabStateAndLogsImpl* CreateTabHelperImpl(content::WebContents* contents);

  std::optional<RulesIndex::ActivationResults> GetLocalActivations(
      RuleGroup group,
      const url::Origin& parent_origin,
      const GURL& url);

  void OnTrackerInfosUpdated(RuleGroup group,
                             const ActiveRuleSource& source,
                             base::Value::Dict new_tracker_infos);

  void OnUrlBlocked(RuleGroup group,
                    url::Origin origin,
                    GURL url,
                    content::RenderFrameHost* frame);
  void OnTabRemoved(content::WebContents* contents);
  void OnAllowAttributionChanged(content::WebContents* contents);

  void SetTabAdQueryTriggers(const GURL& ad_url,
                             std::vector<std::string> ad_query_triggers,
                             content::RenderFrameHost* frame);
  bool DoesAdAttributionMatch(content::RenderFrameHost* frame,
                              std::string_view tracker_url_spec,
                              std::string_view ad_domain_and_query_trigger);

  bool IsPopup(RuleGroup group,
               url::Origin opener_frame_url,
               GURL target_url,
               bool disable_generic_rules);
  bool IsPopunder(RuleGroup group,
                  GURL opener_url,
                  url::Origin target_origin,
                  bool disable_generic_rules);

  // StateAndLogs implementation
  const TrackerInfo* GetTrackerInfo(RuleGroup group,
                                    const std::string& domain) const override;
  std::array<std::optional<TabStateAndLogs::RuleData>, kRuleGroupCount>
  WasNavigationBlocked(
      const content::NavigationHandle* navigation) const override;
  TabStateAndLogs* GetTabHelper(content::WebContents* contents) const override;
  TabStateAndLogs* CreateTabHelper(content::WebContents* contents) override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 private:
  void PrepareNewNotifications();
  void SendNotifications();

  raw_ptr<RuleServiceImpl> rules_service_;

  std::array<std::set<content::WebContents*>, kRuleGroupCount>
      tabs_with_new_blocks_;
  std::set<content::WebContents*> tabs_with_new_attribution_trackers_;

  std::array<std::map<std::string, TrackerInfo>, kRuleGroupCount>
      tracker_infos_;

  base::Time last_notification_time_;
  base::OneShotTimer next_notification_timer_;

  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<StateAndLogsImpl> weak_factory_{this};
};

}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_STATE_AND_LOGS_IMPL_H_
