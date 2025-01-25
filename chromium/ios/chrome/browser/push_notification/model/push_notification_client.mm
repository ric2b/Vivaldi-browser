// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/push_notification_client.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_sender.h"

PushNotificationClient::PushNotificationClient(
    PushNotificationClientId client_id)
    : client_id_(client_id) {}

PushNotificationClient::~PushNotificationClient() = default;

PushNotificationClientId PushNotificationClient::GetClientId() {
  return client_id_;
}

void PushNotificationClient::OnSceneActiveForegroundBrowserReady() {
  if (!urls_delayed_for_loading_.size() && !feedback_presentation_delayed_) {
    return;
  }
  CHECK(!urls_delayed_for_loading_.size() || !feedback_presentation_delayed_);
  Browser* browser = GetSceneLevelForegroundActiveBrowser();
  CHECK(browser);
  if (feedback_presentation_delayed_) {
    id<ApplicationCommands> handler =
        static_cast<id<ApplicationCommands>>(browser->GetCommandDispatcher());
    switch (feedback_presentation_delayed_client_) {
      case PushNotificationClientId::kContent:
      case PushNotificationClientId::kSports:
        [handler
            showReportAnIssueFromViewController:browser->GetSceneState()
                                                    .window.rootViewController
                                         sender:UserFeedbackSender::
                                                    ContentNotification
                            specificProductData:feedback_data_];
        feedback_presentation_delayed_ = false;
        break;
      case PushNotificationClientId::kTips:
      case PushNotificationClientId::kCommerce:
        // Features do not support feedback.
        NOTREACHED_IN_MIGRATION();
        break;
      default:
        break;
    }
  }
  if (urls_delayed_for_loading_.size()) {
    for (const GURL& url : urls_delayed_for_loading_) {
      loadUrlInNewTab(url, browser);
    }
    urls_delayed_for_loading_.clear();
  }
}

// TODO(crbug.com/41497027): Make functionality that relies on this
// multi-profile and multi-window safe. That might mean removing this method and
// finding a different way to determine which window should be used to present
// UI.
Browser* PushNotificationClient::GetSceneLevelForegroundActiveBrowser() {
  BrowserList* browser_list =
      BrowserListFactory::GetForBrowserState(GetLastUsedBrowserState());
  for (Browser* browser :
       browser_list->BrowsersOfType(BrowserList::BrowserType::kRegular)) {
    if (browser->GetSceneState().activationLevel ==
        SceneActivationLevelForegroundActive) {
      return browser;
    }
  }
  return nullptr;
}

void PushNotificationClient::loadUrlInNewTab(const GURL& url) {
  Browser* browser = GetSceneLevelForegroundActiveBrowser();
  if (!browser) {
    urls_delayed_for_loading_.push_back(url);
    return;
  }

  loadUrlInNewTab(url, browser);
}

void PushNotificationClient::loadUrlInNewTab(const GURL& url,
                                             Browser* browser) {
  UrlLoadParams params = UrlLoadParams::InNewTab(url);
  UrlLoadingBrowserAgent::FromBrowser(browser)->Load(params);
}

void PushNotificationClient::loadFeedbackWithPayloadAndClientId(
    NSDictionary<NSString*, NSString*>* data,
    PushNotificationClientId client) {
  Browser* browser = GetSceneLevelForegroundActiveBrowser();
  if (!browser && data) {
    feedback_presentation_delayed_client_ = client;
    feedback_presentation_delayed_ = true;
    feedback_data_ = data;
    return;
  }
}

ChromeBrowserState* PushNotificationClient::GetLastUsedBrowserState() {
  if (last_used_browser_state_for_testing_) {
    return last_used_browser_state_for_testing_;
  }
  return GetApplicationContext()
      ->GetChromeBrowserStateManager()
      ->GetLastUsedBrowserStateDeprecatedDoNotUse();
}
