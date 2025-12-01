// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/general/vivaldi_general_settings_mediator.h"

#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/settings/general/vivaldi_general_settings_prefs.h"
#import "prefs/vivaldi_pref_names.h"

@interface VivaldiGeneralSettingsMediator () <BooleanObserver> {
  PrefBackedBoolean* _homepageEnabled;
  PrefBackedBoolean* _backgroundAudioEnabled;
}
@end

@implementation VivaldiGeneralSettingsMediator {
  // Pref Service
  PrefService* _prefService;
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
}

- (instancetype)initWithOriginalPrefService:(PrefService*)originalPrefService {
  self = [super init];
  if (self) {
    _prefService = originalPrefService;
    _homepageEnabled = [[PrefBackedBoolean alloc]
      initWithPrefService:originalPrefService
                 prefName:vivaldiprefs::kVivaldiHomepageEnabled];
    [_homepageEnabled setObserver:self];
    [self.consumer setPreferenceForHomepageUrl:self.homepageURL];

    _backgroundAudioEnabled = [[PrefBackedBoolean alloc]
      initWithPrefService:originalPrefService
                 prefName:vivaldiprefs::kVivaldiBackgroundAudioEnabled];
    [_backgroundAudioEnabled setObserver:self];
  }
  return self;
}

- (void)disconnect {
  [_homepageEnabled stop];
  [_homepageEnabled setObserver:nil];
  _homepageEnabled = nil;
  _consumer = nil;

  [_backgroundAudioEnabled stop];
  [_backgroundAudioEnabled setObserver:nil];
  _backgroundAudioEnabled = nil;
}

#pragma mark - Private Helpers
- (BOOL)isHomepageEnabled {
  if (!_homepageEnabled) {
    return NO;
  }
  return [_homepageEnabled value];
}

- (NSString*)homepageURL {
  return [VivaldiGeneralSettingPrefs
           getHomepageUrlWithPrefService: _prefService];
}

- (BOOL)isBackgroundAudioEnabled {
  if (!_backgroundAudioEnabled) {
    return NO;
  }
  return [_backgroundAudioEnabled value];
}

#pragma mark - Properties

- (void)setConsumer:(id<VivaldiGeneralSettingsConsumer>)consumer {
  _consumer = consumer;
  [self.consumer setPreferenceForHomepageSwitch:[self isHomepageEnabled]];
  [self.consumer setPreferenceForHomepageUrl:self.homepageURL];
  [self.consumer
      setPreferenceForBackgroundAudioEnabled:[self isBackgroundAudioEnabled]];
}

#pragma mark - VivaldiGeneralSettingsConsumer
- (void)setPreferenceForHomepageSwitch:(BOOL)homepageEnabled {
  if (homepageEnabled != [self isHomepageEnabled])
    [_homepageEnabled setValue:homepageEnabled];
}

- (void)setPreferenceForHomepageUrl:(NSString *)url {
  [VivaldiGeneralSettingPrefs
    setHomepageUrlWithPrefService: url inPrefServices: _prefService];
}

- (void)setPreferenceForBackgroundAudioEnabled:(BOOL)enabled {
  [VivaldiGeneralSettingPrefs setBackgroundAudioEnabled:enabled
                                         inPrefServices:_prefService];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _homepageEnabled) {
    [self.consumer setPreferenceForHomepageSwitch:[observableBoolean value]];
  } else if (observableBoolean == _backgroundAudioEnabled) {
    [self.consumer
        setPreferenceForBackgroundAudioEnabled:[observableBoolean value]];
  }
}

@end
