// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ADVERSE_ADBLOCKING_VIVALDI_SUBRESOURCE_FILTER_THROTTLE_H_
#define COMPONENTS_ADVERSE_ADBLOCKING_VIVALDI_SUBRESOURCE_FILTER_THROTTLE_H_

#include "base/memory/weak_ptr.h"

#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_client.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/common/activation_list.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

using subresource_filter::ActivationDecision;
using subresource_filter::ActivationList;
using subresource_filter::Configuration;
using subresource_filter::SubresourceFilterSafeBrowsingClient;

enum class ActivationPosition {
  kOnly = 0,
  kFirst = 1,
  kMiddle = 2,
  kLast = 3,
  kMaxValue = kLast,
};

// Navigation throttle responsible for activating subresource filtering on page
// loads that match the downloaded Adverse Ad blocking list
class VivaldiSubresourceFilterAdblockingThrottle
    : public content::NavigationThrottle {
 public:
  VivaldiSubresourceFilterAdblockingThrottle(content::NavigationHandle* handle);
  ~VivaldiSubresourceFilterAdblockingThrottle() override;
  VivaldiSubresourceFilterAdblockingThrottle(
      const VivaldiSubresourceFilterAdblockingThrottle&) = delete;
  VivaldiSubresourceFilterAdblockingThrottle& operator=(
      const VivaldiSubresourceFilterAdblockingThrottle&) = delete;

  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest()
      override;
  content::NavigationThrottle::ThrottleCheckResult WillProcessResponse()
      override;
  const char* GetNameForLogging() override;

  content::BrowserContext* GetBrowserContext() { return browser_context_; }

 private:
  // Highest priority config for a check result.
  struct ConfigResult {
    Configuration config;
    bool warning;
    bool matched_valid_configuration;
    ActivationList matched_list;

    ConfigResult(Configuration config,
                 bool warning,
                 bool matched_valid_configuration,
                 ActivationList matched_list);
    ~ConfigResult();
    ConfigResult();
    ConfigResult(const ConfigResult& result);
  };

  void CheckCurrentUrl();
  void NotifyResult();

  void LogMetricsOnChecksComplete(
      ActivationList matched_list,
      ActivationDecision decision,
      subresource_filter::mojom::ActivationLevel level) const;

  bool HasFinishedAllSafeBrowsingChecks() const;
  // Gets the configuration with the highest priority among those activated.
  // Returns it, or none if no valid activated configurations.
  ConfigResult GetHighestPriorityConfiguration(
      const SubresourceFilterSafeBrowsingClient::CheckResult& result);
  // Gets the ActivationDecision for the given Configuration.
  // Returns it, or ACTIVATION_CONDITIONS_NOT_MET if no Configuration.
  ActivationDecision GetActivationDecision(const ConfigResult& selected_config);
  bool DoesMainFrameURLSatisfyActivationConditions(
      const Configuration::ActivationConditions& conditions,
      ActivationList matched_list) const;

  // NOTE(andre@vivaldi.com) : keep this as is since it is the form used
  // throughout the subresource_filter-component.
  std::vector<SubresourceFilterSafeBrowsingClient::CheckResult> check_results_;

  // Set to TimeTicks::Now() when the navigation is deferred in
  // WillProcessResponse. If deferral was not necessary, will remain null.
  base::TimeTicks defer_time_;

  // Whether this throttle is deferring the navigation. Only set to true in
  // WillProcessResponse if there are ongoing safe browsing checks.
  bool deferring_ = false;

  const raw_ptr<content::BrowserContext> browser_context_;
};

#endif  // COMPONENTS_ADVERSE_ADBLOCKING_VIVALDI_SUBRESOURCE_FILTER_THROTTLE_H_
