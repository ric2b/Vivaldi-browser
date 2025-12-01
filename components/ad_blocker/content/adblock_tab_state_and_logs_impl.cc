// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/content/adblock_tab_state_and_logs_impl.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/ad_blocker/content/adblock_document_state.h"
#include "components/ad_blocker/content/adblock_navigation_tracker_impl.h"
#include "components/ad_blocker/content/adblock_state_and_logs_impl.h"
#include "components/ad_blocker/content/interstitial/document_blocked_throttle.h"
#include "components/ad_blocker/content/utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace adblock_filter {
namespace {
constexpr base::TimeDelta kOffSiteTimeout = base::Minutes(30);
constexpr base::TimeDelta kAdAttributionExpiration = base::Days(7);

constexpr base::TimeDelta kPopupRecordLifetime = base::Seconds(10);
}  // namespace

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabStateAndLogsImpl);

class TabStateAndLogsImpl::PotentialPopupRecord {
 public:
  PotentialPopupRecord(base::WeakPtr<StateAndLogsImpl> state_and_logs,
                       RuleGroup group,
                       GURL initial_url,
                       bool disable_generic_rules_for_popup_check,
                       bool disable_generic_rules_for_popunder_check,
                       content::WebContents* opener,
                       content::RenderFrameHost* opener_frame,
                       content::WebContents* target,
                       base::OnceClosure on_destruct,
                       base::OnceClosure self_destruct)
      : state_and_logs_(state_and_logs),
        group_(group),
        target_url_(initial_url),
        opener_page_url_(opener->GetPrimaryMainFrame()->GetLastCommittedURL()),
        opener_page_origin_(
            opener->GetPrimaryMainFrame()->GetLastCommittedOrigin()),
        opener_(opener->GetWeakPtr()),
        target_(target),
        disable_generic_rules_for_popup_check_(
            disable_generic_rules_for_popup_check),
        disable_generic_rules_for_popunder_check_(
            disable_generic_rules_for_popunder_check),
        self_destruct_(std::move(self_destruct)),
        on_destruct_(std::move(on_destruct)) {
    GURL opener_frame_url = opener_frame->GetLastCommittedURL();

    // uBlock does this check when a change of the potential popup URL occurs
    // instead, and retains 'about:blank' if the frame already went away. This
    // might behave slightly differently, but might be more accurate.
    while (opener_frame_url == GURL("about:blank") &&
           opener_frame->GetParent() != nullptr) {
      opener_frame = opener_frame->GetParent();
      opener_frame_url = opener_frame->GetLastCommittedURL();
    }

    // If it's the primary main frame, rely on the web page url instead.
    if (!opener_frame->IsInPrimaryMainFrame()) {
      opener_frame_origin_ = opener_frame->GetLastCommittedOrigin();
    }

    expiration_.Start(FROM_HERE, kPopupRecordLifetime,
                      base::BindOnce(&PotentialPopupRecord::SelfDestruct,
                                     base::Unretained(this)));
  }
  ~PotentialPopupRecord() { std::move(on_destruct_).Run(); }

  PotentialPopupRecord(const PotentialPopupRecord&) = delete;
  PotentialPopupRecord& operator=(const PotentialPopupRecord&) = delete;

  void UpdateTargetUrl(GURL url,
                       const ActivationResults& activations,
                       bool from_trusted_click) {
    if (close_opener_on_target_navigation_) {
      CloseOpener();
      return;
    }

    if (target_url_ == url && seen_target_navigation_) {
      // Avoid double-work
      return;
    }

    if (!seen_target_navigation_ && target_url_ != url && !from_trusted_click) {
      // We still need to do the popup check for the initial URL. It can't be
      // done on initialization, since we lack trusted click details.
      DoPopupBlocking();
    }

    seen_target_navigation_ = true;

    target_url_ = url;

    is_potential_popup_ = !from_trusted_click &&
                          !activations.IsDocumentDecision(RuleDecision::kPass);

    disable_generic_rules_for_popunder_check_ =
        activations.by_type[ActivationType::kGenericBlock].IsDecision(
            RuleDecision::kPass);

    if (!is_potential_popup_) {
      return;
    }

    DoPopupBlocking();
  }

