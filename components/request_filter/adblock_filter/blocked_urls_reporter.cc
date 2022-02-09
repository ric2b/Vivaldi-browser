// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/blocked_urls_reporter.h"

#include "base/stl_util.h"
#include "components/request_filter/adblock_filter/blocked_urls_reporter_tab_helper.h"
#include "content/public/browser/web_contents.h"

namespace adblock_filter {
namespace {
const int kSecondsBetweenNotifications = 1;
}

BlockedUrlsReporter::Observer::~Observer() = default;

BlockedUrlsReporter::BlockedUrlsReporter(BlockedDomains blocked_domains,
                                         base::RepeatingClosure schedule_save)
    : blocked_domains_(std::move(blocked_domains)),
      schedule_save_(schedule_save),
      weak_factory_(this) {}

BlockedUrlsReporter::~BlockedUrlsReporter() = default;

void BlockedUrlsReporter::OnTrackerInfosUpdated(const RuleSource& source,
                                                base::Value new_tracker_infos) {
  DCHECK(new_tracker_infos.is_dict());
  auto& tracker_infos = tracker_infos_[static_cast<size_t>(source.group)];
  base::EraseIf(tracker_infos, [&source](auto& tracker) {
    tracker.second.erase(source.id);
    return tracker.second.empty();
  });

  for (const auto& tracker : new_tracker_infos.DictItems()) {
    tracker_infos[tracker.first][source.id] = tracker.second.Clone();
  }
}

const std::map<uint32_t, base::Value>* BlockedUrlsReporter::GetTrackerInfo(
    RuleGroup group,
    const std::string& domain) {
  auto& tracker_infos = tracker_infos_[static_cast<size_t>(group)];
  const auto& tracker_info = tracker_infos.find(domain);
  if (tracker_info == tracker_infos.end())
    return nullptr;
  else
    return &tracker_info->second;
}

void BlockedUrlsReporter::OnUrlBlocked(RuleGroup group,
                                       GURL url,
                                       content::RenderFrameHost* frame) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame);
  if (!web_contents) {
    // Don't keep stats on blocked urls not tied to a WebContents for now.
    return;
  }

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
        AddToBlockedDomains(group, subdomain);
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
    if (url.has_host())
      AddToBlockedDomains(group, url.host());
  }

  tabs_with_new_blocks_[static_cast<size_t>(group)].insert(web_contents);

  if (next_notifiaction_timer_.IsRunning())
    return;

  base::TimeDelta time_since_last_notification =
      base::Time::Now() - last_notification_time_;
  if (time_since_last_notification >
      base::TimeDelta::FromSeconds(kSecondsBetweenNotifications)) {
    NotifyOfNewBlockedUrls();
    return;
  }

  next_notifiaction_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(kSecondsBetweenNotifications) -
          time_since_last_notification,
      base::BindOnce(&BlockedUrlsReporter::NotifyOfNewBlockedUrls,
                     weak_factory_.GetWeakPtr()));
}

void BlockedUrlsReporter::AddToBlockedDomains(RuleGroup group,
                                              std::string domain) {
  auto& blocked_domains = blocked_domains_[static_cast<size_t>(group)];
  auto blocked_domain = blocked_domains.find(domain);
  if (blocked_domain != blocked_domains.end())
    blocked_domain->second++;
  else
    blocked_domains.insert({domain, 1});
}

void BlockedUrlsReporter::ClearBlockedDomains() {
  for (size_t i = 0; i < kRuleGroupCount; i++) {
    blocked_domains_[i].clear();
  }
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