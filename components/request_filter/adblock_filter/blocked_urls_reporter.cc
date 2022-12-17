// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/blocked_urls_reporter.h"

#include "base/containers/cxx20_erase.h"
#include "base/stl_util.h"
#include "components/request_filter/adblock_filter/blocked_urls_reporter_tab_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"

namespace adblock_filter {
namespace {
const int kSecondsBetweenNotifications = 1;
}

BlockedUrlsReporter::Observer::~Observer() = default;

BlockedUrlsReporter::BlockedUrlsReporter(base::Time reporting_start,
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

BlockedUrlsReporter::~BlockedUrlsReporter() = default;

void BlockedUrlsReporter::OnTrackerInfosUpdated(
    const RuleSource& source,
    base::Value::Dict new_tracker_infos) {
  auto& tracker_infos = tracker_infos_[static_cast<size_t>(source.group)];
  base::EraseIf(tracker_infos, [&source](auto& tracker) {
    tracker.second.erase(source.id);
    return tracker.second.empty();
  });

  for (const auto tracker : new_tracker_infos) {
    tracker_infos[tracker.first][source.id] = std::move(tracker.second);
  }
}

const std::map<uint32_t, base::Value>* BlockedUrlsReporter::GetTrackerInfo(
    RuleGroup group,
    const std::string& domain) const {
  auto& tracker_infos = tracker_infos_[static_cast<size_t>(group)];
  const auto& tracker_info = tracker_infos.find(domain);
  if (tracker_info == tracker_infos.end())
    return nullptr;
  else
    return &tracker_info->second;
}

void BlockedUrlsReporter::OnUrlBlocked(RuleGroup group,
                                       url::Origin origin,
                                       GURL url,
                                       content::RenderFrameHost* frame) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame);
  if (!web_contents) {
    // Don't keep stats on blocked urls not tied to a WebContents for now.
    return;
  }

  bool is_off_the_record = web_contents->GetBrowserContext()->IsOffTheRecord();

    // Create it if it doesn't exist yet.
    BlockedUrlsReporterTabHelper::CreateForWebContents(web_contents);
  BlockedUrlsReporterTabHelper* tab_helper =
      BlockedUrlsReporterTabHelper::FromWebContents(web_contents);

  bool is_known_tracker = false;

  if (url.has_host()) {
    std::string host_str(url.host());
    base::StringPiece host(host_str);
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
      if (position == base::StringPiece::npos)
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

  if (next_notifiaction_timer_.IsRunning())
    return;

  base::TimeDelta time_since_last_notification =
      base::Time::Now() - last_notification_time_;
  if (time_since_last_notification >
      base::Seconds(kSecondsBetweenNotifications)) {
    NotifyOfNewBlockedUrls();
    return;
  }

  next_notifiaction_timer_.Start(
      FROM_HERE,
      base::Seconds(kSecondsBetweenNotifications) -
          time_since_last_notification,
      base::BindOnce(&BlockedUrlsReporter::NotifyOfNewBlockedUrls,
                     weak_factory_.GetWeakPtr()));
}

void BlockedUrlsReporter::AddToCounter(CounterGroup& counter_group,
                                       RuleGroup group,
                                       std::string domain) {
  auto& counters = counter_group[static_cast<size_t>(group)];
  auto domain_counter = counters.find(domain);
  if (domain_counter != counters.end())
    domain_counter->second++;
  else
    counters.insert({domain, 1});
}

void BlockedUrlsReporter::ClearBlockedCounters() {
  for (size_t i = 0; i < kRuleGroupCount; i++) {
    blocked_domains_[i].clear();
    blocked_for_origin_[i].clear();
  }
  reporting_start_ = base::Time::Now();
}

void BlockedUrlsReporter::OnTabRemoved(content::WebContents* contents) {
  for (size_t group = 0; group < kRuleGroupCount; group++)
    tabs_with_new_blocks_[group].erase(contents);
}

void BlockedUrlsReporter::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void BlockedUrlsReporter::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void BlockedUrlsReporter::NotifyOfNewBlockedUrls() {
  schedule_save_.Run();
  for (size_t group = 0; group < kRuleGroupCount; group++)
    if (!tabs_with_new_blocks_[group].empty()) {
      for (Observer& observer : observers_)
        observer.OnNewBlockedUrlsReported(RuleGroup(group),
                                          tabs_with_new_blocks_[group]);
      tabs_with_new_blocks_[group].clear();
    }
}

}  // namespace adblock_filter