  void UpdateOpenerUrl(GURL url,
                       url::Origin origin,
                       const ActivationResults& activations) {
    if (opener_page_url_ == url) {
      return;
    }

    opener_page_url_ = url;
    opener_page_origin_ = origin;

    is_potential_popunder_ =
        !activations.IsDocumentDecision(RuleDecision::kPass);

    if (!is_potential_popunder_.value_or(false)) {
      return;
    }

    if (!opener_frame_origin_) {
      disable_generic_rules_for_popup_check_ =
          activations.by_type[ActivationType::kGenericBlock].IsDecision(
              RuleDecision::kPass);
    }

    DoPopupBlocking();
  }

 private:
  void SelfDestruct() {
    expiration_.Stop();
    // Wait until whatever called us is done, in case we are still needed.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(self_destruct_));
  }

  void DoPopupBlocking() {
    if (self_destruct_.is_null()) {
      // We self-destructed as part of running an earlier popup check, so we are
      // done.
      return;
    }

    if (!state_and_logs_) {
      SelfDestruct();
      return;
    }

    if (is_potential_popup_ && CanFilterUrl(target_url_, true) &&
        state_and_logs_->IsPopup(
            group_, opener_frame_origin_.value_or(opener_page_origin_),
            target_url_, disable_generic_rules_for_popup_check_)) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&content::WebContents::Close, target_->GetWeakPtr()));
      // No need to self-destruct here. We'll be destroyed at the same time as
      // the target.
      expiration_.Stop();
      self_destruct_.Reset();
      return;
    }

    if (opener_ && is_potential_popunder_ &&
        CanFilterUrl(opener_page_url_, true) &&
        state_and_logs_->IsPopunder(
            group_, opener_page_url_, url::Origin::Create(target_url_),
            disable_generic_rules_for_popunder_check_)) {
      if (seen_target_navigation_) {
        CloseOpener();
      } else {
        // Chromium doesn't like (runs into DCHECKS) having the opener go away
        // before the target had a chance to navigate. We instead close the
        // opener as soon as the target starts navigating.
        close_opener_on_target_navigation_ = true;
      }
      return;
    }

    expiration_.Reset();
  }

  void CloseOpener() {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&content::WebContents::Close, opener_->GetWeakPtr()));
    SelfDestruct();
  }

  base::WeakPtr<StateAndLogsImpl> state_and_logs_;
  RuleGroup group_;
  GURL target_url_;
  GURL opener_page_url_;
  url::Origin opener_page_origin_;
  std::optional<url::Origin> opener_frame_origin_;
  base::WeakPtr<content::WebContents> opener_;
  raw_ptr<content::WebContents> target_;
  bool seen_target_navigation_ = false;
  // Set to false initially as we want to wait until the navigation has started
  // to know if it's from a trusted link, before making any decision.
  bool is_potential_popup_ = false;
  // This gets set only once there is a url change for the opener. Until there
  // is, we assume this is not a popunder.
  std::optional<bool> is_potential_popunder_;
  bool disable_generic_rules_for_popup_check_;
  bool disable_generic_rules_for_popunder_check_;
  bool close_opener_on_target_navigation_ = false;

  base::OnceClosure self_destruct_;
  base::OnceClosure on_destruct_;
  base::OneShotTimer expiration_;
};

TabStateAndLogsImpl::~TabStateAndLogsImpl() = default;

void TabStateAndLogsImpl::OnUrlBlocked(RuleGroup group, GURL url) {
  TabBlockedUrlInfo& blocked_urls = !has_ongoing_navigation_
                                        ? blocked_urls_[group]
                                        : new_blocked_urls_[group];

  blocked_urls.total_count++;
  blocked_urls.blocked_urls[url.spec()].blocked_count++;
}

void TabStateAndLogsImpl::OnTrackerBlocked(RuleGroup group,
                                           const std::string& domain,
                                           const GURL& url) {
  TabBlockedUrlInfo& blocked_urls = !has_ongoing_navigation_
                                        ? blocked_urls_[group]
                                        : new_blocked_urls_[group];

  blocked_urls.total_count++;
  BlockedTrackerInfo& blocked_tracker = blocked_urls.blocked_trackers[domain];
  blocked_tracker.blocked_count++;
  blocked_tracker.blocked_urls[url.spec()].blocked_count++;
}

