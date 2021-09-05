// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/adverse_adblocking/vivaldi_subresource_filter_client.h"

#include "app/vivaldi_apptools.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"
#include "components/adverse_adblocking/vivaldi_subresource_filter_throttle.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/safe_browsing/core/db/database_manager.h"
#include "components/subresource_filter/content/browser/ads_intervention_manager.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"

// static
void VivaldiSubresourceFilterClient::
    CreateThrottleManagerWithClientForWebContents(
        content::WebContents* web_contents) {
  subresource_filter::RulesetService* ruleset_service =
      g_browser_process->subresource_filter_ruleset_service();
  subresource_filter::VerifiedRulesetDealer::Handle* dealer =
      ruleset_service ? ruleset_service->GetRulesetDealer() : nullptr;

  // Chaining the existing filterclient.
  auto* throttle_manager = subresource_filter::
    ContentSubresourceFilterThrottleManager::FromWebContents(web_contents);

  //Take ownership of the previously created client, if any.
  ChromeSubresourceFilterClient* chromesfc = nullptr;
  if (throttle_manager) {
    chromesfc = static_cast<ChromeSubresourceFilterClient*>(throttle_manager->release_client());
  }

  std::unique_ptr<VivaldiSubresourceFilterClient> client =
      std::make_unique<VivaldiSubresourceFilterClient>(web_contents, chromesfc);

  // Releases the old throttle_manager.
  if (chromesfc) {
    web_contents->RemoveUserData(
        subresource_filter::ContentSubresourceFilterThrottleManager::
            kContentSubresourceFilterThrottleManagerWebContentsUserDataKey);
  }

  subresource_filter::ContentSubresourceFilterThrottleManager::
      CreateForWebContents(
          web_contents,
          std::move(client),
          dealer);
}

// static
VivaldiSubresourceFilterClient* VivaldiSubresourceFilterClient::FromWebContents(
    content::WebContents* web_contents) {
    auto* throttle_manager = subresource_filter::
      ContentSubresourceFilterThrottleManager::FromWebContents(web_contents);

  if (!throttle_manager)
    return nullptr;

  return static_cast<VivaldiSubresourceFilterClient*>(
      throttle_manager->client());
}


// NOTE: Most functions redirect to APIs in ChromeSubresourceFilterClient.
// This is because that object take precedence, and will process the results
// from the check of the list, and if necessary block a site before this object
// is being processed. The logic is also the same.
VivaldiSubresourceFilterClient::VivaldiSubresourceFilterClient(
    content::WebContents* web_contents,
    ChromeSubresourceFilterClient* chromesfc) {
  DCHECK(web_contents);
  DCHECK(chromesfc);
  chrome_subresource_filter_client_.reset(
      chromesfc);
  profile_context_ = SubresourceFilterProfileContextFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
}

VivaldiSubresourceFilterClient::~VivaldiSubresourceFilterClient() {
  profile_context_ = nullptr;
}

void VivaldiSubresourceFilterClient::MaybeAppendNavigationThrottles(
    content::NavigationHandle* navigation_handle,
    std::vector<std::unique_ptr<content::NavigationThrottle>>* throttles) {
  DCHECK(!navigation_handle->IsSameDocument());

  if (navigation_handle->IsInMainFrame()) {
    std::unique_ptr<VivaldiSubresourceFilterAdblockingThrottle> throttle(
        new VivaldiSubresourceFilterAdblockingThrottle(navigation_handle,
                                                       this));
    throttles->push_back(std::move(throttle));
  }

  auto* throttle_manager =
      subresource_filter::ContentSubresourceFilterThrottleManager::
          FromWebContents(navigation_handle->GetWebContents());

  if (throttle_manager)
    throttle_manager->MaybeAppendNavigationThrottles(navigation_handle,
                                                     throttles, true);
}

void VivaldiSubresourceFilterClient::ShowNotification() {
  chrome_subresource_filter_client_->ShowNotification();
}

void VivaldiSubresourceFilterClient::OnReloadRequested() {
  chrome_subresource_filter_client_->OnReloadRequested();
}

const scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
VivaldiSubresourceFilterClient::GetSafeBrowsingDatabaseManager() {
  safe_browsing::SafeBrowsingService* safe_browsing_service =
      g_browser_process->safe_browsing_service();
  return safe_browsing_service ? safe_browsing_service->database_manager()
                               : nullptr;
}

subresource_filter::ProfileInteractionManager*
VivaldiSubresourceFilterClient::GetProfileInteractionManager() {
  return nullptr;
}

void VivaldiSubresourceFilterClient::OnAdsViolationTriggered(
    content::RenderFrameHost* rfh,
    subresource_filter::mojom::AdsViolation triggered_violation) {
  chrome_subresource_filter_client_->OnAdsViolationTriggered(
      rfh, triggered_violation);
}
