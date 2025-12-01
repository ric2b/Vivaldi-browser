// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/content/adblock_state_and_logs_impl.h"

#include <optional>

#include "base/stl_util.h"
#include "components/ad_blocker/content/adblock_rule_service_impl.h"
#include "components/ad_blocker/content/adblock_tab_state_and_logs_impl.h"
#include "components/ad_blocker/content/utils.h"
#include "components/ad_blocker/public/content/adblock_tab_state_and_logs.h"
#include "components/ad_blocker/public/core/adblock_rule_manager.h"
#include "components/ad_blocker/public/core/adblock_stats_data.h"
#include "components/ad_blocker/public/core/adblock_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace adblock_filter {
namespace {
constexpr int kSecondsBetweenNotifications = 1;

TabStateAndLogsImpl* GetTabHelperImpl(content::WebContents* contents) {
  return TabStateAndLogsImpl::FromWebContents(contents);
}

TabStateAndLogsImpl* GetTabHelperImpl(content::RenderFrameHost* frame) {
  return GetTabHelperImpl(content::WebContents::FromRenderFrameHost(frame));
}

}  // namespace

StateAndLogsImpl::StateAndLogsImpl(RuleServiceImpl* rules_service)
    : rules_service_(rules_service) {}

StateAndLogsImpl::~StateAndLogsImpl() = default;

void StateAndLogsImpl::OnTrackerInfosUpdated(
    RuleGroup group,
    const ActiveRuleSource& source,
    base::Value::Dict new_tracker_infos) {
  auto& tracker_infos = tracker_infos_[static_cast<size_t>(group)];
  std::erase_if(tracker_infos, [&source](auto& tracker) {
    tracker.second.erase(source.core.id());
    return tracker.second.empty();
  });

  for (const auto tracker : new_tracker_infos) {
    tracker_infos[tracker.first][source.core.id()] = std::move(tracker.second);
  }
}

const std::map<uint32_t, base::Value>* StateAndLogsImpl::GetTrackerInfo(
    RuleGroup group,
    const std::string& domain) const {
  auto& tracker_infos = tracker_infos_[static_cast<size_t>(group)];
  const auto& tracker_info = tracker_infos.find(domain);
  if (tracker_info == tracker_infos.end())
    return nullptr;
  else
    return &tracker_info->second;
}

void StateAndLogsImpl::OnUrlBlocked(RuleGroup group,
                                    url::Origin origin,
                                    GURL url,
                                    content::RenderFrameHost* frame) {
  CHECK(frame);
  TabStateAndLogsImpl* tab_helper = CreateTabHelperImpl(frame);

  bool is_known_tracker = false;

  if (url.has_host()) {
    std::string host_str(url.host());
    std::string_view host(host_str);
    // If the host name ends with a dot, then ignore it.
    if (host.back() == '.')
      host.remove_suffix(1);

    for (size_t position = 0;; ++position) {
      const std::string subdomain(host.substr(position));

      if (tracker_infos_[static_cast<size_t>(group)].count(subdomain)) {
        tab_helper->OnTrackerBlocked(group, subdomain, url);
        is_known_tracker = true;
        break;
      }

      position = host.find('.', position);
      if (position == std::string_view::npos)
        break;
    }
  }

  if (!is_known_tracker) {
    tab_helper->OnUrlBlocked(group, url);
  }

  if (url.has_host() && !frame->GetBrowserContext()->IsOffTheRecord()) {
    rules_service_->GetStatsStore()->AddEntry(url, origin.host(),
                                              base::Time::Now(), group);
  }

  tabs_with_new_blocks_[static_cast<size_t>(group)].insert(
      content::WebContents::FromRenderFrameHost(frame));

  PrepareNewNotifications();
}

void StateAndLogsImpl::SetTabAdQueryTriggers(
    const GURL& ad_url,
    std::vector<std::string> ad_query_triggers,
    content::RenderFrameHost* frame) {
  CHECK(frame);
  if (!frame->IsInPrimaryMainFrame() ||
      frame->GetBrowserContext()->IsOffTheRecord()) {
    return;
  }

  CreateTabHelperImpl(frame)->SetAdQueryTriggers(ad_url,
                                                 std::move(ad_query_triggers));
}

bool StateAndLogsImpl::DoesAdAttributionMatch(
    content::RenderFrameHost* frame,
    std::string_view tracker_url_spec,
    std::string_view ad_domain_and_query_trigger) {
  CHECK(frame);
  if (frame->GetBrowserContext()->IsOffTheRecord()) {
    return false;
  }
  bool result = CreateTabHelperImpl(frame)->DoesAdAttributionMatch(
      tracker_url_spec, ad_domain_and_query_trigger);

  if (result) {
    tabs_with_new_attribution_trackers_.insert(
        content::WebContents::FromRenderFrameHost(frame));
    PrepareNewNotifications();
  }

  return result;
}