void TabStateAndLogsImpl::SetAdQueryTriggers(
    const GURL& ad_url,
    std::vector<std::string> triggers) {
  if (!ad_attribution_enabled_ || !has_ongoing_navigation_) {
    return;
  }

  ResetAdAttribution();
  ad_click_time_ = base::TimeTicks::Now();
  current_ad_click_domain_ = ad_url.host_piece();
  ad_query_triggers_.swap(triggers);

  // Only the first matching ad-query-trigger rule should be used. This
  // prevents further matches to succeed.
  ad_attribution_enabled_ = false;
}

std::optional<RulesIndex::AdAttributionMatchParams>
TabStateAndLogsImpl::GetAdAttributionMatchParams() const {
  if (current_ad_landing_domain_.empty() || !is_on_ad_landing_site_) {
    return std::nullopt;
  }

  return RulesIndex::AdAttributionMatchParams{
      .ad_trigger_ = current_ad_trigger_,
      .ad_click_domain_ = current_ad_click_domain_};
}

void TabStateAndLogsImpl::OnMatchedAttributionTracker(const GURL& url) {
  if (!has_ongoing_navigation_) {
    allowed_attribution_trackers_.insert(std::string(url.spec()));
  } else {
    new_allowed_attribution_trackers_.insert(std::string(url.spec()));
  }
}

void TabStateAndLogsImpl::SetPotentialPopup(
    RuleGroup group,
    GURL initial_url,
    bool initial_disable_generic_rules_for_popup_check,
    bool initial_disable_generic_rules_for_popunder_check,
    content::WebContents* opener,
    content::RenderFrameHost* opener_frame,
    base::OnceClosure on_destruct) {
  // Unretained is OK, since we own the record and it owns the closure.
  potential_popup_record_[group] = std::make_unique<PotentialPopupRecord>(
      state_and_logs_, group, initial_url,
      initial_disable_generic_rules_for_popup_check,
      initial_disable_generic_rules_for_popunder_check, opener, opener_frame,
      web_contents(), std::move(on_destruct),
      base::BindOnce(
          [](base::WeakPtr<content::WebContents> web_contents,
             RuleGroup group) {
            if (!web_contents) {
              return;
            }
            TabStateAndLogsImpl::FromWebContents(web_contents.get())
                ->potential_popup_record_[group]
                .reset();
          },
          web_contents()->GetWeakPtr(), group));
}

void TabStateAndLogsImpl::UpdatePotentialPopup(
    RuleGroup group,
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame()) {
    return;
  }

  const ActivationResults& activations =
      NavigationTrackerImpl::GetForNavigationHandle(*navigation_handle)
          ->GetActivations(group);

  // NOTE: The way we determine whether a click is trusted is probably not fully
  // correct. Ideally, VivaldiIsFromTrustedEvent should tell us the whole
  // picture, but there are some navigation scenarios where we just lose event
  // details too early. The code paths related to using guest view seem to
  // contribute to this issue. Testing for HasUserGesture in addition should
  // mitigate those scenarios to an extent.
  if (potential_popup_record_[group]) {
    potential_popup_record_[group]->UpdateTargetUrl(
        navigation_handle->GetURL(), activations,
        navigation_handle->VivaldiIsFromTrustedEvent() &&
            navigation_handle->HasUserGesture());
  }
  for (content::WebContents* potential_popup : potential_popups_[group]) {
    TabStateAndLogsImpl::FromWebContents(potential_popup)
        ->potential_popup_record_[group]
        ->UpdateOpenerUrl(navigation_handle->GetURL(),
                          url::Origin::Resolve(
                              navigation_handle->GetURL(),
                              navigation_handle->GetInitiatorOrigin().value_or(
                                  url::Origin())),
                          activations);
  }
}

const std::string& TabStateAndLogsImpl::GetCurrentAdLandingDomain() const {
  return current_ad_landing_domain_;
}
const std::set<std::string>&
TabStateAndLogsImpl::GetAllowedAttributionTrackers() const {
  return allowed_attribution_trackers_;
}
bool TabStateAndLogsImpl::IsOnAdLandingSite() const {
  return is_on_ad_landing_site_;
}

const TabStateAndLogs::TabBlockedUrlInfo&
TabStateAndLogsImpl::GetBlockedUrlsInfo(RuleGroup group) const {
  return blocked_urls_[group];
}

void TabStateAndLogsImpl::MaybeAddNavigationThrottle(
    content::NavigationThrottleRegistry& registry) const {
  if (registry.GetNavigationHandle().IsInMainFrame()) {
    registry.AddThrottle(std::make_unique<DocumentBlockedThrottle>(registry));
  }
}

