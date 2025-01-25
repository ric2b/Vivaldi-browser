// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/model/price_notifications/price_notifications_tab_helper.h"

#import "components/commerce/core/shopping_service.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/commerce/model/push_notification/push_notification_feature.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"

namespace {

void OnProductInfoUrl(
    id<HelpCommands> help_handler,
    const GURL& product_url,
    const std::optional<const commerce::ProductInfo>& product_info) {
  if (!product_info) {
    return;
  }
  [help_handler presentInProductHelpWithType:
                    InProductHelpType::kPriceNotificationsWhileBrowsing];
}

// Returns whether the price notification should be presented
// for `web_state`.
bool ShouldPresentPriceNotifications(web::WebState* web_state) {
  ChromeBrowserState* const browser_state =
      ChromeBrowserState::FromBrowserState(web_state->GetBrowserState());

  if (!IsPriceTrackingEnabled(browser_state)) {
    return false;
  }

  feature_engagement::Tracker* const tracker =
      feature_engagement::TrackerFactory::GetForBrowserState(browser_state);
  if (!tracker->WouldTriggerHelpUI(
          feature_engagement ::kIPHPriceNotificationsWhileBrowsingFeature)) {
    return false;
  }

  return true;
}

}  // namespace

PriceNotificationsTabHelper::PriceNotificationsTabHelper(
    web::WebState* web_state) {
  web_state_observation_.Observe(web_state);
  shopping_service_ = commerce::ShoppingServiceFactory::GetForBrowserState(
      web_state->GetBrowserState());
}

PriceNotificationsTabHelper::~PriceNotificationsTabHelper() = default;

void PriceNotificationsTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // Do not show price notifications IPH if the feature engagement
  // conditions are not fulfilled.
  if (!ShouldPresentPriceNotifications(web_state)) {
    return;
  }
  // Local strong reference for binding to the callback below.
  id<HelpCommands> help_handler = help_handler_;
  shopping_service_->GetProductInfoForUrl(
      web_state->GetVisibleURL(),
      base::BindOnce(&OnProductInfoUrl, help_handler));
}

void PriceNotificationsTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state_observation_.Reset();
}

WEB_STATE_USER_DATA_KEY_IMPL(PriceNotificationsTabHelper)
