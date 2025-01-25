// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_WEBSITE_DARK_MODE_WEBSITE_DARK_MODE_TAB_HELPER_H_
#define IOS_WEBSITE_DARK_MODE_WEBSITE_DARK_MODE_TAB_HELPER_H_

#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@class WebsiteDarkModeAgent;
class ProfileIOS;

// Class binding an instance of WebsiteDarkModeAgent to a WebState.
class WebsiteDarkModeTabHelper: public web::WebStateObserver,
                      public web::WebStateUserData<WebsiteDarkModeTabHelper> {
 public:
  WebsiteDarkModeTabHelper(const WebsiteDarkModeTabHelper&) = delete;
  WebsiteDarkModeTabHelper& operator=(const WebsiteDarkModeTabHelper&) = delete;

  ~WebsiteDarkModeTabHelper() override;

 private:
  friend class web::WebStateUserData<WebsiteDarkModeTabHelper>;

  WebsiteDarkModeTabHelper(web::WebState* web_state);

  // web::WebStateObserver implementation.
  void WebStateDestroyed(web::WebState* web_state) override;

  // The profile associated with this WebState.
  ProfileIOS* profile_;

  // The Objective-C AutofillAgent instance.
  __strong WebsiteDarkModeAgent* website_dark_mode_agent_;

  WEB_STATE_USER_DATA_KEY_DECL();
};


#endif  // IOS_WEBSITE_DARK_MODE_WEBSITE_DARK_MODE_TAB_HELPER_H_
