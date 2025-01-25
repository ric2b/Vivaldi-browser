// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/tabs/vivaldi_tab_settings_mediator.h"

#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/settings/tabs/vivaldi_ntp_type.h"
#import "ios/ui/settings/tabs/vivaldi_tab_setting_prefs.h"
#import "prefs/vivaldi_pref_names.h"

@interface VivaldiTabSettingsMediator () <BooleanObserver,
                                          PrefObserverDelegate> {
  PrefBackedBoolean* _bottomOmniboxEnabled;
  PrefBackedBoolean* _reverseSearchResultsEnabled;
  PrefBackedBoolean* _tabBarEnabled;
  PrefBackedBoolean* _showXButtonBackgroundTabsEnabled;
  PrefBackedBoolean* _openNTPOnClosingLastTabEnabled;
  PrefBackedBoolean* _focusOmniboxOnNTPEnabled;
}
@end

@implementation VivaldiTabSettingsMediator {
  // Preference service from the application context.
  raw_ptr<PrefService> _local_prefs;
  // Pref Service
  PrefService* _prefService;
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
}

- (instancetype)initWithOriginalPrefService:(PrefService*)originalPrefService
                           localPrefService:(PrefService*)localPrefService {
  self = [super init];
  if (self) {
    _local_prefs = localPrefService;
    _prefService = originalPrefService;
    _bottomOmniboxEnabled =
        [[PrefBackedBoolean alloc] initWithPrefService:localPrefService
                                              prefName:prefs::kBottomOmnibox];
    [_bottomOmniboxEnabled setObserver:self];

    _reverseSearchResultsEnabled =
        [[PrefBackedBoolean alloc]
           initWithPrefService:originalPrefService
              prefName:vivaldiprefs::kVivaldiReverseSearchResultsEnabled];
    [_reverseSearchResultsEnabled setObserver:self];

    _tabBarEnabled =
        [[PrefBackedBoolean alloc]
            initWithPrefService:originalPrefService
                       prefName:vivaldiprefs::kVivaldiDesktopTabsEnabled];
    [_tabBarEnabled setObserver:self];

    _showXButtonBackgroundTabsEnabled =
        [[PrefBackedBoolean alloc]
            initWithPrefService:originalPrefService
               prefName:vivaldiprefs::kVivaldiShowXButtonBackgroundTabsEnabled];
    [_showXButtonBackgroundTabsEnabled setObserver:self];

    _openNTPOnClosingLastTabEnabled =
        [[PrefBackedBoolean alloc]
            initWithPrefService:originalPrefService
                prefName:vivaldiprefs::kVivaldiOpenNTPOnClosingLastTabEnabled];
    [_openNTPOnClosingLastTabEnabled setObserver:self];

    _focusOmniboxOnNTPEnabled =
        [[PrefBackedBoolean alloc]
            initWithPrefService:originalPrefService
                prefName:vivaldiprefs::kVivaldiFocusOmniboxOnNTPEnabled];
    [_focusOmniboxOnNTPEnabled setObserver:self];

    [self.consumer setPreferenceForVivaldiNTPType:self.newTabSetting
                                          withURL:self.newTabURL];

    if (IsInactiveTabsAvailable()) {
      _prefChangeRegistrar.Init(_local_prefs);
      _prefObserverBridge.reset(new PrefObserverBridge(self));
      _prefObserverBridge->ObserveChangesForPreference(
          prefs::kInactiveTabsTimeThreshold, &_prefChangeRegistrar);
      [self.consumer
          setPreferenceForInactiveTabsTimeThreshold:[self defaultThreshold]];
    }

  }
  return self;
}

- (void)disconnect {
  [_bottomOmniboxEnabled stop];
  [_bottomOmniboxEnabled setObserver:nil];
  _bottomOmniboxEnabled = nil;

  [_reverseSearchResultsEnabled stop];
  [_reverseSearchResultsEnabled setObserver:nil];
  _reverseSearchResultsEnabled = nil;

  [_tabBarEnabled stop];
  [_tabBarEnabled setObserver:nil];
  _tabBarEnabled = nil;

  [_showXButtonBackgroundTabsEnabled stop];
  [_showXButtonBackgroundTabsEnabled setObserver:nil];
  _showXButtonBackgroundTabsEnabled = nil;

  [_openNTPOnClosingLastTabEnabled stop];
  [_openNTPOnClosingLastTabEnabled setObserver:nil];
  _openNTPOnClosingLastTabEnabled = nil;

  [_focusOmniboxOnNTPEnabled stop];
  [_focusOmniboxOnNTPEnabled setObserver:nil];
  _focusOmniboxOnNTPEnabled = nil;

  if (IsInactiveTabsAvailable()) {
    _prefChangeRegistrar.RemoveAll();
    _prefObserverBridge.reset();
  }
  _local_prefs = nil;
  _consumer = nil;
}

#pragma mark - Private Helpers
- (BOOL)isBottomOmniboxEnabled {
  if (!_bottomOmniboxEnabled) {
    return NO;
  }
  return [_bottomOmniboxEnabled value];
}

- (BOOL)isReverseSearchResultOrder {
  if (!_reverseSearchResultsEnabled) {
    return NO;
  }
  return [_reverseSearchResultsEnabled value];
}

- (BOOL)isTabBarEnabled {
  if ([VivaldiGlobalHelpers isDeviceTablet] || !_tabBarEnabled) {
    return YES;
  }
  return [_tabBarEnabled value];
}

- (BOOL)showXButtonInBackgroundTab {
  if (!_showXButtonBackgroundTabsEnabled) {
    return NO;
  }
  return [_showXButtonBackgroundTabsEnabled value];
}

