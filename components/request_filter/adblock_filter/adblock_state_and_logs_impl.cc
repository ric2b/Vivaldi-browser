// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/adblock_state_and_logs_impl.h"

#include <optional>

#include "base/stl_util.h"
#include "components/ad_blocker/adblock_rule_manager.h"
#include "components/ad_blocker/adblock_stats_data.h"
#include "components/ad_blocker/adblock_types.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_impl.h"
#include "components/request_filter/adblock_filter/adblock_tab_state_and_logs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace adblock_filter {
namespace {
const int kSecondsBetweenNotifications = 1;

constexpr base::TimeDelta kOffSiteTimeout = base::Minutes(30);
constexpr base::TimeDelta kAdAttributionExpiration = base::Days(7);

TabStateAndLogs::RuleData MakeRuleData(
    RulesIndex::RuleAndSource rule_and_source) {
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
  rule_data.rule_source_id = rule_and_source.source_id;
  rule_data.decision = convert_decision(rule_and_source.rule->decision());
  if (rule_and_source.rule->original_rule_text()) {
    rule_data.rule_text =
        rule_and_source.rule->original_rule_text()->string_view();
  }
  return rule_data;
}

class TabStateAndLogsImpl
    : public TabStateAndLogs,
      public content::WebContentsObserver,
      public content::WebContentsUserData<TabStateAndLogsImpl> {
 public:
  ~TabStateAndLogsImpl() override = default;
  TabStateAndLogsImpl(const TabStateAndLogsImpl&) = delete;
  TabStateAndLogsImpl& operator=(const TabStateAndLogsImpl&) = delete;

  void SetFrameBlockState(RuleGroup group,
                          RulesIndex::RuleAndSource rule_and_source,
                          content::FrameTreeNodeId frame_tree_node_id) {
    blocked_frames_[static_cast<size_t>(group)][frame_tree_node_id] =
        MakeRuleData(rule_and_source);
  }

  void ResetFrameBlockState(RuleGroup group,
                            content::FrameTreeNodeId frame_tree_node_id) {
    blocked_frames_[static_cast<size_t>(group)].erase(frame_tree_node_id);
  }

  void OnUrlBlocked(RuleGroup group, GURL url) {
    TabBlockedUrlInfo& blocked_urls =
        !has_ongoing_navigations_
            ? blocked_urls_[static_cast<size_t>(group)]
            : new_blocked_urls_[static_cast<size_t>(group)];

    blocked_urls.total_count++;
    blocked_urls.blocked_urls[url.spec()].blocked_count++;
  }

  void OnTrackerBlocked(RuleGroup group,
                        const std::string& domain,
                        const GURL& url) {
    TabBlockedUrlInfo& blocked_urls =
        !has_ongoing_navigations_
            ? blocked_urls_[static_cast<size_t>(group)]
            : new_blocked_urls_[static_cast<size_t>(group)];

    blocked_urls.total_count++;
    BlockedTrackerInfo& blocked_tracker = blocked_urls.blocked_trackers[domain];
    blocked_tracker.blocked_count++;
    blocked_tracker.blocked_urls[url.spec()].blocked_count++;
  }

  void ArmAdAttribution() {
    // Avoid enabling Ad Attribution as a result of preloading. See the comment
    // in LogTabActivation for more details.
    if (has_ongoing_navigations_)
      new_ad_attribution_enabled_ = true;
  }

  void SetAdQueryTriggers(const GURL& ad_url,
                          std::vector<std::string> triggers) {
    if (!ad_attribution_enabled_ || !has_ongoing_navigations_) {
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

  bool DoesAdAttributionMatch(std::string_view tracker_url_spec,
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
      if (!has_ongoing_navigations_) {
        allowed_attribution_trackers_.insert(std::string(tracker_url_spec));
      } else {
        new_allowed_attribution_trackers_.insert(std::string(tracker_url_spec));
      }
      return true;
    }

    return false;
  }

  void LogTabActivations(RuleGroup group, TabActivations states) {
    // Tab Activations are normally be set when loading the main frame,
    // so a navigation should be ongoing. On some websites (e.g.: Google), we
    // may be receiving these because of one or another form of preloading.
    // These should be ignored to ensure the reported tab activations match the
    // currrently loaded page.
    if (has_ongoing_navigations_) {
      did_set_activation_states_[static_cast<size_t>(group)] = true;
      new_tab_activation_states_[static_cast<size_t>(group)] =
          std::move(states);
    }
  }

  const std::string& GetCurrentAdLandingDomain() const override {
    return current_ad_landing_domain_;
  }
  const std::set<std::string>& GetAllowedAttributionTrackers() const override {
    return allowed_attribution_trackers_;
  }
  bool IsOnAdLandingSite() const override { return is_on_ad_landing_site_; }

  const TabBlockedUrlInfo& GetBlockedUrlsInfo(RuleGroup group) const override {
    return blocked_urls_[static_cast<size_t>(group)];
  }

  std::array<std::optional<TabStateAndLogs::RuleData>, kRuleGroupCount>
  WasFrameBlocked(content::FrameTreeNodeId frame_tree_node_id) const override {
    std::array<std::optional<TabStateAndLogs::RuleData>, kRuleGroupCount>
        result;

    for (auto group :
         {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
      if (blocked_frames_[static_cast<size_t>(group)].contains(
              frame_tree_node_id)) {
        result[static_cast<size_t>(group)] =
            blocked_frames_[static_cast<size_t>(group)].at(frame_tree_node_id);
      }
    }
    return result;
  }

  const TabActivations& GetTabActivations(RuleGroup group) const override {
    return tab_activation_states_[static_cast<size_t>(group)];
  }

 private:
  friend class content::WebContentsUserData<TabStateAndLogsImpl>;

  TabStateAndLogsImpl(content::WebContents* contents,
                      base::WeakPtr<StateAndLogsImpl> state_and_logs)
      : WebContentsObserver(contents),
        WebContentsUserData<TabStateAndLogsImpl>(*contents),
        state_and_logs_(state_and_logs) {
    // NOTE: |contents| might have already started loading. We need to call
    // didstart from here in case.
    if (contents->IsWaitingForResponse()) {
      has_ongoing_navigations_ = true;
      HasStartedNavigation();
    }

    CHECK(state_and_logs);
  }

  // content::WebContentsObserver implementation
  void FrameDeleted(content::FrameTreeNodeId frame_tree_node_id) override {
    for (auto group :
         {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
      ResetFrameBlockState(group, frame_tree_node_id);
    }
  }

  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->IsInPrimaryMainFrame() ||
        navigation_handle->IsSameDocument())
      return;

    has_ongoing_navigations_ = true;

    // Whether the navigation was initiated by the renderer process. Examples of
    // renderer-initiated navigations include:
    //  * <a> link click
    //  * changing window.location.href
    //  * redirect via the <meta http-equiv="refresh"> tag
    //  * using window.history.pushState

    bool is_user_gesture = navigation_handle->HasUserGesture();

    bool is_renderer_initiated_load = navigation_handle->IsRendererInitiated();
    if ((navigation_handle->GetPageTransition() &
         ui::PAGE_TRANSITION_IS_REDIRECT_MASK) ||
        (is_renderer_initiated_load && !is_user_gesture)) {
      DoQueryTriggerCheck(navigation_handle->GetURL());
      return;
    }

    HasStartedNavigation();
  }

  void HasStartedNavigation() {
    // Start recording blocked URLs from the beginning of the latest triggered
    // navigation. We might have cancelled ongoing navigations before starting
    // this one, so make sure we remove the records from any previous
    // navigation attempt.
    new_blocked_urls_ = std::array<TabBlockedUrlInfo, kRuleGroupCount>();
    new_ad_attribution_enabled_ = false;
    new_allowed_attribution_trackers_.clear();
    new_tab_activation_states_ = std::array<TabActivations, kRuleGroupCount>();
    ad_query_triggers_.clear();
    did_set_activation_states_ = {false, false};
  }

  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->IsInPrimaryMainFrame() ||
        navigation_handle->IsSameDocument())
      return;

    DoQueryTriggerCheck(navigation_handle->GetURL());
  }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->IsInPrimaryMainFrame() ||
        navigation_handle->IsSameDocument())
      return;

    if (!navigation_handle->HasCommitted()) {
      has_ongoing_navigations_ = false;
      return;
    }

    for (auto group :
         {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
      if (!did_set_activation_states_[static_cast<size_t>(group)]) {
        state_and_logs_->ComputeMissingActivationsForNavigation(
            group, navigation_handle->GetRenderFrameHost());
      }
    }

    has_ongoing_navigations_ = false;

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
    tab_activation_states_.swap(new_tab_activation_states_);
  }

  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override {
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

      state_and_logs_->OnAllowAttributionChanged(new_contents);
    }
  }

  void WebContentsDestroyed() override {
    state_and_logs_->OnTabRemoved(web_contents());
  }

  void DoQueryTriggerCheck(const GURL& url) {
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
              base::TimeTicks::Now() - ad_click_time_ +
                  kAdAttributionExpiration,
              base::BindOnce(&TabStateAndLogsImpl::ResetAdAttribution,
                             base::Unretained(this)));
          state_and_logs_->OnAllowAttributionChanged(web_contents());
        }
        return;
      }
    }
  }

  void ResetAdAttribution() {
    ad_click_time_ = base::TimeTicks();
    current_ad_click_domain_.clear();
    current_ad_trigger_.clear();
    current_ad_landing_domain_.clear();
    last_attributed_navigation_ = base::TimeTicks();
    is_on_ad_landing_site_ = false;
    ad_attribution_expiration_.Stop();

    state_and_logs_->OnAllowAttributionChanged(web_contents());
  }

  void SetIsOnAdLandingSite(bool is_on_ad_landing_site) {
    bool was_on_ad_landing_site = is_on_ad_landing_site_;
    is_on_ad_landing_site_ = is_on_ad_landing_site;

    if (is_on_ad_landing_site != was_on_ad_landing_site) {
      state_and_logs_->OnAllowAttributionChanged(web_contents());
    }
  }

  const base::WeakPtr<StateAndLogsImpl> state_and_logs_;

  std::array<std::map<content::FrameTreeNodeId, RuleData>, kRuleGroupCount>
      blocked_frames_;
  std::set<std::string> allowed_attribution_trackers_;
  std::set<std::string> new_allowed_attribution_trackers_;

  bool has_ongoing_navigations_ = false;
  std::array<TabBlockedUrlInfo, kRuleGroupCount> blocked_urls_;
  std::array<TabBlockedUrlInfo, kRuleGroupCount> new_blocked_urls_;

  std::array<TabActivations, kRuleGroupCount> tab_activation_states_;
  std::array<TabActivations, kRuleGroupCount> new_tab_activation_states_;

  // Should we check if the next load is an ad?
  bool ad_attribution_enabled_ = false;
  bool new_ad_attribution_enabled_ = false;

  std::array<bool, kRuleGroupCount> did_set_activation_states_ = {false, false};

  // Information related to clicked ad.
  std::string current_ad_click_domain_;
  std::vector<std::string> ad_query_triggers_;
  base::TimeTicks ad_click_time_;

  // Ad attribution settings, once a trigger was matched.
  std::string current_ad_trigger_;
  std::string current_ad_landing_domain_;
  base::TimeTicks last_attributed_navigation_;
  bool is_on_ad_landing_site_ = false;
  base::OneShotTimer ad_attribution_expiration_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabStateAndLogsImpl);