bool StateAndLogsImpl::IsPopup(RuleGroup group,
                               url::Origin opener_frame_origin,
                               GURL target_url,
                               bool disable_generic_rules) {
  RulesIndex* index =
      rules_service_->GetRuleIndex(static_cast<RuleGroup>(group));
  if (index && index->FindMatchingBeforeRequestRule(
                   target_url, false /*must_intersect_host*/,
                   opener_frame_origin, flat::ResourceType_POPUP,
                   GetPartyMatcher(target_url, opener_frame_origin),
                   disable_generic_rules, std::nullopt)) {
    return true;
  }
  return false;
}
bool StateAndLogsImpl::IsPopunder(RuleGroup group,
                                  GURL opener_url,
                                  url::Origin target_origin,
                                  bool disable_generic_rules) {
  RulesIndex* index =
      rules_service_->GetRuleIndex(static_cast<RuleGroup>(group));
  if (!index) {
    return false;
  }
  if (index->FindMatchingBeforeRequestRule(
          opener_url, false /*must_intersect_host*/, target_origin,
          flat::ResourceType_POPUNDER,
          GetPartyMatcher(opener_url, target_origin), disable_generic_rules,
          std::nullopt)) {
    return true;
  }

  if (!opener_url.has_host()) {
    return false;
  }

  // Matching uBlock behavior: We look for matching popup rules. If they match
  // and the match contains some of the host part of the url, consider it a
  // popunder

  if (index->FindMatchingBeforeRequestRule(
          opener_url, true /*must_intersect_host*/, target_origin,
          flat::ResourceType_POPUP, GetPartyMatcher(opener_url, target_origin),
          disable_generic_rules, std::nullopt)) {
    return true;
  }

  GURL origin_only = opener_url.GetWithEmptyPath();
  return origin_only.is_valid() &&
         index->FindMatchingBeforeRequestRule(
             origin_only, true /*must_intersect_host*/, target_origin,
             flat::ResourceType_POPUP,
             GetPartyMatcher(origin_only, target_origin), disable_generic_rules,
             std::nullopt);
}

std::array<std::optional<TabStateAndLogs::RuleData>, kRuleGroupCount>
StateAndLogsImpl::WasNavigationBlocked(
    const content::NavigationHandle* navigation) const {
  TabStateAndLogsImpl* tab_state_and_logs =
      GetTabHelperImpl(navigation->GetRenderFrameHost());
  if (!tab_state_and_logs) {
    return {std::nullopt, std::nullopt};
  }
  return tab_state_and_logs->WasNavigationBlocked(navigation);
}

void StateAndLogsImpl::OnTabRemoved(content::WebContents* contents) {
  for (size_t group = 0; group < kRuleGroupCount; group++)
    tabs_with_new_blocks_[group].erase(contents);
}

void StateAndLogsImpl::OnAllowAttributionChanged(
    content::WebContents* contents) {
  for (Observer& observer : observers_) {
    observer.OnAllowAttributionChanged(contents);
  }
}

std::optional<RulesIndex::ActivationResults>
StateAndLogsImpl::GetLocalActivations(RuleGroup group,
                                      const url::Origin& parent_origin,
                                      const GURL& url) {
  if (!CanFilterUrl(url) || !url.is_valid()) {
    return RulesIndex::ActivationResults{};
  }

  RulesIndex* index = rules_service_->GetRuleIndex(group);
  if (!index) {
    return std::nullopt;
  }

  return index->FindActivations(
      base::BindRepeating(&IsOriginWanted, rules_service_, group),
      parent_origin, url);
}

void StateAndLogsImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void StateAndLogsImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void StateAndLogsImpl::PrepareNewNotifications() {
  if (next_notification_timer_.IsRunning())
    return;

  base::TimeDelta time_since_last_notification =
      base::Time::Now() - last_notification_time_;
  if (time_since_last_notification >
      base::Seconds(kSecondsBetweenNotifications)) {
    SendNotifications();
    return;
  }

  next_notification_timer_.Start(
      FROM_HERE,
      base::Seconds(kSecondsBetweenNotifications) -
          time_since_last_notification,
      base::BindOnce(&StateAndLogsImpl::SendNotifications,
                     weak_factory_.GetWeakPtr()));
}

TabStateAndLogs* StateAndLogsImpl::GetTabHelper(
    content::WebContents* contents) const {
  return GetTabHelperImpl(contents);
}

TabStateAndLogs* StateAndLogsImpl::CreateTabHelper(
    content::WebContents* contents) {
  return CreateTabHelperImpl(contents);
}

TabStateAndLogsImpl* StateAndLogsImpl::CreateTabHelperImpl(
    content::RenderFrameHost* frame) {
  CHECK(frame);
  return CreateTabHelperImpl(content::WebContents::FromRenderFrameHost(frame));
}

TabStateAndLogsImpl* StateAndLogsImpl::CreateTabHelperImpl(
    content::WebContents* contents) {
  TabStateAndLogsImpl::CreateForWebContents(contents,
                                            weak_factory_.GetWeakPtr());
  return GetTabHelperImpl(contents);
}

void StateAndLogsImpl::SendNotifications() {
  for (size_t group = 0; group < kRuleGroupCount; group++) {
    if (!tabs_with_new_blocks_[group].empty()) {
      for (Observer& observer : observers_)
        observer.OnNewBlockedUrlsReported(RuleGroup(group),
                                          tabs_with_new_blocks_[group]);
      tabs_with_new_blocks_[group].clear();
    }
  }

  if (!tabs_with_new_attribution_trackers_.empty()) {
    for (Observer& observer : observers_) {
      observer.OnNewAttributionTrackerAllowed(
          tabs_with_new_attribution_trackers_);
    }
    tabs_with_new_attribution_trackers_.clear();
  }
}

}  // namespace adblock_filter