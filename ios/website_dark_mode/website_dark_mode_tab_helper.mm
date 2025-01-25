// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#import "ios/website_dark_mode/website_dark_mode_tab_helper.h"

#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/website_dark_mode/website_dark_mode_agent.h"

WebsiteDarkModeTabHelper::~WebsiteDarkModeTabHelper() = default;

WebsiteDarkModeTabHelper::WebsiteDarkModeTabHelper(web::WebState* web_state)
    : profile_(
          ProfileIOS::FromBrowserState(web_state->GetBrowserState())),
website_dark_mode_agent_([[WebsiteDarkModeAgent alloc]
          initWithPrefService:profile_->GetPrefs()
                     webState:web_state]) {
  web_state->AddObserver(this);
}

void WebsiteDarkModeTabHelper::WebStateDestroyed(web::WebState* web_state) {
  website_dark_mode_agent_ = nil;
  web_state->RemoveObserver(this);
}

WEB_STATE_USER_DATA_KEY_IMPL(WebsiteDarkModeTabHelper)