struct FrameInfo {
  content::WebContents* web_contents;
  bool is_off_the_record;
  TabStateAndLogsImpl* tab_helper;
};

// Allow passing a null `state_and_logs` if `create_helper_if_needed`, to allow
// this being called from const methods.
std::optional<FrameInfo> GetFrameInfo(
    base::WeakPtr<StateAndLogsImpl> state_and_logs,
    bool create_helper_if_needed,
    content::RenderFrameHost* frame,
    bool allow_off_the_record) {
  CHECK(state_and_logs || !create_helper_if_needed);
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
  if (create_helper_if_needed) {
    TabStateAndLogsImpl::CreateForWebContents(web_contents, state_and_logs);
  }

  TabStateAndLogsImpl* tab_helper =
      TabStateAndLogsImpl::FromWebContents(web_contents);

  if (!tab_helper) {
    return std::nullopt;
  }

  return FrameInfo{web_contents, is_off_the_record, tab_helper};
}

}  // namespace

StateAndLogsImpl::StateAndLogsImpl(RuleServiceImpl* rules_service,
                                   base::RepeatingClosure schedule_save)
    : schedule_save_(schedule_save), rules_service_(rules_service) {}

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

void StateAndLogsImpl::SetFrameBlockState(
    RuleGroup group,
    RulesIndex::RuleAndSource rule_and_source,
    content::RenderFrameHost* frame) {
  std::optional<FrameInfo> frame_info =
      GetFrameInfo(weak_factory_.GetWeakPtr(), true, frame, false);
  if (!frame_info) {
    return;
  }

  frame_info->tab_helper->SetFrameBlockState(group, rule_and_source,
                                             frame->GetFrameTreeNodeId());
}

