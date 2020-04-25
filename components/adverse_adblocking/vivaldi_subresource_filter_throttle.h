// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ADVERSE_ADBLOCKING_VIVALDI_SUBRESOURCE_FILTER_THROTTLE_H_
#define COMPONENTS_ADVERSE_ADBLOCKING_VIVALDI_SUBRESOURCE_FILTER_THROTTLE_H_

#include "base/memory/weak_ptr.h"

#include "components/subresource_filter/content/browser/subresource_filter_client.h"
#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_client.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/common/activation_list.h"
#include "content/public/browser/navigation_throttle.h"

using subresource_filter::SubresourceFilterClient;
using subresource_filter::SubresourceFilterSafeBrowsingClient;
using subresource_filter::Configuration;
using subresource_filter::ActivationList;
using subresource_filter::ActivationDecision;

class VivaldiSubresourceFilterClient;

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
    : public content::NavigationThrottle,
      public base::SupportsWeakPtr<VivaldiSubresourceFilterAdblockingThrottle> {
 public:
  VivaldiSubresourceFilterAdblockingThrottle(
      content::NavigationHandle* handle,
      base::WeakPtr<VivaldiSubresourceFilterClient> client);
  ~VivaldiSubresourceFilterAdblockingThrottle() override;

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

  bool HasFinishedAllSafeBrowsingChecks() const;
  // Gets the configuration with the highest priority among those activated.
  // Returns it, or none if no valid activated configurations.
  ConfigResult GetHighestPriorityConfiguration(
      const SubresourceFilterSafeBrowsingClient::CheckResult& result);
  // Gets the ActivationDecision for the given Configuration.
  // Returns it, or ACTIVATION_CONDITIONS_NOT_MET if no Configuration.
  ActivationDecision GetActivationDecision(
      const std::vector<ConfigResult>& configs,
      ConfigResult* selected_config);
  bool DoesMainFrameURLSatisfyActivationConditions(
      const Configuration::ActivationConditions& conditions,
      ActivationList matched_list) const;

  // NOTE(andre@vivaldi.com) : keep this as is since it is the form used
  // throughout the subresource_filter-component.
  std::vector<SubresourceFilterSafeBrowsingClient::CheckResult> check_results_;

  // Whether this throttle is deferring the navigation. Only set to true in
  // WillProcessResponse if there are ongoing safe browsing checks.
  bool deferring_ = false;

  // This must outlive this class and is passed as a WeakPtr so all running
  // tasks here will be cancelled when it goes away.
  base::WeakPtr<VivaldiSubresourceFilterClient> filter_client_;

  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiSubresourceFilterAdblockingThrottle);
};
#endif  // COMPONENTS_ADVERSE_ADBLOCKING_VIVALDI_SUBRESOURCE_FILTER_THROTTLE_H_
