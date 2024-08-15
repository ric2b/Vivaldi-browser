// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#import "ios/website_dark_mode/website_dark_mode_agent.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/memory/weak_ptr.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/ui/settings/appearance/vivaldi_appearance_settings_prefs_helper.h"
#import "ios/ui/settings/appearance/vivaldi_appearance_settings_prefs.h"
#import "ios/ui/settings/appearance/vivaldi_appearance_settings_swift.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager_observer_bridge.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ios/web/public/web_state.h"
#import "ios/website_dark_mode/website_dark_mode_java_script_feature.h"
#import "prefs/vivaldi_pref_names.h"

@interface WebsiteDarkModeAgent () <CRWWebStateObserver,
                             CRWWebFramesManagerObserver> {
  // The WebState this instance is observing. Will be null after
  // -webStateDestroyed: has been called.
  web::WebState* _webState;

  // Bridge to observe the web state from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Bridge to observe the web frames manager from Objective-C.
  std::unique_ptr<web::WebFramesManagerObserverBridge>
      _webFramesManagerObserverBridge;

  // The pref service for which this agent was created.
  PrefService* _prefService;

  // Pref tracking if force dark pages is enabled.
  PrefBackedBoolean* _forceDarkWebPagesEnabled;
}

@end

@implementation WebsiteDarkModeAgent

- (instancetype)initWithPrefService:(PrefService*)prefService
                           webState:(web::WebState*)webState {
  DCHECK(prefService);
  DCHECK(webState);
  self = [super init];
  if (self) {
    _prefService = prefService;
    _webState = webState;

    [VivaldiAppearanceSettingPrefs setPrefService:prefService];

    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserverBridge.get());
    _webFramesManagerObserverBridge =
        std::make_unique<web::WebFramesManagerObserverBridge>(self);
    web::WebFramesManager* framesManager =
        WebsiteDarkModeJavaScriptFeature::GetInstance()->GetWebFramesManager(
            _webState);
    framesManager->AddObserver(_webFramesManagerObserverBridge.get());

    _forceDarkWebPagesEnabled =
        [[PrefBackedBoolean alloc]
            initWithPrefService:prefService
                prefName:vivaldiprefs::kVivaldiWebsiteAppearanceForceDarkTheme];
    [self forceReloadWebpageIfNeeded];

  }
  return self;
}

- (void)dealloc {
  if (_webState) {
    [self webStateDestroyed:_webState];
  }
}

#pragma mark - Private methods

- (BOOL)forceDarkWebPages {
  if (!_forceDarkWebPagesEnabled)
    return NO;

  BOOL enabledToggle = [_forceDarkWebPagesEnabled value];
  if (enabledToggle &&
      [self websiteAppearanceStyle] == VivaldiWebsiteAppearanceStyleDark) {
    return YES;
  }

  if (enabledToggle &&
      [self websiteAppearanceStyle] == VivaldiWebsiteAppearanceStyleAuto &&
      self.isBrowserOrSystemThemeDark) {
    return YES;
  }

  return NO;
}

- (VivaldiWebsiteAppearanceStyle)websiteAppearanceStyle {
  int style = [VivaldiAppearanceSettingsPrefsHelper getWebsiteAppearanceStyle];
  switch (style) {
    case 0:
      return VivaldiWebsiteAppearanceStyleLight;
    case 1:
      return VivaldiWebsiteAppearanceStyleDark;
    case 2:
      return VivaldiWebsiteAppearanceStyleAuto;
    default:
      return VivaldiWebsiteAppearanceStyleAuto;
  }
}

- (BOOL)isBrowserOrSystemThemeDark {
  return [VivaldiAppearanceSettingsPrefsHelper isBrowserThemeDark] ||
      UITraitCollection.currentTraitCollection.userInterfaceStyle ==
          UIUserInterfaceStyleDark;
}

- (void)forceReloadWebpageIfNeeded {
  if (!_webState)
    return;
  WebsiteDarkModeJavaScriptFeature::GetInstance()->ToggleDarkMode(
      _webState, [self forceDarkWebPages]);
}

#pragma mark - CRWWebStateObserver
- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webStateObserverBridge.reset();
    web::WebFramesManager* framesManager =
        WebsiteDarkModeJavaScriptFeature::GetInstance()->GetWebFramesManager(
            _webState);
    framesManager->RemoveObserver(_webFramesManagerObserverBridge.get());
    _webFramesManagerObserverBridge.reset();
    _webState = nullptr;

    if (_forceDarkWebPagesEnabled) {
      [_forceDarkWebPagesEnabled stop];
      _forceDarkWebPagesEnabled = nil;
    }
  }
}

#pragma mark - CRWWebFramesManagerObserver
- (void)webFramesManager:(web::WebFramesManager*)webFramesManager
    frameBecameAvailable:(web::WebFrame*)webFrame {
  DCHECK(_webState);
  DCHECK(webFrame);
  [self forceReloadWebpageIfNeeded];
}
@end
