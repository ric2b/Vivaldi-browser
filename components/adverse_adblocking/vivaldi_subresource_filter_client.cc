// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/adverse_adblocking/vivaldi_subresource_filter_client.h"

#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"
#include "chrome/browser/subresource_filter/subresource_filter_content_settings_manager.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"
#include "components/adverse_adblocking/vivaldi_subresource_filter_throttle.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/android/content_settings/ads_blocked_infobar_delegate.h"
#endif

// NOTE: Most functions redirect to APIs in ChromeSubresourceFilterClient.
// This is because that object take precedence, and will process the results
// from the check of the list, and if necessary block a site before this object
// is being processed. The logic is also the same.
VivaldiSubresourceFilterClient::VivaldiSubresourceFilterClient(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  DCHECK(web_contents);
  subresource_filter::RulesetService* ruleset_service =
      g_browser_process->subresource_filter_ruleset_service();
  subresource_filter::VerifiedRulesetDealer::Handle* dealer =
      ruleset_service ? ruleset_service->GetRulesetDealer() : nullptr;
  throttle_manager_ = std::make_unique<
      subresource_filter::ContentSubresourceFilterThrottleManager>(
      this, dealer, web_contents);
}

VivaldiSubresourceFilterClient::~VivaldiSubresourceFilterClient() {}

void VivaldiSubresourceFilterClient::MaybeAppendNavigationThrottles(
    content::NavigationHandle* navigation_handle,
    std::vector<std::unique_ptr<content::NavigationThrottle>>* throttles) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (navigation_handle->IsInMainFrame()) {
    throttles->push_back(
        std::make_unique<VivaldiSubresourceFilterAdblockingThrottle>(
            navigation_handle, AsWeakPtr()));
  }
  throttle_manager_->MaybeAppendNavigationThrottles(navigation_handle,
                                                    throttles);
}

void VivaldiSubresourceFilterClient::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  ChromeSubresourceFilterClient::FromWebContents(web_contents())
      ->DidStartNavigation(navigation_handle);
}

bool VivaldiSubresourceFilterClient::did_show_ui_for_navigation() const {
  return ChromeSubresourceFilterClient::FromWebContents(web_contents())
      ->did_show_ui_for_navigation();
}

void VivaldiSubresourceFilterClient::ShowNotification() {
  ChromeSubresourceFilterClient::FromWebContents(web_contents())
      ->ShowNotification();
}

subresource_filter::mojom::ActivationLevel
VivaldiSubresourceFilterClient::OnPageActivationComputed(
    content::NavigationHandle* navigation_handle,
    subresource_filter::mojom::ActivationLevel initial_activation_level,
    subresource_filter::ActivationDecision* decision) {
  return ChromeSubresourceFilterClient::FromWebContents(web_contents())
      ->OnPageActivationComputed(navigation_handle, initial_activation_level,
                                 decision);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(VivaldiSubresourceFilterClient)
