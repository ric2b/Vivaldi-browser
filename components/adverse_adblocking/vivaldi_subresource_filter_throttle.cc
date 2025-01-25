// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/adverse_adblocking/vivaldi_subresource_filter_throttle.h"

#include "app/vivaldi_apptools.h"
#include "base/metrics/histogram_macros.h"
#include "components/adverse_adblocking/adverse_ad_filter_list.h"
#include "components/adverse_adblocking/adverse_ad_filter_list_factory.h"
#include "components/adverse_adblocking/vivaldi_subresource_filter_throttle_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/subresource_filter/content/browser/content_activation_list_utils.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/navigation_console_logger.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "components/subresource_filter/content/shared/browser/ruleset_service.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"

using subresource_filter::ActivationScope;
using subresource_filter::kActivationWarningConsoleMessage;
using subresource_filter::kFilterAdsOnAbusiveSites;
using subresource_filter::NavigationConsoleLogger;
using subresource_filter::SubresourceFilterObserverManager;

VivaldiSubresourceFilterAdblockingThrottle::
    VivaldiSubresourceFilterAdblockingThrottle(
        content::NavigationHandle* handle)
    : NavigationThrottle(handle),
      browser_context_(handle->GetStartingSiteInstance()->GetBrowserContext()) {
  DCHECK(handle->IsInMainFrame());
  CheckCurrentUrl();
}

VivaldiSubresourceFilterAdblockingThrottle::
    ~VivaldiSubresourceFilterAdblockingThrottle() {
  check_results_.clear();
}