- (BOOL)openNTPOnClosingLastTab {
  if (!_openNTPOnClosingLastTabEnabled) {
    return YES;
  }
  return [_openNTPOnClosingLastTabEnabled value];
}

- (BOOL)focusOmniboxOnNTP {
  if (!_focusOmniboxOnNTPEnabled) {
    return YES;
  }
  return [_focusOmniboxOnNTPEnabled value];
}

- (NSString*)homepageURL {
  return [VivaldiTabSettingPrefs getHomepageUrlWithPrefService: _prefService];
}

- (VivaldiNTPType)newTabSetting {
  return [VivaldiTabSettingPrefs getNewTabSettingWithPrefService: _prefService];
}

- (NSString*)newTabURL {
  return [VivaldiTabSettingPrefs getNewTabUrlWithPrefService: _prefService];
}

- (int)defaultThreshold {
  // Use InactiveTabsTimeThreshold() instead of reading the pref value
  // directly as this function also manage flag and default value.
  int currentThreshold = IsInactiveTabsExplicitlyDisabledByUser()
                             ? kInactiveTabsDisabledByUser
                             : InactiveTabsTimeThreshold().InDays();
  return currentThreshold;
}

#pragma mark - Properties

- (void)setConsumer:(id<VivaldiTabsSettingsConsumer>)consumer {
  _consumer = consumer;
  [self.consumer setPreferenceForOmniboxAtBottom:[self isBottomOmniboxEnabled]];
  [self.consumer setPreferenceForReverseSearchResultOrder:
      [self isReverseSearchResultOrder]];
  [self.consumer setPreferenceForShowTabBar:[self isTabBarEnabled]];
  [self.consumer
      setPreferenceShowXButtonInBackgroundTab:
            [self showXButtonInBackgroundTab]];
  [self.consumer
      setPreferenceOpenNTPOnClosingLastTab:[self openNTPOnClosingLastTab]];
  [self.consumer
      setPreferenceFocusOmniboxOnNTP:[self focusOmniboxOnNTP]];

  [self.consumer setPreferenceForShowInactiveTabs:
      [VivaldiTabSettingPrefs isInactiveTabsAvailable]];
  if (IsInactiveTabsAvailable()) {
    [self.consumer
        setPreferenceForInactiveTabsTimeThreshold:[self defaultThreshold]];
  }

  [self.consumer setPreferenceForVivaldiNTPType:self.newTabSetting
                                        withURL:self.newTabURL];
}

#pragma mark - VivaldiTabsSettingsConsumer
- (void)setPreferenceForOmniboxAtBottom:(BOOL)omniboxAtBottom {
  if (omniboxAtBottom != [self isBottomOmniboxEnabled])
    [_bottomOmniboxEnabled setValue:omniboxAtBottom];
}

- (void)setPreferenceForReverseSearchResultOrder:(BOOL)reverseOrder {
  if (reverseOrder != [self isReverseSearchResultOrder])
    [_reverseSearchResultsEnabled setValue:reverseOrder];
}

- (void)setPreferenceForShowTabBar:(BOOL)showTabBar {
  if (showTabBar != [self isTabBarEnabled])
    [_tabBarEnabled setValue:showTabBar];
}

- (void)setPreferenceOpenNTPOnClosingLastTab:(BOOL)openNTP {
  if (openNTP != [self openNTPOnClosingLastTab])
    [_openNTPOnClosingLastTabEnabled setValue:openNTP];
}

- (void)setPreferenceFocusOmniboxOnNTP:(BOOL)focusOmnibox {
  if (focusOmnibox != [self focusOmniboxOnNTP])
    [_focusOmniboxOnNTPEnabled setValue:focusOmnibox];
}

- (void)setPreferenceShowXButtonInBackgroundTab:(BOOL)showXButton {
  if (showXButton != [self showXButtonInBackgroundTab])
    [_showXButtonBackgroundTabsEnabled setValue:showXButton];
}

- (void)setPreferenceForInactiveTabsTimeThreshold:(int)threshold {
  // No op.
}

- (void)setPreferenceForVivaldiNTPType:(VivaldiNTPType)setting
                               withURL:(NSString*)url {
  [VivaldiTabSettingPrefs setNewTabSettingWithPrefService: _prefService
                                               andSetting: setting
                                                  withURL: url];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _bottomOmniboxEnabled) {
    [self.consumer setPreferenceForOmniboxAtBottom:[observableBoolean value]];
  } else if (observableBoolean == _reverseSearchResultsEnabled) {
    [self.consumer setPreferenceForReverseSearchResultOrder:
        [observableBoolean value]];
  } else if (observableBoolean == _tabBarEnabled) {
    [self.consumer setPreferenceForShowTabBar:[observableBoolean value]];
  } else if (observableBoolean == _showXButtonBackgroundTabsEnabled) {
    [self.consumer
        setPreferenceShowXButtonInBackgroundTab:[observableBoolean value]];
  } else if (observableBoolean == _openNTPOnClosingLastTabEnabled) {
    [self.consumer
        setPreferenceOpenNTPOnClosingLastTab:[observableBoolean value]];
  } else if (observableBoolean == _focusOmniboxOnNTPEnabled) {
    [self.consumer
        setPreferenceFocusOmniboxOnNTP:[observableBoolean value]];
  }
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kInactiveTabsTimeThreshold) {
    CHECK(IsInactiveTabsAvailable());
    [_consumer
        setPreferenceForInactiveTabsTimeThreshold:_local_prefs->GetInteger(
                                            prefs::kInactiveTabsTimeThreshold)];
  }
}

@end
