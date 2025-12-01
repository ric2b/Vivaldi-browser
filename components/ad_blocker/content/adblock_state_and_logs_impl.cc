// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/content/adblock_state_and_logs_impl.h"

#include <optional>

#include "components/ad_blocker/content/adblock_navigation_tracker_impl.h"
#include "components/ad_blocker/content/adblock_rule_service_impl.h"
#include "components/ad_blocker/content/adblock_tab_state_and_logs_impl.h"
#include "components/ad_blocker/content/simple_index_base_query.h"
#include "components/ad_blocker/content/simple_index_request_query.h"
#include "components/ad_blocker/content/utils.h"
#include "components/ad_blocker/public/content/adblock_tab_state_and_logs.h"
#include "components/ad_blocker/public/core/adblock_rule_manager.h"
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
  auto& tracker_infos = tracker_infos_[group];
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
  auto& tracker_infos = tracker_infos_[group];
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
  TabStateAndLogsImpl* tab_helper = GetTabHelperImpl(frame);

  bool is_known_tracker = false;

  if (url.has_host()) {
    std::string host_str(url.host());
    std::string_view host(host_str);
    // If the host name ends with a dot, then ignore it.
    if (host.back() == '.')
      host.remove_suffix(1);

    for (size_t position = 0;; ++position) {
      const std::string subdomain(host.substr(position));

      if (tracker_infos_[group].count(subdomain)) {
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

  tabs_with_new_blocks_[group].insert(
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

  GetTabHelperImpl(frame)->SetAdQueryTriggers(ad_url,
                                              std::move(ad_query_triggers));
}

std::optional<RulesIndex::AdAttributionMatchParams>
StateAndLogsImpl::GetAdAttributionMatchParams(
    content::RenderFrameHost* frame) const {
  if (!frame || frame->GetBrowserContext()->IsOffTheRecord()) {
    return std::nullopt;
  }

  return GetTabHelperImpl(frame)->GetAdAttributionMatchParams();
}

void StateAndLogsImpl::OnMatchedAttributionTracker(
    content::RenderFrameHost* frame,
    const GURL& url) {
  CHECK(frame && !frame->GetBrowserContext()->IsOffTheRecord());
  GetTabHelperImpl(frame)->OnMatchedAttributionTracker(url);

  tabs_with_new_attribution_trackers_.insert(
      content::WebContents::FromRenderFrameHost(frame));
  PrepareNewNotifications();
}

bool StateAndLogsImpl::IsPopup(RuleGroup group,
                               url::Origin opener_frame_origin,
                               GURL target_url,
                               bool disable_generic_rules) {
  RulesIndex* index =
      rules_service_->GetRuleIndex(static_cast<RuleGroup>(group));
  if (!index) {
    return false;
  }

  const std::optional<RequestFilterRuleStub>& rule_stub =
      index->FindMatchingBeforeRequestRule(
          SimpleIndexRequestQuery(target_url, opener_frame_origin,
                                  "" /*method*/, disable_generic_rules),
          false /*must_intersect_host*/, ResourceType::kPopup, std::nullopt);
  CHECK(!rule_stub || rule_stub->modify_block);

  if (!rule_stub || rule_stub->decision == RuleDecision::kPass) {
    return false;
  }

  return true;
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
  SimpleIndexRequestQuery query(opener_url, target_origin, "" /*method*/,
                                disable_generic_rules);
  {
    const std::optional<RequestFilterRuleStub>& rule_stub =
        index->FindMatchingBeforeRequestRule(
            query, false /*must_intersect_host*/, ResourceType::kPopunder,
            std::nullopt);

    CHECK(!rule_stub || rule_stub->modify_block);

    if (rule_stub && rule_stub->decision != RuleDecision::kPass) {
      return true;
    }

    if (!opener_url.has_host()) {
      return false;
    }
  }

  // Matching uBlock behavior: We look for matching popup rules. If they match
  // and the match contains some of the host part of the url, consider it a
  // popunder

  {
    const std::optional<RequestFilterRuleStub>& rule_stub =
        index->FindMatchingBeforeRequestRule(
            query, true /*must_intersect_host*/, ResourceType::kPopup,
            std::nullopt);

    CHECK(!rule_stub || rule_stub->modify_block);

    if (rule_stub && rule_stub->decision != RuleDecision::kPass) {
      return true;
    }
  }

  GURL origin_only = opener_url.GetWithEmptyPath();
  if (!origin_only.is_valid()) {
    return false;
  }

  const std::optional<RequestFilterRuleStub>& rule_stub =
      index->FindMatchingBeforeRequestRule(
          SimpleIndexRequestQuery(origin_only, target_origin, "" /*method*/,
                                  disable_generic_rules),
          true /*must_intersect_host*/, ResourceType::kPopup, std::nullopt);

  CHECK(!rule_stub || rule_stub->modify_block);

  if (rule_stub && rule_stub->decision != RuleDecision::kPass) {
    return true;
  }
  return false;
}

void StateAndLogsImpl::OnNavigationTrackerCreated(
    NavigationTrackerImpl* tracker) {
  // No entry is expected to be present for this key.
  CHECK(navigation_trackers_.insert_or_assign(tracker->navigation_id(), tracker)
            .second);
}

void StateAndLogsImpl::OnNavigationTrackerDestroyed(
    NavigationTrackerImpl* tracker) {
  navigation_trackers_.erase(tracker->navigation_id());
}

NavigationTrackerImpl* StateAndLogsImpl::GetNavigationTrackerFromNavigationId(
    int64_t navigation_id) const {
  auto tracker = navigation_trackers_.find(navigation_id);
  if (tracker == navigation_trackers_.end()) {
    return nullptr;
  }

  return tracker->second;
}

void StateAndLogsImpl::OnTabRemoved(content::WebContents* contents) {
  for (auto [group, contents_set] : tabs_with_new_blocks_) {
    contents_set.erase(contents);
  }
}

void StateAndLogsImpl::OnAllowAttributionChanged(
    content::WebContents* contents) {
  for (Observer& observer : observers_) {
    observer.OnAllowAttributionChanged(contents);
  }
}

std::optional<ActivationResults> StateAndLogsImpl::GetLocalActivations(
    RuleGroup group,
    const url::Origin& parent_origin,
    const GURL& url) {
  const std::optional<std::string_view> browser_owned_frame_url_prefix =
      rules_service_->GetBrowserOwnedFrameUrlPrefix();
  if (browser_owned_frame_url_prefix && url.possibly_invalid_spec().starts_with(
                                            *browser_owned_frame_url_prefix)) {
    return ActivationResults{.document_exception = true};
  }

  if (!CanFilterUrl(url, false) || !url.is_valid()) {
    return ActivationResults{};
  }

  RulesIndex* index = rules_service_->GetRuleIndex(group);
  if (!index) {
    return std::nullopt;
  }

  ActivationResults result =
      index->FindActivations(SimpleIndexBaseQuery(url, parent_origin));
  if (rules_service_->GetRuleManager()->IsExemptOfFiltering(
          group, url::Origin::Create(url))) {
    result.document_exception = true;
  }
  return result;
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

void StateAndLogsImpl::CreateTabHelper(content::WebContents* contents) {
  TabStateAndLogsImpl::CreateForWebContents(contents,
                                            weak_factory_.GetWeakPtr());
}

TabStateAndLogs* StateAndLogsImpl::GetTabHelper(
    content::WebContents* contents) const {
  return GetTabHelperImpl(contents);
}

NavigationTracker* StateAndLogsImpl::GetNavigationTracker(
    content::NavigationHandle& navigation_handle) const {
  return NavigationTrackerImpl::GetForNavigationHandle(navigation_handle);
}

void StateAndLogsImpl::SendNotifications() {
  for (auto [group, contents_set] : tabs_with_new_blocks_) {
    if (!contents_set.empty()) {
      for (Observer& observer : observers_)
        observer.OnNewBlockedUrlsReported(group, contents_set);
      contents_set.clear();
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
