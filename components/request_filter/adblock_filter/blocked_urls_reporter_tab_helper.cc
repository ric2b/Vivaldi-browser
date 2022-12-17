// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/blocked_urls_reporter_tab_helper.h"

#include "components/request_filter/adblock_filter/adblock_rule_service_content.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_factory.h"
#include "components/request_filter/adblock_filter/blocked_urls_reporter.h"
#include "content/public/browser/navigation_handle.h"

namespace adblock_filter {
WEB_CONTENTS_USER_DATA_KEY_IMPL(BlockedUrlsReporterTabHelper);

BlockedUrlsReporterTabHelper::BlockedTrackerInfo::BlockedTrackerInfo() =
    default;
BlockedUrlsReporterTabHelper::BlockedTrackerInfo::~BlockedTrackerInfo() =
    default;
BlockedUrlsReporterTabHelper::BlockedTrackerInfo::BlockedTrackerInfo(
    BlockedTrackerInfo&& other) = default;

BlockedUrlsReporterTabHelper::TabBlockedUrlInfo::TabBlockedUrlInfo() = default;
BlockedUrlsReporterTabHelper::TabBlockedUrlInfo::~TabBlockedUrlInfo() = default;
BlockedUrlsReporterTabHelper::TabBlockedUrlInfo::TabBlockedUrlInfo(
    TabBlockedUrlInfo&& other) = default;
BlockedUrlsReporterTabHelper::TabBlockedUrlInfo&
BlockedUrlsReporterTabHelper::TabBlockedUrlInfo::operator=(
    TabBlockedUrlInfo&& other) = default;

BlockedUrlsReporterTabHelper::BlockedUrlsReporterTabHelper(
    content::WebContents* contents)
    : WebContentsObserver(contents),
      WebContentsUserData<BlockedUrlsReporterTabHelper>(*contents) {}

BlockedUrlsReporterTabHelper::~BlockedUrlsReporterTabHelper() = default;

void BlockedUrlsReporterTabHelper::OnUrlBlocked(RuleGroup group, GURL url) {
  TabBlockedUrlInfo& blocked_urls =
      ongoing_navigations_.empty()
          ? blocked_urls_[static_cast<size_t>(group)]
          : new_blocked_urls_[static_cast<size_t>(group)];

  blocked_urls.total_count++;
  blocked_urls.blocked_urls[url.spec()].blocked_count++;
}

void BlockedUrlsReporterTabHelper::OnTrackerBlocked(RuleGroup group,
                                                    const std::string& domain,
                                                    const GURL& url) {
  TabBlockedUrlInfo& blocked_urls =
      ongoing_navigations_.empty()
          ? blocked_urls_[static_cast<size_t>(group)]
          : new_blocked_urls_[static_cast<size_t>(group)];

  blocked_urls.total_count++;
  BlockedTrackerInfo& blocked_tracker = blocked_urls.blocked_trackers[domain];
  blocked_tracker.blocked_count++;
  blocked_tracker.blocked_urls[url.spec()].blocked_count++;
}

const BlockedUrlsReporterTabHelper::TabBlockedUrlInfo&
BlockedUrlsReporterTabHelper::GetBlockedUrlsInfo(RuleGroup group) {
  return blocked_urls_[static_cast<size_t>(group)];
}

void BlockedUrlsReporterTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument())
    return;

  ongoing_navigations_.insert(navigation_handle->GetNavigationId());

  // Start recording blocked URLs from the beginning of the latest triggered
  // navigation. We might have cancelled ongoing navigations before starting
  // this one, so make sure we remove the records from any previous navigation
  // attempt.
  new_blocked_urls_ = std::array<TabBlockedUrlInfo, kRuleGroupCount>();
}

void BlockedUrlsReporterTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument())
    return;

  ongoing_navigations_.erase(navigation_handle->GetNavigationId());

  if (!navigation_handle->HasCommitted())
    return;

  blocked_urls_.swap(new_blocked_urls_);
}

void BlockedUrlsReporterTabHelper::WebContentsDestroyed() {
  adblock_filter::RuleService* rules_service =
      adblock_filter::RuleServiceFactory::GetForBrowserContext(
          web_contents()->GetBrowserContext());
  if (rules_service)
    rules_service->GetBlockerUrlsReporter()->OnTabRemoved(web_contents());
}

}  // namespace adblock_filter