content::NavigationThrottle::ThrottleCheckResult
VivaldiSubresourceFilterAdblockingThrottle::WillStartRequest() {
  CheckCurrentUrl();
  return PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
VivaldiSubresourceFilterAdblockingThrottle::WillRedirectRequest() {
  CheckCurrentUrl();
  return PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
VivaldiSubresourceFilterAdblockingThrottle::WillProcessResponse() {
  // No need to defer the navigation if the check already happened.
  if (HasFinishedAllSafeBrowsingChecks()) {
    NotifyResult();
    return PROCEED;
  }
  deferring_ = true;
  defer_time_ = base::TimeTicks::Now();
  return DEFER;
}

const char* VivaldiSubresourceFilterAdblockingThrottle::GetNameForLogging() {
  return "VivaldiSubresourceFilterAdblockingThrottle";
}

void VivaldiSubresourceFilterAdblockingThrottle::CheckCurrentUrl() {
  VivaldiSubresourceFilterAdblockingThrottleManager* manager =
      VivaldiSubresourceFilterAdblockingThrottleManager::FromWebContents(
          navigation_handle()->GetWebContents());

  DCHECK(manager);

  AdverseAdFilterListService* adblock_list = manager->adblock_list();

  DCHECK(adblock_list);

  bool matched = adblock_list->IsSiteInList(navigation_handle()->GetURL());

  check_results_.emplace_back();
  size_t request_id = check_results_.size() - 1;

  std::unique_ptr<SubresourceFilterSafeBrowsingClient::CheckResult> result(
      new SubresourceFilterSafeBrowsingClient::CheckResult);
  result->request_id = request_id;
  if (matched) {
    result->threat_type =
        safe_browsing::SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER;
    result->threat_metadata.subresource_filter_match =
        safe_browsing::SubresourceFilterMatch(
            {{safe_browsing::SubresourceFilterType::ABUSIVE,
              safe_browsing::SubresourceFilterLevel::ENFORCE}});
  } else {
    result->threat_type = safe_browsing::SBThreatType::SB_THREAT_TYPE_SAFE;
  }
  result->finished = true;

  // Copy the result.
  auto& stored_result = check_results_.at(request_id);
  CHECK(!stored_result.finished);
  stored_result = *(result.release());
  if (deferring_ && HasFinishedAllSafeBrowsingChecks()) {
    NotifyResult();
    deferring_ = false;
    Resume();
  }
}

bool VivaldiSubresourceFilterAdblockingThrottle::
    HasFinishedAllSafeBrowsingChecks() const {
  for (const auto& check_result : check_results_) {
    if (!check_result.finished) {
      return false;
    }
  }
  return true;
}

VivaldiSubresourceFilterAdblockingThrottle::ConfigResult::ConfigResult(
    Configuration config,
    bool warning,
    bool matched_valid_configuration,
    ActivationList matched_list)
    : config(config),
      warning(warning),
      matched_valid_configuration(matched_valid_configuration),
      matched_list(matched_list) {}

VivaldiSubresourceFilterAdblockingThrottle::ConfigResult::ConfigResult() =
    default;

VivaldiSubresourceFilterAdblockingThrottle::ConfigResult::ConfigResult(
    const ConfigResult&) = default;

VivaldiSubresourceFilterAdblockingThrottle::ConfigResult::~ConfigResult() =
    default;

void VivaldiSubresourceFilterAdblockingThrottle::NotifyResult() {
  DCHECK(!check_results_.empty());

  // Determine which results to consider for safebrowsing/abusive.
  SubresourceFilterSafeBrowsingClient::CheckResult check_result =
      check_results_.back();

  // Find the ConfigResult for each safe browsing check.
  ConfigResult selection = GetHighestPriorityConfiguration(check_result);

  // Get the activation decision with the associated ConfigResult.
  ActivationDecision activation_decision = GetActivationDecision(selection);
  DCHECK_NE(activation_decision, ActivationDecision::UNKNOWN);

  // Notify the observers of the check results.
  SubresourceFilterObserverManager::FromWebContents(
      navigation_handle()->GetWebContents())
      ->NotifySafeBrowsingChecksComplete(navigation_handle(), check_result);

  // Compute the activation level.
  subresource_filter::mojom::ActivationLevel activation_level =
      selection.config.activation_options.activation_level;

  if (selection.warning &&
      activation_level ==
          subresource_filter::mojom::ActivationLevel::kEnabled) {
    NavigationConsoleLogger::LogMessageOnCommit(
        navigation_handle(), blink::mojom::ConsoleMessageLevel::kWarning,
        kActivationWarningConsoleMessage);
    activation_level = subresource_filter::mojom::ActivationLevel::kDisabled;
  }

  LogMetricsOnChecksComplete(selection.matched_list, activation_decision,
                             activation_level);

  SubresourceFilterObserverManager::FromWebContents(
      navigation_handle()->GetWebContents())
      ->NotifyPageActivationComputed(
          navigation_handle(),
          selection.config.GetActivationState(activation_level));
}

void VivaldiSubresourceFilterAdblockingThrottle::LogMetricsOnChecksComplete(
    ActivationList matched_list,
    ActivationDecision decision,
    subresource_filter::mojom::ActivationLevel level) const {
  DCHECK(HasFinishedAllSafeBrowsingChecks());

  base::TimeDelta delay = defer_time_.is_null()
                              ? base::Milliseconds(0)
                              : base::TimeTicks::Now() - defer_time_;
  UMA_HISTOGRAM_TIMES("SubresourceFilter.PageLoad.SafeBrowsingDelay", delay);

  ukm::SourceId source_id = ukm::ConvertToSourceId(
      navigation_handle()->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);
  ukm::builders::SubresourceFilter builder(source_id);
  builder.SetActivationDecision(static_cast<int64_t>(decision));
  if (level == subresource_filter::mojom::ActivationLevel::kDryRun) {
    DCHECK_EQ(ActivationDecision::ACTIVATED, decision);
    builder.SetDryRun(true);
  }

  UMA_HISTOGRAM_ENUMERATION("SubresourceFilter.PageLoad.ActivationDecision",
                            decision,
                            ActivationDecision::ACTIVATION_DECISION_MAX);
  UMA_HISTOGRAM_ENUMERATION("SubresourceFilter.PageLoad.ActivationList",
                            matched_list,
                            static_cast<int>(ActivationList::LAST) + 1);
}

VivaldiSubresourceFilterAdblockingThrottle::ConfigResult
VivaldiSubresourceFilterAdblockingThrottle::GetHighestPriorityConfiguration(
    const SubresourceFilterSafeBrowsingClient::CheckResult& result) {
  DCHECK(result.finished);
  Configuration selected_config;
  bool warning = false;
  bool matched = false;
  ActivationList matched_list =
      subresource_filter::GetListForThreatTypeAndMetadata(
          result.threat_type, result.threat_metadata, &warning);
  // If it's http or https, find the best config.
  if (navigation_handle()->GetURL().SchemeIsHTTPOrHTTPS()) {
    std::vector<Configuration> decreasing_configs;
    decreasing_configs.emplace_back(
        subresource_filter::mojom::ActivationLevel::kEnabled,
        subresource_filter::ActivationScope::ACTIVATION_LIST,
        subresource_filter::ActivationList::ABUSIVE);
    decreasing_configs.emplace_back(
        subresource_filter::mojom::ActivationLevel::kEnabled,
        subresource_filter::ActivationScope::ACTIVATION_LIST,
        subresource_filter::ActivationList::SUBRESOURCE_FILTER);

    const auto selected_config_itr =
        std::find_if(decreasing_configs.begin(), decreasing_configs.end(),
                     [matched_list, this](const Configuration& config) {
                       return DoesMainFrameURLSatisfyActivationConditions(
                           config.activation_conditions, matched_list);
                     });
    if (selected_config_itr != decreasing_configs.end()) {
      selected_config = *selected_config_itr;
      matched = true;
    }
  }
  return ConfigResult(selected_config, warning, matched, matched_list);
}

ActivationDecision
VivaldiSubresourceFilterAdblockingThrottle::GetActivationDecision(
    const ConfigResult& selected_config) {
  if (!selected_config.matched_valid_configuration) {
    return ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET;
  }

  // Get the activation level for the matching configuration.
  auto activation_level =
      selected_config.config.activation_options.activation_level;

  // If there is an activation triggered by the activation list (not a dry run),
  // report where in the redirect chain it was triggered.
  if (selected_config.config.activation_conditions.activation_scope ==
          ActivationScope::ACTIVATION_LIST &&
      activation_level ==
          subresource_filter::mojom::ActivationLevel::kEnabled) {
  }

  // Compute and return the activation decision.
  return activation_level ==
                 subresource_filter::mojom::ActivationLevel::kDisabled
             ? ActivationDecision::ACTIVATION_DISABLED
             : ActivationDecision::ACTIVATED;
}

bool VivaldiSubresourceFilterAdblockingThrottle::
    DoesMainFrameURLSatisfyActivationConditions(
        const Configuration::ActivationConditions& conditions,
        ActivationList matched_list) const {
  switch (conditions.activation_scope) {
    case ActivationScope::ALL_SITES:
      return true;
    case ActivationScope::ACTIVATION_LIST:
      if (matched_list == ActivationList::NONE)
        return false;
      if (conditions.activation_list == matched_list)
        return true;

      if (conditions.activation_list == ActivationList::PHISHING_INTERSTITIAL &&
          matched_list == ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL) {
        // Handling special case, where activation on the phishing sites also
        // mean the activation on the sites with social engineering metadata.
        return true;
      }
      if (conditions.activation_list == ActivationList::BETTER_ADS &&
          matched_list == ActivationList::ABUSIVE &&
          base::FeatureList::IsEnabled(kFilterAdsOnAbusiveSites)) {
        // Trigger activation on abusive sites if the condition says to trigger
        // on Better Ads sites. This removes the need for adding a separate
        // Configuration for Abusive enforcement.
        return true;
      }
      return false;
    case ActivationScope::NO_SITES:
      return false;
  }
  NOTREACHED();
  //return false;
}
