// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/content/adblock_tab_state_and_logs_impl.h"

#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/ad_blocker/content/adblock_document_activations.h"
#include "components/ad_blocker/content/adblock_state_and_logs_impl.h"
#include "components/ad_blocker/content/interstitial/document_blocked_throttle.h"
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

void ApplyParentActivations(RuleGroup group,
                            const content::RenderFrameHost* parent_frame,
                            RulesIndex::ActivationResults& local_activations) {
  if (!parent_frame) {
    return;
  }

  RulesIndex::ActivationResults parent_activations =
      DocumentActivations::GetActivations(group, parent_frame);

  if (parent_activations.document_exception) {
    local_activations.document_exception = true;
  }

  for (const auto& [type, parent_activation] : parent_activations.by_type) {
    RulesIndex::ActivationResult& local_activation =
        local_activations.by_type[type];
    if ((local_activation.IsDecision(flat::Decision_MODIFY_IMPORTANT)))
      continue;

    CHECK(parent_activation.rule_details);

    if (!local_activation.rule_details ||
        local_activation.priority < parent_activation.priority) {
      local_activation.rule_details = parent_activation.rule_details;
      local_activation.from_parent = true;
    }
  }

  return;
}

TabStateAndLogs::RuleData MakeRuleData(uint32_t source_id,
                                       flat::Decision decision,
                                       std::string_view rule_text) {
  auto convert_decision = [](flat::Decision decision) {
    switch (decision) {
      case flat::Decision_MODIFY:
        return RequestFilterRule::kModify;
      case flat::Decision_PASS:
        return RequestFilterRule::kPass;
      case flat::Decision_MODIFY_IMPORTANT:
        return RequestFilterRule::kModifyImportant;
      default:
        NOTREACHED();
    }
  };

  TabStateAndLogs::RuleData rule_data;
  rule_data.rule_source_id = source_id;
  rule_data.decision = convert_decision(decision);
  rule_data.rule_text = rule_text;
  return rule_data;
}
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
                       RulesIndex::ActivationResults activations,
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
                          !activations.IsDocumentDecision(flat::Decision_PASS);

    disable_generic_rules_for_popunder_check_ =
        activations.by_type[flat::ActivationType_GENERIC_BLOCK].IsDecision(
            flat::Decision_PASS);

    if (!is_potential_popup_) {
      return;
    }

    DoPopupBlocking();
  }

  void UpdateOpenerUrl(GURL url,
                       url::Origin origin,
                       RulesIndex::ActivationResults activations) {
    if (opener_page_url_ == url) {
      return;
    }

    opener_page_url_ = url;
    opener_page_origin_ = origin;

    is_potential_popunder_ =
        !activations.IsDocumentDecision(flat::Decision_PASS);

    if (!is_potential_popunder_.value_or(false)) {
      return;
    }

    if (!opener_frame_origin_) {
      disable_generic_rules_for_popup_check_ =
          activations.by_type[flat::ActivationType_GENERIC_BLOCK].IsDecision(
              flat::Decision_PASS);
    }

    DoPopupBlocking();
  }

 private:
  void SelfDestruct() {
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

    if (!CanFilterUrl(target_url_) &&
        !target_url_.SchemeIs(url::kAboutScheme)) {
      return;
    }

    if (is_potential_popup_ &&
        state_and_logs_->IsPopup(
            group_, opener_frame_origin_.value_or(opener_page_origin_),
            target_url_, disable_generic_rules_for_popup_check_)) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&content::WebContents::Close, target_->GetWeakPtr()));
      // No need to self-destruct here. We'll be destroyed at the same time as
      // the target.
      self_destruct_.Reset();
      return;
    }

    if (opener_ && is_potential_popunder_ &&
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

void TabStateAndLogsImpl::OnBlockedNavigation(
    RuleGroup group,
    RulesIndex::RuleAndSource rule_and_source,
    int64_t navigation_id) {
  blocked_navigations_[static_cast<size_t>(group)][navigation_id] =
      MakeRuleData(
          rule_and_source.source_id, rule_and_source.rule->decision(),
          rule_and_source.rule->original_rule_text()
              ? rule_and_source.rule->original_rule_text()->string_view()
              : std::string_view());
}

void TabStateAndLogsImpl::OnUrlBlocked(RuleGroup group, GURL url) {
  TabBlockedUrlInfo& blocked_urls =
      !has_ongoing_navigation_ ? blocked_urls_[static_cast<size_t>(group)]
                               : new_blocked_urls_[static_cast<size_t>(group)];

  blocked_urls.total_count++;
  blocked_urls.blocked_urls[url.spec()].blocked_count++;
}

void TabStateAndLogsImpl::OnTrackerBlocked(RuleGroup group,
                                           const std::string& domain,
                                           const GURL& url) {
  TabBlockedUrlInfo& blocked_urls =
      !has_ongoing_navigation_ ? blocked_urls_[static_cast<size_t>(group)]
                               : new_blocked_urls_[static_cast<size_t>(group)];

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

bool TabStateAndLogsImpl::DoesAdAttributionMatch(
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
    if (!has_ongoing_navigation_) {
      allowed_attribution_trackers_.insert(std::string(tracker_url_spec));
    } else {
      new_allowed_attribution_trackers_.insert(std::string(tracker_url_spec));
    }
    return true;
  }

  return false;
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
  potential_popup_record_[static_cast<size_t>(group)] =
      std::make_unique<PotentialPopupRecord>(
          state_and_logs_, group, initial_url,
          initial_disable_generic_rules_for_popup_check,
          initial_disable_generic_rules_for_popunder_check, opener,
          opener_frame, web_contents(), std::move(on_destruct),
          base::BindOnce(
              [](base::WeakPtr<content::WebContents> web_contents,
                 RuleGroup group) {
                if (!web_contents) {
                  return;
                }
                TabStateAndLogsImpl::FromWebContents(web_contents.get())
                    ->potential_popup_record_[static_cast<size_t>(group)]
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

  auto& activations_for_navigation =
      activations_for_navigations_[navigation_handle->GetNavigationId()];
  RulesIndex::ActivationResults activations;
  if (activations_for_navigation[static_cast<size_t>(group)]
          .determined_for_url == navigation_handle->GetURL()) {
    activations =
        activations_for_navigation[static_cast<size_t>(group)].activations;
  }
  // If we don't have activations, either the index isn't loaded and
  // therefore, there won't be any blocking anyway, or the defaults
  // activations are what we want.

  // NOTE: The way we determine whether a click is trusted is probably not fully
  // correct. Ideally, VivaldiIsFromTrustedEvent should tell us the whole
  // picture, but there are some navigation scenarios where we just lose event
  // details too early. The code paths related to using guest view seem to
  // contribute to this issue. Testing for HasUserGesture in addition should
  // mitigate those scenarios to an extent.
  if (potential_popup_record_[static_cast<size_t>(group)]) {
    potential_popup_record_[static_cast<size_t>(group)]->UpdateTargetUrl(
        navigation_handle->GetURL(), activations,
        navigation_handle->VivaldiIsFromTrustedEvent() &&
            navigation_handle->HasUserGesture());
  }
  for (content::WebContents* potential_popup :
       potential_popups_[static_cast<size_t>(group)]) {
    TabStateAndLogsImpl::FromWebContents(potential_popup)
        ->potential_popup_record_[static_cast<size_t>(group)]
        ->UpdateOpenerUrl(navigation_handle->GetURL(),
                          url::Origin::Resolve(
                              navigation_handle->GetURL(),
                              navigation_handle->GetInitiatorOrigin().value_or(
                                  url::Origin())),
                          activations);
  }
}

RulesIndex::ActivationResults
TabStateAndLogsImpl::GetActivationsForLoadingFrame(
    RuleGroup group,
    int64_t navigation_id,
    const content::RenderFrameHost* parent_frame,
    const GURL& url) {
  UpdateActivationsForNavigation(group, navigation_id, parent_frame, url);

  const ActivationsDetails& details =
      activations_for_navigations_[navigation_id][static_cast<size_t>(group)];

  // This must have been enforced by the Update call above.
  CHECK_EQ(details.determined_for_url, url);
  return details.activations;
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
  return blocked_urls_[static_cast<size_t>(group)];
}

std::array<std::optional<TabStateAndLogs::RuleData>, kRuleGroupCount>
TabStateAndLogsImpl::WasNavigationBlocked(
    const content::NavigationHandle* navigation) const {
  std::array<std::optional<TabStateAndLogs::RuleData>, kRuleGroupCount> result;

  for (auto group : {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
    if (blocked_navigations_[static_cast<size_t>(group)].contains(
            navigation->GetNavigationId())) {
      result[static_cast<size_t>(group)] =
          blocked_navigations_[static_cast<size_t>(group)].at(
              navigation->GetNavigationId());
    }
  }
  return result;
}

TabStateAndLogs::TabActivations TabStateAndLogsImpl::GetTabActivations(
    RuleGroup group) const {
  auto convert_activation_type = [](flat::ActivationType activation_type) {
    switch (activation_type) {
      case flat::ActivationType_DOCUMENT:
        return RequestFilterRule::kWholeDocument;
      case flat::ActivationType_ELEMENT_HIDE:
        return RequestFilterRule::kElementHide;
      case flat::ActivationType_GENERIC_BLOCK:
        return RequestFilterRule::kGenericBlock;
      case flat::ActivationType_GENERIC_HIDE:
        return RequestFilterRule::kGenericHide;
      case flat::ActivationType_ATTRIBUTE_ADS:
        return RequestFilterRule::kAttributeAds;
      default:
        NOTREACHED();
    }
  };

  RulesIndex::ActivationResults activations =
      DocumentActivations::GetActivations(
          group, web_contents()->GetPrimaryMainFrame());
  TabStateAndLogs::TabActivations tab_activations;
  tab_activations.document_exception = activations.document_exception;

  for (const auto& [activation_type, activation_result] : activations.by_type) {
    TabStateAndLogs::TabActivationState state;
    state.from_parent = activation_result.from_parent;
    if (activation_result.rule_details) {
      state.rule_data = MakeRuleData(activation_result.rule_details->source_id,
                                     activation_result.rule_details->decision,
                                     std::string_view());
    }

    tab_activations.by_type.emplace(convert_activation_type(activation_type),
                                    state);
  }

  return tab_activations;
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
  // NOTE: `contents` might have already started loading. We need to call
  // didstart from here in case. This is only expected to happen for web
  // contents that do not belong to a tab.
  if (contents->HasUncommittedNavigationInPrimaryMainFrame()) {
    has_ongoing_navigation_ = true;
    HasStartedNavigation();
  }

  CHECK(state_and_logs);
}

void TabStateAndLogsImpl::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // No point in calculating this in bfcache navigation as it would just be
  // thrown away
  if (!navigation_handle->IsServedFromBackForwardCache()) {
    for (auto group :
         {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
      UpdateActivationsForNavigation(
          group, navigation_handle->GetNavigationId(),
          navigation_handle->GetParentFrame(), navigation_handle->GetURL());
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

  HasStartedNavigation();
}

void TabStateAndLogsImpl::HasStartedNavigation() {
  // Start recording blocked URLs from the beginning of the latest triggered
  // navigation. We might have cancelled ongoing navigations before starting
  // this one, so make sure we remove the records from any previous
  // navigation attempt.
  new_blocked_urls_ = std::array<TabBlockedUrlInfo, kRuleGroupCount>();
  new_allowed_attribution_trackers_.clear();
  ad_query_triggers_.clear();
}

void TabStateAndLogsImpl::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  // It wouldn't make sense for a redirect to happen when a document is served
  // from the bf cache
  CHECK(!navigation_handle->IsServedFromBackForwardCache());
  for (auto group : {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
    UpdateActivationsForNavigation(group, navigation_handle->GetNavigationId(),
                                   navigation_handle->GetParentFrame(),
                                   navigation_handle->GetURL());
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
    std::array<RulesIndex::ActivationResults, kRuleGroupCount>
        document_activations;
    for (auto group :
         {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
      // Run this one more time, in case this class was only instanciated very
      // late during navigation.
      UpdateActivationsForNavigation(
          group, navigation_handle->GetNavigationId(),
          navigation_handle->GetParentFrame(), navigation_handle->GetURL());
      UpdatePotentialPopup(group, navigation_handle);
      document_activations[static_cast<size_t>(group)] = std::move(
          activations_for_navigations_[navigation_handle->GetNavigationId()]
                                      [static_cast<size_t>(group)]
                                          .activations);
    }

    DocumentActivations::CreateForCurrentDocument(
        navigation_handle->GetRenderFrameHost(),
        std::move(document_activations));
    activations_for_navigations_.erase(navigation_handle->GetNavigationId());
  }

  if (navigation_handle->IsSameDocument() ||
      !navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  has_ongoing_navigation_ = false;

  if (!navigation_handle->HasCommitted()) {
    return;
  }

  ad_attribution_enabled_ = DocumentActivations::IsAdAttributionArmed(
      navigation_handle->GetRenderFrameHost());

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

  for (auto group : {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
    blocked_navigations_[static_cast<size_t>(group)].erase(
        navigation_handle->GetNavigationId());
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
    RulesIndex::ActivationResults opener_activations =
        DocumentActivations::GetActivations(group, source_render_frame_host);
    if (opener_activations.IsDocumentDecision(flat::Decision_PASS)) {
      continue;
    }

    std::optional<RulesIndex::ActivationResults> target_activations =
        state_and_logs_->GetLocalActivations(group, url::Origin::Create(url),
                                             url);
    bool initial_disable_generic_rules_for_popunder_check = false;
    if (target_activations) {
      if (target_activations->IsDocumentDecision(flat::Decision_PASS)) {
        continue;
      }
      initial_disable_generic_rules_for_popunder_check =
          target_activations->by_type[flat::ActivationType_GENERIC_BLOCK]
              .IsDecision(flat::Decision_PASS);
    }

    potential_popups_[static_cast<size_t>(group)].insert(new_contents);
    new_tab_helper->SetPotentialPopup(
        group, url,
        opener_activations.by_type[flat::ActivationType_GENERIC_BLOCK]
            .IsDecision(flat::Decision_PASS),
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
                  ->potential_popups_[static_cast<size_t>(group)]
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

void TabStateAndLogsImpl::UpdateActivationsForNavigation(
    RuleGroup group,
    int64_t navigation_id,
    const content::RenderFrameHost* parent_frame,
    const GURL& url) {
  if (activations_for_navigations_[navigation_id][static_cast<size_t>(group)]
          .determined_for_url == url) {
    // Our cache is current.
    return;
  }

  // If we are shutting down, activations don't matter anymore as everything
  // will be blocked. We'll just skip populating them.
  if (!state_and_logs_) {
    return;
  }

  ActivationsDetails new_activation_details;
  new_activation_details.determined_for_url = url;

  url::Origin parent_origin = parent_frame
                                  ? parent_frame->GetLastCommittedOrigin()
                                  : url::Origin::Create(url);

  std::optional<RulesIndex::ActivationResults> local_activations =
      state_and_logs_->GetLocalActivations(group, parent_origin, url);
  if (!local_activations) {
    // Indexes are not yet ready. This can happen when this is called as part
    // of handling navigations. In sucha  case, we'd rather not record
    // anything and wait until it's called again as part of the actual
    // request, which does wait for the indexes to be ready.
    return;
  }
  new_activation_details.activations = std::move(*local_activations);
  ApplyParentActivations(group, parent_frame,
                         new_activation_details.activations);

  activations_for_navigations_[navigation_id][static_cast<size_t>(group)] =
      std::move(new_activation_details);
}
}  // namespace adblock_filter