TabStateAndLogsImpl::TabStateAndLogsImpl(
    content::WebContents* contents,
    base::WeakPtr<StateAndLogsImpl> state_and_logs)
    : WebContentsObserver(contents),
      WebContentsUserData<TabStateAndLogsImpl>(*contents),
      state_and_logs_(state_and_logs) {
  // We expect to have this registered for every single web content upon
  // creation, before any navigation can occur.
  CHECK(!contents->HasUncommittedNavigationInPrimaryMainFrame());
  CHECK(state_and_logs);
}

void TabStateAndLogsImpl::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  NavigationTrackerImpl::CreateForNavigationHandle(*navigation_handle,
                                                   state_and_logs_);
  // No point in calculating this in bfcache navigation as it would just be
  // thrown away
  if (!navigation_handle->IsServedFromBackForwardCache()) {
    for (auto group :
         {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
      UpdatePotentialPopup(group, navigation_handle);
    }
  }

  if (navigation_handle->IsSameDocument() ||
      !navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  has_ongoing_navigation_ = true;

  // Whether the navigation was initiated by the renderer process. Examples of
  // renderer-initiated navigations include:
  //  * <a> link click
  //  * changing window.location.href
  //  * redirect via the <meta http-equiv="refresh"> tag
  //  * using window.history.pushState

  bool is_renderer_initiated_load = navigation_handle->IsRendererInitiated();
  bool is_user_gesture = navigation_handle->HasUserGesture();
  if ((navigation_handle->GetPageTransition() &
       ui::PAGE_TRANSITION_IS_REDIRECT_MASK) ||
      (is_renderer_initiated_load && !is_user_gesture)) {
    DoQueryTriggerCheck(navigation_handle->GetURL());
    return;
  }

  // Start recording blocked URLs from the beginning of the latest triggered
  // navigation. We might have cancelled ongoing navigations before starting
  // this one, so make sure we remove the records from any previous
  // navigation attempt.
  new_blocked_urls_ = RuleGroupArray<TabBlockedUrlInfo>();
  new_allowed_attribution_trackers_.clear();
  ad_query_triggers_.clear();
}

void TabStateAndLogsImpl::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  // It wouldn't make sense for a redirect to happen when a document is served
  // from the bf cache
  CHECK(!navigation_handle->IsServedFromBackForwardCache());
  for (auto group : {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
    UpdatePotentialPopup(group, navigation_handle);
  }

  if (navigation_handle->IsSameDocument() ||
      !navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  DoQueryTriggerCheck(navigation_handle->GetURL());
}

void TabStateAndLogsImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->HasCommitted() && !navigation_handle->IsErrorPage() &&
      !navigation_handle->IsServedFromBackForwardCache()) {
    for (auto group :
         {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
      // Run this one more time, in case this class was only instanciated very
      // late during navigation.
      UpdatePotentialPopup(group, navigation_handle);
    }
  }

  if (navigation_handle->HasCommitted()) {
    DocumentState::CreateForCurrentDocument(
        navigation_handle->GetRenderFrameHost(), navigation_handle);
  }

  if (navigation_handle->IsSameDocument() ||
      !navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  has_ongoing_navigation_ = false;

  if (!navigation_handle->HasCommitted()) {
    return;
  }

  ad_attribution_enabled_ = false;
  for (auto group : {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
    if (DocumentState::GetActivations(group,
                                      navigation_handle->GetRenderFrameHost())
            .by_type[ActivationType::kAttributeAds]
            .IsDecision(RuleDecision::kPass)) {
      ad_attribution_enabled_ = true;
    }
  }

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
}

void TabStateAndLogsImpl::DidOpenRequestedURL(
    content::WebContents* new_contents,
    content::RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  if (disposition != WindowOpenDisposition::SINGLETON_TAB &&
      disposition != WindowOpenDisposition::NEW_FOREGROUND_TAB &&
      disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB &&
      disposition != WindowOpenDisposition::NEW_POPUP &&
      disposition != WindowOpenDisposition::NEW_WINDOW &&
      disposition != WindowOpenDisposition::OFF_THE_RECORD) {
    return;  // We didn't actually open a new window or tab.
  }

  TabStateAndLogsImpl::CreateForWebContents(new_contents, state_and_logs_);
  auto* new_tab_helper = TabStateAndLogsImpl::FromWebContents(new_contents);

  new_tab_helper->ad_attribution_enabled_ = ad_attribution_enabled_;

  if (!current_ad_landing_domain_.empty() &&
      current_ad_landing_domain_ ==
          net::registry_controlled_domains::GetDomainAndRegistry(
              url,
              net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    new_tab_helper->current_ad_click_domain_ = current_ad_click_domain_;
    new_tab_helper->ad_click_time_ = ad_click_time_;
    new_tab_helper->current_ad_trigger_ = current_ad_trigger_;
    new_tab_helper->current_ad_landing_domain_ = current_ad_landing_domain_;
    new_tab_helper->is_on_ad_landing_site_ = true;
    new_tab_helper->last_attributed_navigation_ = base::TimeTicks::Now();
    new_tab_helper->ad_attribution_expiration_.Start(
        FROM_HERE,
        base::TimeTicks::Now() - ad_click_time_ + kAdAttributionExpiration,
        base::BindOnce(&TabStateAndLogsImpl::ResetAdAttribution,
                       base::Unretained(new_tab_helper)));

    if (state_and_logs_) {
      state_and_logs_->OnAllowAttributionChanged(new_contents);
    }
  }

  if (web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL() ==
          GURL("about:newtab") ||
      !renderer_initiated || started_from_context_menu) {
    return;
  }
  for (auto group : {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
    const ActivationResults& opener_activations =
        DocumentState::GetActivations(group, source_render_frame_host);
    if (opener_activations.IsDocumentDecision(RuleDecision::kPass)) {
      continue;
    }

    std::optional<ActivationResults> target_activations =
        state_and_logs_->GetLocalActivations(group, url::Origin::Create(url),
                                             url);
    bool initial_disable_generic_rules_for_popunder_check = false;
    if (target_activations) {
      if (target_activations->IsDocumentDecision(RuleDecision::kPass)) {
        continue;
      }
      initial_disable_generic_rules_for_popunder_check =
          target_activations->by_type[ActivationType::kGenericBlock].IsDecision(
              RuleDecision::kPass);
    }

    potential_popups_[group].insert(new_contents);
    new_tab_helper->SetPotentialPopup(
        group, url,
        opener_activations.by_type[ActivationType::kGenericBlock].IsDecision(
            RuleDecision::kPass),
        initial_disable_generic_rules_for_popunder_check, web_contents(),
        source_render_frame_host,
        base::BindOnce(
            [](RuleGroup group,
               base::WeakPtr<content::WebContents> web_contents,
               content::WebContents* potential_popup) {
              if (!web_contents) {
                return;
              }
              TabStateAndLogsImpl::FromWebContents(web_contents.get())
                  ->potential_popups_[group]
                  .erase(potential_popup);
            },
            group, web_contents()->GetWeakPtr(), new_contents));
  }
}

void TabStateAndLogsImpl::WebContentsDestroyed() {
  if (state_and_logs_) {
    state_and_logs_->OnTabRemoved(web_contents());
  }
}

void TabStateAndLogsImpl::DoQueryTriggerCheck(const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS() || !url.has_host())
    return;

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
            base::BindOnce(&TabStateAndLogsImpl::ResetAdAttribution,
                           base::Unretained(this)));
        if (state_and_logs_) {
          state_and_logs_->OnAllowAttributionChanged(web_contents());
        }
      }
      return;
    }
  }
}

void TabStateAndLogsImpl::ResetAdAttribution() {
  ad_click_time_ = base::TimeTicks();
  current_ad_click_domain_.clear();
  current_ad_trigger_.clear();
  current_ad_landing_domain_.clear();
  last_attributed_navigation_ = base::TimeTicks();
  is_on_ad_landing_site_ = false;
  ad_attribution_expiration_.Stop();

  if (state_and_logs_) {
    state_and_logs_->OnAllowAttributionChanged(web_contents());
  }
}

void TabStateAndLogsImpl::SetIsOnAdLandingSite(bool is_on_ad_landing_site) {
  bool was_on_ad_landing_site = is_on_ad_landing_site_;
  is_on_ad_landing_site_ = is_on_ad_landing_site;

  if (is_on_ad_landing_site != was_on_ad_landing_site && state_and_logs_) {
    state_and_logs_->OnAllowAttributionChanged(web_contents());
  }
}

}  // namespace adblock_filter
