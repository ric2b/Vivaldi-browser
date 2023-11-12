// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/supports_user_data.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"

class AdverseAdFilterListService;

class VivaldiSubresourceFilterAdblockingThrottleManager
    : public base::SupportsUserData::Data {
 private:
  //
  raw_ptr<AdverseAdFilterListService> adblock_list_ = nullptr;  // owned by the profile.

 public:
  static const char
      kVivaldiSubresourceFilterThrottleManagerWebContentsUserDataKey[];

  VivaldiSubresourceFilterAdblockingThrottleManager();
  ~VivaldiSubresourceFilterAdblockingThrottleManager() override;
  VivaldiSubresourceFilterAdblockingThrottleManager(
      const VivaldiSubresourceFilterAdblockingThrottleManager&) = delete;
  VivaldiSubresourceFilterAdblockingThrottleManager& operator=(
      const VivaldiSubresourceFilterAdblockingThrottleManager&) = delete;

  static void CreateForWebContents(content::WebContents* web_contents);

  static VivaldiSubresourceFilterAdblockingThrottleManager* FromWebContents(
      content::WebContents* web_contents);

  // Creates a VivaldiSubresourceFilterAdblockingThrottleManager and attaches it
  // to |web_contents|.
  static void CreateSubresourceFilterWebContentsHelper(
      content::WebContents* web_contents);

  // Will just chain VivaldiSubresourceFilterAdblockingThrottle to existing
  // throttles.
  void MaybeAppendNavigationThrottles(
      content::NavigationHandle* navigation_handle,
      std::vector<std::unique_ptr<content::NavigationThrottle>>* throttles,
      bool done_mainframe = false);

  void set_adblock_list(AdverseAdFilterListService* list) {
    adblock_list_ = list;
  }

  AdverseAdFilterListService* adblock_list() { return adblock_list_; }
};
