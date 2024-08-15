// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/adverse_adblocking/vivaldi_subresource_filter_throttle_manager.h"

#include "components/adverse_adblocking/vivaldi_subresource_filter_throttle.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"
#include "content/public/browser/web_contents.h"

const char VivaldiSubresourceFilterAdblockingThrottleManager::
    kVivaldiSubresourceFilterThrottleManagerWebContentsUserDataKey[] =
        "vivaldi_subresource_filter_throttle_manager";

// static
void VivaldiSubresourceFilterAdblockingThrottleManager::CreateForWebContents(
    content::WebContents* web_contents) {
  if (FromWebContents(web_contents))
    return;

  web_contents->SetUserData(
      kVivaldiSubresourceFilterThrottleManagerWebContentsUserDataKey,
      std::make_unique<VivaldiSubresourceFilterAdblockingThrottleManager>());
}

// static
VivaldiSubresourceFilterAdblockingThrottleManager*
VivaldiSubresourceFilterAdblockingThrottleManager::FromWebContents(
    content::WebContents* web_contents) {
  return static_cast<VivaldiSubresourceFilterAdblockingThrottleManager*>(
      web_contents->GetUserData(
          kVivaldiSubresourceFilterThrottleManagerWebContentsUserDataKey));
}

// static
void VivaldiSubresourceFilterAdblockingThrottleManager::
    CreateSubresourceFilterWebContentsHelper(
        content::WebContents* web_contents) {
  VivaldiSubresourceFilterAdblockingThrottleManager::CreateForWebContents(
      web_contents);
}

VivaldiSubresourceFilterAdblockingThrottleManager::
    VivaldiSubresourceFilterAdblockingThrottleManager() {}

VivaldiSubresourceFilterAdblockingThrottleManager::
    ~VivaldiSubresourceFilterAdblockingThrottleManager() {}

void VivaldiSubresourceFilterAdblockingThrottleManager::
    MaybeAppendNavigationThrottles(
        content::NavigationHandle* navigation_handle,
        std::vector<std::unique_ptr<content::NavigationThrottle>>* throttles,
        bool done_mainframe) {
  if (navigation_handle->IsInMainFrame()) {
    std::unique_ptr<VivaldiSubresourceFilterAdblockingThrottle> throttle(
        new VivaldiSubresourceFilterAdblockingThrottle(navigation_handle));
    throttles->push_back(std::move(throttle));
  }

  auto* throttle_manager =
      subresource_filter::ContentSubresourceFilterThrottleManager::
          FromNavigationHandle(*navigation_handle);

  if (throttle_manager)
    throttle_manager->MaybeAppendNavigationThrottles(navigation_handle,
                                                     throttles, true);
}
