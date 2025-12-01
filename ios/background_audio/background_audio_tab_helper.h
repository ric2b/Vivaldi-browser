// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_BACKGROUND_AUDIO_BACKGROUND_AUDIO_TAB_HELPER_H_
#define IOS_BACKGROUND_AUDIO_BACKGROUND_AUDIO_TAB_HELPER_H_

#import "base/memory/raw_ptr.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@class WebsiteDarkModeAgent;
class PrefService;
class BackgroundAudioTabHelper;

@interface BackgroundAudioPrefObserver : NSObject
- (instancetype)initWithOwner:(BackgroundAudioTabHelper*)owner
                  prefService:(PrefService*)prefs;
- (void)stopObserving;
@end

class BackgroundAudioTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<BackgroundAudioTabHelper> {
 public:
  BackgroundAudioTabHelper(const BackgroundAudioTabHelper&) = delete;
  BackgroundAudioTabHelper& operator=(const BackgroundAudioTabHelper&) = delete;

  ~BackgroundAudioTabHelper() override;

  void BackgroundAudioPrefChanged();

 private:
  friend class web::WebStateUserData<BackgroundAudioTabHelper>;

  BackgroundAudioTabHelper(web::WebState* web_state);

  // WebStateObserver methods:
  void WebStateDestroyed(web::WebState* web_state) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;

  BackgroundAudioPrefObserver* allowBackgroundAudioObserver_;
  raw_ptr<PrefService> prefs_;
  raw_ptr<web::WebState> web_state_;

  bool hasInjectedCode = false;
  bool isYoutube = false;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_BACKGROUND_AUDIO_BACKGROUND_AUDIO_TAB_HELPER_H_