void StateAndLogsImpl::ResetFrameBlockState(RuleGroup group,
                                            content::RenderFrameHost* frame) {
  std::optional<FrameInfo> frame_info =
      GetFrameInfo(weak_factory_.GetWeakPtr(), true, frame, false);
  if (!frame_info) {
    return;
  }

  frame_info->tab_helper->ResetFrameBlockState(group,
                                               frame->GetFrameTreeNodeId());
}

void StateAndLogsImpl::ReportTabActivations(
    RuleGroup group,
    content::RenderFrameHost* frame,
    const RulesIndex::ActivationResults& activations) {
  if (!frame || !frame->IsInPrimaryMainFrame()) {
    // Only log this for the top level frame, for the time
    return;
  }

  std::optional<FrameInfo> frame_info =
      GetFrameInfo(weak_factory_.GetWeakPtr(), true, frame, false);
  if (!frame_info) {
    return;
  }

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

  TabStateAndLogs::TabActivations logged_activations;
  logged_activations.document_exception = activations.document_exception;

  for (const auto& [activation_type, activation_result] : activations.by_type) {
    if (activation_type == flat::ActivationType_ATTRIBUTE_ADS &&
        activation_result.GetDecision().value_or(flat::Decision_MODIFY) ==
            flat::Decision_PASS) {
      frame_info->tab_helper->ArmAdAttribution();
    }

    TabStateAndLogs::TabActivationState state;
    state.from_parent = activation_result.from_parent;
    if (activation_result.rule_and_source) {
      state.rule_data = MakeRuleData(*activation_result.rule_and_source);
    }

    logged_activations.by_type.emplace(convert_activation_type(activation_type),
                                       state);
  }

  frame_info->tab_helper->LogTabActivations(group,
                                            std::move(logged_activations));
}

