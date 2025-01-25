// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/adblock_tab_handler.h"

#include <optional>

#include "base/stl_util.h"
#include "components/request_filter/adblock_filter/adblock_request_filter_tab_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"

namespace adblock_filter {
namespace {
const int kSecondsBetweenNotifications = 1;

struct FrameInfo {
  content::WebContents* web_contents;
  bool is_off_the_record;
  RequestFilterTabHelper* tab_helper;
};

std::optional<FrameInfo> GetFrameInfo(content::RenderFrameHost* frame,
                                      bool allow_off_the_record) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame);
  if (!web_contents) {
    // Don't keep stats on blocked urls not tied to a WebContents for now.
    return std::nullopt;
  }

  bool is_off_the_record = web_contents->GetBrowserContext()->IsOffTheRecord();

  if (is_off_the_record && !allow_off_the_record) {
    return std::nullopt;
  }

  // Create it if it doesn't exist yet.
  RequestFilterTabHelper::CreateForWebContents(web_contents);
  RequestFilterTabHelper* tab_helper =
      RequestFilterTabHelper::FromWebContents(web_contents);

  if (!tab_helper) {
    return std::nullopt;
  }

  return FrameInfo{web_contents, is_off_the_record, tab_helper};
}
}  // namespace

TabHandler::Observer::~Observer() = default;

TabHandler::TabHandler(base::Time reporting_start,
                       CounterGroup blocked_domains,
                       CounterGroup blocked_for_origin,
                       base::RepeatingClosure schedule_save)
    : reporting_start_(reporting_start),
      blocked_domains_(std::move(blocked_domains)),
      blocked_for_origin_(std::move(blocked_for_origin)),
      schedule_save_(schedule_save),
      weak_factory_(this) {
  if (reporting_start.is_null())
    ClearBlockedCounters();
}

TabHandler::~TabHandler() = default;

void TabHandler::OnTrackerInfosUpdated(RuleGroup group,
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

const std::map<uint32_t, base::Value>* TabHandler::GetTrackerInfo(
    RuleGroup group,
    const std::string& domain) const {
  auto& tracker_infos = tracker_infos_[static_cast<size_t>(group)];
  const auto& tracker_info = tracker_infos.find(domain);
  if (tracker_info == tracker_infos.end())
    return nullptr;
  else
    return &tracker_info->second;
}

void TabHandler::SetFrameBlockState(RuleGroup group,
                                    content::RenderFrameHost* frame) {
  std::optional<FrameInfo> frame_info = GetFrameInfo(frame, false);
  if (!frame_info) {
    return;
  }

  frame_info->tab_helper->SetFrameBlockState(group,
                                             frame->GetFrameTreeNodeId());
}

void TabHandler::ResetFrameBlockState(RuleGroup group,
                                      content::RenderFrameHost* frame) {
  std::optional<FrameInfo> frame_info = GetFrameInfo(frame, false);
  if (!frame_info) {
    return;
  }

  frame_info->tab_helper->ResetFrameBlockState(group,
                                               frame->GetFrameTreeNodeId());
}

void TabHandler::OnUrlBlocked(RuleGroup group,
                              url::Origin origin,
                              GURL url,
                              content::RenderFrameHost* frame) {
  std::optional<FrameInfo> frame_info = GetFrameInfo(frame, true);
  if (!frame_info) {
    return;
  }
  auto [web_contents, is_off_the_record, tab_helper] = *frame_info;

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
        if (!is_off_the_record)
          AddToCounter(blocked_domains_, group, subdomain);
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
    if (url.has_host() && !is_off_the_record)
      AddToCounter(blocked_domains_, group, url.host());
  }

  if (!origin.host().empty() && !is_off_the_record)
    AddToCounter(blocked_for_origin_, group, origin.host());

  tabs_with_new_blocks_[static_cast<size_t>(group)].insert(web_contents);

  PrepareNewNotifications();
}

void TabHandler::SetAdAttributionState(bool enabled,
                                       content::RenderFrameHost* frame) {
  std::optional<FrameInfo> frame_info = GetFrameInfo(frame, false);
  if (!frame_info || frame_info->web_contents->GetPrimaryMainFrame() != frame) {
    return;
  }
  frame_info->tab_helper->SetAdAttributionState(enabled);
}

void TabHandler::SetTabAdQueryTriggers(
    const GURL& ad_url,
    std::vector<std::string> ad_query_triggers,
    content::RenderFrameHost* frame) {
  std::optional<FrameInfo> frame_info = GetFrameInfo(frame, false);
  if (!frame_info || frame_info->web_contents->GetPrimaryMainFrame() != frame) {
    return;
  }
  frame_info->tab_helper->SetAdQueryTriggers(ad_url,
                                             std::move(ad_query_triggers));
}

bool TabHandler::DoesAdAttributionMatch(
    content::RenderFrameHost* frame,
    std::string_view tracker_url_spec,
    std::string_view ad_domain_and_query_trigger) {
  std::optional<FrameInfo> frame_info = GetFrameInfo(frame, false);
  if (!frame_info) {
    return false;
  }
  bool result = frame_info->tab_helper->DoesAdAttributionMatch(
      tracker_url_spec, ad_domain_and_query_trigger);

  if (result) {
    tabs_with_new_attribution_trackers_.insert(frame_info->web_contents);
    PrepareNewNotifications();
  }

  return result;
}

void TabHandler::AddToCounter(CounterGroup& counter_group,
                              RuleGroup group,
                              std::string domain) {
  auto& counters = counter_group[static_cast<size_t>(group)];
  auto domain_counter = counters.find(domain);
  if (domain_counter != counters.end())
    domain_counter->second++;
  else
    counters.insert({domain, 1});
}

void TabHandler::ClearBlockedCounters() {
  for (size_t i = 0; i < kRuleGroupCount; i++) {
    blocked_domains_[i].clear();
    blocked_for_origin_[i].clear();
  }
  reporting_start_ = base::Time::Now();
}

bool TabHandler::WasFrameBlocked(RuleGroup group,
                                 content::RenderFrameHost* frame) const {
  std::optional<FrameInfo> frame_info = GetFrameInfo(frame, false);
  if (!frame_info) {
    return false;
  }

  return frame_info->tab_helper->WasFrameBlocked(group,
                                                 frame->GetFrameTreeNodeId());
}

void TabHandler::OnTabRemoved(content::WebContents* contents) {
  for (size_t group = 0; group < kRuleGroupCount; group++)
    tabs_with_new_blocks_[group].erase(contents);
}

void TabHandler::OnAllowAttributionChanged(content::WebContents* contents) {
  for (Observer& observer : observers_) {
    observer.OnAllowAttributionChanged(contents);
  }
}

void TabHandler::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TabHandler::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void TabHandler::PrepareNewNotifications() {
  if (next_notification_timer_.IsRunning())
    return;

  base::TimeDelta time_since_last_notification =
      base::Time::Now() - last_notification_time_;
  if (time_since_last_notification >
      base::Seconds(kSecondsBetweenNotifications)) {
    SendNotifications();
    return;
  }

  next_notification_timer_.Start(FROM_HERE,
                                 base::Seconds(kSecondsBetweenNotifications) -
                                     time_since_last_notification,
                                 base::BindOnce(&TabHandler::SendNotifications,
                                                weak_factory_.GetWeakPtr()));
}

void TabHandler::SendNotifications() {
  schedule_save_.Run();
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