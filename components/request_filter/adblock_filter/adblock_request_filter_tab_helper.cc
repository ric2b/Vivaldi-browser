// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/adblock_request_filter_tab_helper.h"

#include "components/request_filter/adblock_filter/adblock_rule_service_content.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_factory.h"
#include "components/request_filter/adblock_filter/adblock_tab_handler.h"
#include "content/public/browser/navigation_handle.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"

namespace adblock_filter {

namespace {
constexpr base::TimeDelta kOffSiteTimeout = base::Minutes(30);
constexpr base::TimeDelta kAdAttributionExpiration = base::Days(7);
}  // namespace

WEB_CONTENTS_USER_DATA_KEY_IMPL(RequestFilterTabHelper);

RequestFilterTabHelper::BlockedTrackerInfo::BlockedTrackerInfo() = default;
RequestFilterTabHelper::BlockedTrackerInfo::~BlockedTrackerInfo() = default;
RequestFilterTabHelper::BlockedTrackerInfo::BlockedTrackerInfo(
    BlockedTrackerInfo&& other) = default;

RequestFilterTabHelper::TabBlockedUrlInfo::TabBlockedUrlInfo() = default;
RequestFilterTabHelper::TabBlockedUrlInfo::~TabBlockedUrlInfo() = default;
RequestFilterTabHelper::TabBlockedUrlInfo::TabBlockedUrlInfo(
    TabBlockedUrlInfo&& other) = default;
RequestFilterTabHelper::TabBlockedUrlInfo&
RequestFilterTabHelper::TabBlockedUrlInfo::operator=(
    TabBlockedUrlInfo&& other) = default;

RequestFilterTabHelper::RequestFilterTabHelper(content::WebContents* contents)
    : WebContentsObserver(contents),
      WebContentsUserData<RequestFilterTabHelper>(*contents) {}

RequestFilterTabHelper::~RequestFilterTabHelper() = default;

void RequestFilterTabHelper::SetFrameBlockState(
    RuleGroup group,
    content::FrameTreeNodeId frame_tree_node_id) {
  blocked_frames_[static_cast<size_t>(group)].insert(frame_tree_node_id);
}

void RequestFilterTabHelper::ResetFrameBlockState(
    RuleGroup group,
    content::FrameTreeNodeId frame_tree_node_id) {
  blocked_frames_[static_cast<size_t>(group)].erase(frame_tree_node_id);
}

void RequestFilterTabHelper::OnUrlBlocked(RuleGroup group, GURL url) {
  TabBlockedUrlInfo& blocked_urls =
      ongoing_navigations_.empty()
          ? blocked_urls_[static_cast<size_t>(group)]
          : new_blocked_urls_[static_cast<size_t>(group)];

  blocked_urls.total_count++;
  blocked_urls.blocked_urls[url.spec()].blocked_count++;
}

void RequestFilterTabHelper::OnTrackerBlocked(RuleGroup group,
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

void RequestFilterTabHelper::SetAdAttributionState(bool enabled) {
  if (ongoing_navigations_.empty())
    new_ad_attribution_enabled_ = enabled;
  else
    ad_attribution_enabled_ = enabled;
}

void RequestFilterTabHelper::SetAdQueryTriggers(
    const GURL& ad_url,
    std::vector<std::string> triggers) {
  if (!ad_attribution_enabled_ || ongoing_navigations_.empty()) {
    return;
  }

  ResetAdAttribution();
  ad_click_time_ = base::TimeTicks::Now();
  current_ad_click_domain_ = ad_url.host_piece();
  ad_query_triggers_.swap(triggers);
}

bool RequestFilterTabHelper::DoesAdAttributionMatch(
    std::string_view tracker_url_spec,
    std::string_view ad_domain_and_query_trigger) {
  if (current_ad_landing_domain_.empty() || !is_on_ad_landing_site_) {
    return false;
  }

  size_t separator = ad_domain_and_query_trigger.find_first_of('|');
  CHECK(separator != std::string_view::npos);

  if (ad_domain_and_query_trigger.substr(separator + 1) !=
      current_ad_trigger_) {
    return false;
  }

  std::string_view match_domain =
      ad_domain_and_query_trigger.substr(0, separator);

  if (match_domain.back() == '.') {
    match_domain.remove_suffix(1);
  }

  std::string_view ad_click_domain(current_ad_click_domain_);
  if (ad_click_domain.back() == '.') {
    ad_click_domain.remove_suffix(1);
  }

  if (!ad_click_domain.ends_with(match_domain)) {
    return false;
  }

  ad_click_domain.remove_suffix(match_domain.size());
  if (ad_click_domain.empty() || ad_click_domain.back() == '.') {
    if (ongoing_navigations_.empty()) {
      allowed_attribution_trackers_.insert(std::string(tracker_url_spec));
    } else {
      new_allowed_attribution_trackers_.insert(std::string(tracker_url_spec));
    }
    return true;
  }

  return false;
}

void RequestFilterTabHelper::ResetAdAttribution() {
  ad_click_time_ = base::TimeTicks();
  current_ad_click_domain_.clear();
  current_ad_trigger_.clear();
  current_ad_landing_domain_.clear();
  last_attributed_navigation_ = base::TimeTicks();
  is_on_ad_landing_site_ = false;
  ad_attribution_expiration_.Stop();

  adblock_filter::RuleService* rules_service =
      adblock_filter::RuleServiceFactory::GetForBrowserContext(
          web_contents()->GetBrowserContext());
  if (rules_service)
    rules_service->GetTabHandler()->OnAllowAttributionChanged(web_contents());
}

const RequestFilterTabHelper::TabBlockedUrlInfo&
RequestFilterTabHelper::GetBlockedUrlsInfo(RuleGroup group) {
  return blocked_urls_[static_cast<size_t>(group)];
}

bool RequestFilterTabHelper::WasFrameBlocked(
    RuleGroup group,
    content::FrameTreeNodeId frame_tree_node_id) {
  return blocked_frames_[static_cast<size_t>(group)].contains(
      frame_tree_node_id);
}

void RequestFilterTabHelper::FrameDeleted(
    content::FrameTreeNodeId frame_tree_node_id) {
  for (auto group : {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
    ResetFrameBlockState(group, frame_tree_node_id);
  }
}

void RequestFilterTabHelper::DoQueryTriggerCheck(const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS() || !url.has_host())
    return;

  if (!current_ad_trigger_.empty()) {
    return;
  }

  // Make it easy to match arguments using &name=
  std::string query("&");
  query.append(url.query());
  for (const std::string& ad_query_trigger : ad_query_triggers_) {
    if (query.find(ad_query_trigger) != std::string::npos) {
      current_ad_landing_domain_ =
          net::registry_controlled_domains::GetDomainAndRegistry(
              url,
              net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
      if (!current_ad_landing_domain_.empty()) {
        current_ad_trigger_ = ad_query_trigger;
        last_attributed_navigation_ = base::TimeTicks::Now();
        // Unretained is safe as we own the timer and the timer owns the
        // callback.
        ad_attribution_expiration_.Start(
            FROM_HERE,
            base::TimeTicks::Now() - ad_click_time_ + kAdAttributionExpiration,
            base::BindOnce(&RequestFilterTabHelper::ResetAdAttribution,
                           base::Unretained(this)));
        adblock_filter::RuleService* rules_service =
            adblock_filter::RuleServiceFactory::GetForBrowserContext(
                web_contents()->GetBrowserContext());
        if (rules_service)
          rules_service->GetTabHandler()->OnAllowAttributionChanged(
              web_contents());
      }
      return;
    }
  }
}

void RequestFilterTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument())
    return;

  ongoing_navigations_.insert(navigation_handle->GetNavigationId());

  if (navigation_handle->GetPageTransition() &
      ui::PAGE_TRANSITION_IS_REDIRECT_MASK) {
    DoQueryTriggerCheck(navigation_handle->GetURL());
    return;
  }

  // Start recording blocked URLs from the beginning of the latest triggered
  // navigation. We might have cancelled ongoing navigations before starting
  // this one, so make sure we remove the records from any previous
  // navigation attempt.
  new_blocked_urls_ = std::array<TabBlockedUrlInfo, kRuleGroupCount>();
  new_ad_attribution_enabled_ = false;
  new_allowed_attribution_trackers_.clear();
  ad_query_triggers_.clear();
}

void RequestFilterTabHelper::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument())
    return;

  DoQueryTriggerCheck(navigation_handle->GetURL());
}

void RequestFilterTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument())
    return;

  ongoing_navigations_.erase(navigation_handle->GetNavigationId());

  if (!navigation_handle->HasCommitted())
    return;

  if (!current_ad_landing_domain_.empty()) {
    if (current_ad_landing_domain_ ==
        net::registry_controlled_domains::GetDomainAndRegistry(
            navigation_handle->GetURL(),
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
      SetIsOnAdLandingSite(true);
      last_attributed_navigation_ = base::TimeTicks::Now();
    } else if (last_attributed_navigation_ + kOffSiteTimeout >
               base::TimeTicks::Now()) {
      SetIsOnAdLandingSite(false);
      last_attributed_navigation_ = base::TimeTicks::Now();
    } else {
      ResetAdAttribution();
    }
  }

  blocked_urls_.swap(new_blocked_urls_);
  allowed_attribution_trackers_.swap(new_allowed_attribution_trackers_);
  ad_attribution_enabled_ = new_ad_attribution_enabled_;
}

void RequestFilterTabHelper::DidOpenRequestedURL(
    content::WebContents* new_contents,
    content::RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  if (!current_ad_landing_domain_.empty() &&
      current_ad_landing_domain_ ==
          net::registry_controlled_domains::GetDomainAndRegistry(
              url,
              net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    RequestFilterTabHelper::CreateForWebContents(new_contents);
    auto* new_tab_helper =
        RequestFilterTabHelper::FromWebContents(new_contents);
    new_tab_helper->current_ad_click_domain_ = current_ad_click_domain_;
    new_tab_helper->ad_click_time_ = ad_click_time_;
    new_tab_helper->current_ad_trigger_ = current_ad_trigger_;
    new_tab_helper->current_ad_landing_domain_ = current_ad_landing_domain_;
    new_tab_helper->is_on_ad_landing_site_ = true;
    new_tab_helper->last_attributed_navigation_ = base::TimeTicks::Now();
    new_tab_helper->ad_attribution_expiration_.Start(
        FROM_HERE,
        base::TimeTicks::Now() - ad_click_time_ + kAdAttributionExpiration,
        base::BindOnce(&RequestFilterTabHelper::ResetAdAttribution,
                       base::Unretained(new_tab_helper)));

    adblock_filter::RuleService* rules_service =
        adblock_filter::RuleServiceFactory::GetForBrowserContext(
            web_contents()->GetBrowserContext());
    if (rules_service)
      rules_service->GetTabHandler()->OnAllowAttributionChanged(new_contents);
  }
}

void RequestFilterTabHelper::SetIsOnAdLandingSite(bool is_on_ad_landing_site) {
  bool was_on_ad_landing_site = is_on_ad_landing_site_;
  is_on_ad_landing_site_ = is_on_ad_landing_site;

  if (is_on_ad_landing_site != was_on_ad_landing_site) {
    adblock_filter::RuleService* rules_service =
        adblock_filter::RuleServiceFactory::GetForBrowserContext(
            web_contents()->GetBrowserContext());
    if (rules_service)
      rules_service->GetTabHandler()->OnAllowAttributionChanged(web_contents());
  }
}

void RequestFilterTabHelper::WebContentsDestroyed() {
  adblock_filter::RuleService* rules_service =
      adblock_filter::RuleServiceFactory::GetForBrowserContext(
          web_contents()->GetBrowserContext());
  if (rules_service)
    rules_service->GetTabHandler()->OnTabRemoved(web_contents());
}

}  // namespace adblock_filter