void StateAndLogsImpl::OnUrlBlocked(RuleGroup group,
                                    url::Origin origin,
                                    GURL url,
                                    content::RenderFrameHost* frame) {
  std::optional<FrameInfo> frame_info =
      GetFrameInfo(weak_factory_.GetWeakPtr(), true, frame, true);
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

  if (!is_off_the_record)
    AddToCounter(group, url, origin.host());

  tabs_with_new_blocks_[static_cast<size_t>(group)].insert(web_contents);

  PrepareNewNotifications();
}

void StateAndLogsImpl::SetTabAdQueryTriggers(
    const GURL& ad_url,
    std::vector<std::string> ad_query_triggers,
    content::RenderFrameHost* frame) {
  std::optional<FrameInfo> frame_info =
      GetFrameInfo(weak_factory_.GetWeakPtr(), true, frame, false);
  if (!frame_info || frame_info->web_contents->GetPrimaryMainFrame() != frame) {
    return;
  }
  frame_info->tab_helper->SetAdQueryTriggers(ad_url,
                                             std::move(ad_query_triggers));
}

bool StateAndLogsImpl::DoesAdAttributionMatch(
    content::RenderFrameHost* frame,
    std::string_view tracker_url_spec,
    std::string_view ad_domain_and_query_trigger) {
  std::optional<FrameInfo> frame_info =
      GetFrameInfo(weak_factory_.GetWeakPtr(), true, frame, false);
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

void StateAndLogsImpl::AddToCounter(RuleGroup group,
                                    const GURL& host,
                                    const std::string& origin_host) {
  auto* stats_store = rules_service_->GetStatsStore();
  if (stats_store && host.has_host()) {
    stats_store->AddEntry(host, origin_host, base::Time::Now(), group);
  }
}

std::array<std::optional<TabStateAndLogs::RuleData>, kRuleGroupCount>
StateAndLogsImpl::WasFrameBlocked(content::RenderFrameHost* frame) const {
  std::optional<FrameInfo> frame_info =
      GetFrameInfo(nullptr, false, frame, false);
  if (!frame_info) {
    return {std::nullopt, std::nullopt};
  }

  return frame_info->tab_helper->WasFrameBlocked(frame->GetFrameTreeNodeId());
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

bool StateAndLogsImpl::IsOriginWanted(RuleGroup group, url::Origin origin) {
  return !rules_service_->GetRuleManager()->IsExemptOfFiltering(group, origin);
}

void StateAndLogsImpl::ComputeMissingActivationsForNavigation(
    RuleGroup group,
    content::RenderFrameHost* frame) {
  RulesIndex* index = rules_service_->GetRuleIndex(group);

  if (!index) {
    return;
  }

  ReportTabActivations(
      group, frame,
      index->GetActivationsForFrame(
          base::BindRepeating(&StateAndLogsImpl::IsOriginWanted,
                              base::Unretained(this), group),
          frame));
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
  return TabStateAndLogsImpl::FromWebContents(contents);
}

void StateAndLogsImpl::CreateTabHelper(content::WebContents* contents) {
  TabStateAndLogsImpl::CreateForWebContents(contents,
                                            weak_factory_.GetWeakPtr());
}

void StateAndLogsImpl::SendNotifications() {
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