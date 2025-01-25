// Copyright 2024-25 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/pagezoom/vivaldi_pagezoom_settings_mediator.h"

#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/public/provider/chrome/browser/text_zoom/text_zoom_api.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/settings/pagezoom/vivaldi_pagezoom_settings_prefs.h"
#import "prefs/vivaldi_pref_names.h"

@implementation VivaldiPageZoomSettingsMediator {
  PrefService* _prefService;
}

- (instancetype)initWithOriginalPrefService:(PrefService*)originalPrefService {
  self = [super init];
  if (self) {
    _prefService = originalPrefService;
    [self.consumer setPreferenceForPageZoomLevel:self.pageZoomLevel];
  }
  return self;
}

- (void)disconnect {
  _consumer = nil;
}

#pragma mark - Private Helpers

- (double)pageZoomLevel {
  return [VivaldiPageZoomSettingPrefs
      getPageZoomLevelWithPrefService: _prefService];
}

#pragma mark - Properties

- (void)setConsumer:(id<VivaldiPageZoomSettingsConsumer>)consumer {
  _consumer = consumer;
  [self.consumer setPreferenceForPageZoomLevel:self.pageZoomLevel];
}

#pragma mark - VivaldiPageZoomSettingsConsumer
- (void)setPreferenceForPageZoomLevel:(NSInteger)level {
  [VivaldiPageZoomSettingPrefs
      setPageZoomLevelWithPrefService:level inPrefServices: _prefService];
  // Enable global page zoom on tapping of the zoom levels percentage
  [VivaldiPageZoomSettingPrefs
      setGlobalPageZoomEnabledWithPrefService:YES inPrefServices: _prefService];
  // Applying the zoom level for all web states
  if (_browser) {
    WebStateList* webStateList = _browser->GetWebStateList();
    for (int i = 0; i < webStateList->count(); i++) {
      web::WebState* webState = webStateList->GetWebStateAt(i);
      if (webState) {
        ios::provider::SetTextZoomForWebState(webState, level);
      }
    }
  }
}

- (void)setPreferenceForGlobalPageZoomEnabled:(BOOL)enabled {
  [VivaldiPageZoomSettingPrefs
      setGlobalPageZoomEnabledWithPrefService:enabled
                               inPrefServices:_prefService];
}

@end
