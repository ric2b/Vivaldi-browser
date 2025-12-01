// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#import "ios/background_audio/background_audio_tab_helper.h"

#import "base/apple/bundle_locations.h"
#import "base/strings/sys_string_conversions.h"
#import "components/google/core/common/google_util.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "prefs/vivaldi_pref_names.h"

@interface BackgroundAudioPrefObserver () <BooleanObserver>

// PrefBackedBoolean for "Allow Background Audio" setting state.
@property(nonatomic, strong, readonly) PrefBackedBoolean* allowBackgroundAudio;
@property(nonatomic, assign, readonly) BackgroundAudioTabHelper* owner;

@end

@implementation BackgroundAudioPrefObserver
- (instancetype)initWithOwner:(BackgroundAudioTabHelper*)owner
                  prefService:(PrefService*)prefs {
  self = [super init];
  if (self) {
    _owner = owner;

    _allowBackgroundAudio = [[PrefBackedBoolean alloc]
        initWithPrefService:prefs
                   prefName:vivaldiprefs::kVivaldiBackgroundAudioEnabled];
    [_allowBackgroundAudio setObserver:self];
  }
  return self;
}

- (void)stopObserving {
  [_allowBackgroundAudio stop];
  [_allowBackgroundAudio setObserver:nil];
  _allowBackgroundAudio = nil;
}

- (void)dealloc {
  _owner = nil;
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (!_owner) {
    return;
  }
  if (observableBoolean == _allowBackgroundAudio) {
    _owner->BackgroundAudioPrefChanged();
  }
}

@end

BackgroundAudioTabHelper::~BackgroundAudioTabHelper() = default;

BackgroundAudioTabHelper::BackgroundAudioTabHelper(web::WebState* web_state)
    : prefs_(ProfileIOS::FromBrowserState(web_state->GetBrowserState())
                 ->GetPrefs()),
      web_state_(web_state) {
  web_state->AddObserver(this);
  allowBackgroundAudioObserver_ =
      [[BackgroundAudioPrefObserver alloc] initWithOwner:this
                                             prefService:prefs_];
}

#pragma mark - WebStateObserver methods.

void BackgroundAudioTabHelper::WebStateDestroyed(web::WebState* web_state) {
  CHECK_EQ(web_state, web_state_);
  web_state->RemoveObserver(this);
  [allowBackgroundAudioObserver_ stopObserving];
}

void BackgroundAudioTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  CHECK_EQ(web_state, web_state_);

  // Check url first, so we can reload in case going from disabled->enabled
  if (google_util::IsYoutubeDomainUrl(
          navigation_context->GetUrl(), google_util::ALLOW_SUBDOMAIN,
          google_util::DISALLOW_NON_STANDARD_PORTS)) {
    isYoutube = true;
  } else {
    isYoutube = false;
  }

  if (!isYoutube) {
    // Only YouTube domains for now.
    return;
  }

  bool isEnabled =
      prefs_->GetBoolean(vivaldiprefs::kVivaldiBackgroundAudioEnabled);

  if (!isEnabled) {
    // Don't inject if the feature is disabled
    return;
  }

  web::WebFrame* web_frame =
      web_state->GetPageWorldWebFramesManager()->GetMainWebFrame();
  if (!web_frame) {
    return;
  }

  NSString* script_path =
      [base::apple::FrameworkBundle() pathForResource:@"background_audio"
                                               ofType:@"js"];
  NSError* error = nil;
  NSString* script_bundle =
      [NSString stringWithContentsOfFile:script_path
                                encoding:NSUTF8StringEncoding
                                   error:&error];
  if (error) {
    return;
  }

  std::u16string script = base::SysNSStringToUTF16(script_bundle);
  web_frame->ExecuteJavaScript(script);
  hasInjectedCode = true;
}

void BackgroundAudioTabHelper::BackgroundAudioPrefChanged() {
  if (!hasInjectedCode) {
    // No injected code
    bool isEnabled =
        prefs_->GetBoolean(vivaldiprefs::kVivaldiBackgroundAudioEnabled);
    if (!isEnabled || !isYoutube) {
      // Feature is disabled so no need to inject
      // OR not youtube so no need to inject
      return;
    }
  }

  // We get here if:
  // Feature is enabled && url is youtube but has no injected code OR
  // Feature is disabled but we have injected code
  // => we should reload the tab.
  web_state_->GetNavigationManager()->Reload(web::ReloadType::NORMAL,
                                             /*check_for_repost=*/true);
}

WEB_STATE_USER_DATA_KEY_IMPL(BackgroundAudioTabHelper)
