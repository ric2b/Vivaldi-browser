// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/appearance/vivaldi_appearance_settings_mediator.h"

#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/ui/settings/appearance/vivaldi_appearance_settings_prefs_helper.h"
#import "prefs/vivaldi_pref_names.h"

@interface VivaldiAppearanceSettingsMediator () <BooleanObserver,
                                                PrefObserverDelegate> {
  PrefService* _prefs;
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
  // Observer for force webpage dark theme state
  PrefBackedBoolean* _forceWebpageDarkThemeEnabled;
  // Observer for dynamic accent color state
  PrefBackedBoolean* _dynamicAccentColorEnabled;
  // Observer for bottom omnibox state
  PrefBackedBoolean* _bottomOmniboxEnabled;
  // Observer for tab bar state
  PrefBackedBoolean* _tabBarEnabled;
}
@end

@implementation VivaldiAppearanceSettingsMediator

- (instancetype)initWithOriginalPrefService:(PrefService*)originalPrefService {
  self = [super init];
  if (self) {
    _prefs = originalPrefService;
    _prefChangeRegistrar.Init(_prefs);
    _prefObserverBridge.reset(new PrefObserverBridge(self));

    _prefObserverBridge->ObserveChangesForPreference(
        vivaldiprefs::kVivaldiWebsiteAppearanceStyle, &_prefChangeRegistrar);

    _prefObserverBridge->ObserveChangesForPreference(
        vivaldiprefs::kVivaldiCustomAccentColor, &_prefChangeRegistrar);

    _forceWebpageDarkThemeEnabled =
        [[PrefBackedBoolean alloc]
            initWithPrefService:originalPrefService
                prefName:vivaldiprefs::kVivaldiWebsiteAppearanceForceDarkTheme];
    [_forceWebpageDarkThemeEnabled setObserver:self];

    _dynamicAccentColorEnabled =
        [[PrefBackedBoolean alloc]
            initWithPrefService:originalPrefService
                prefName:vivaldiprefs::kVivaldiDynamicAccentColorEnabled];
    [_dynamicAccentColorEnabled setObserver:self];

    _bottomOmniboxEnabled =
        [[PrefBackedBoolean alloc]
            initWithPrefService:GetApplicationContext()->GetLocalState()
                prefName:prefs::kBottomOmnibox];
    [_bottomOmniboxEnabled setObserver:self];

    _tabBarEnabled =
        [[PrefBackedBoolean alloc]
            initWithPrefService:originalPrefService
                       prefName:vivaldiprefs::kVivaldiDesktopTabsEnabled];
    [_tabBarEnabled setObserver:self];
  }
  return self;
}

- (void)disconnect {
  _prefChangeRegistrar.RemoveAll();
  _prefObserverBridge.reset();
  _prefs = nil;

  [_forceWebpageDarkThemeEnabled stop];
  [_forceWebpageDarkThemeEnabled setObserver:nil];
  _forceWebpageDarkThemeEnabled = nil;

  [_dynamicAccentColorEnabled stop];
  [_dynamicAccentColorEnabled setObserver:nil];
  _dynamicAccentColorEnabled = nil;

  [_bottomOmniboxEnabled stop];
  [_bottomOmniboxEnabled setObserver:nil];
  _bottomOmniboxEnabled = nil;

  [_tabBarEnabled stop];
  [_tabBarEnabled setObserver:nil];
  _tabBarEnabled = nil;
}

#pragma mark - Properties

- (void)setConsumer:(id<VivaldiAppearanceSettingsConsumer>)consumer {
  _consumer = consumer;
  [self.consumer
      setPreferenceWebsiteAppearanceStyle:[self websiteAppearanceStyle]];
  [self.consumer
      setPreferenceWebpageForceDarkThemeEnabled:
          [_forceWebpageDarkThemeEnabled value]];

  [self.consumer
      setPreferenceDynamicAccentColorEnabled:
          [_dynamicAccentColorEnabled value]];
  [self.consumer
      setPreferenceCustomAccentColor:[self customAccentColor]];

  [self.consumer setPreferenceForOmniboxAtBottom:[_bottomOmniboxEnabled value]];
  [self.consumer setPreferenceForTabBarEnabled:[_tabBarEnabled value]];
}

#pragma mark - VivaldiAppearanceSettingsConsumer
- (void)setPreferenceWebsiteAppearanceStyle:(int)style {
  [VivaldiAppearanceSettingsPrefsHelper setWebsiteAppearanceStyle:style];
}

- (void)setPreferenceWebpageForceDarkThemeEnabled:(BOOL)forceDarkThemeEnabled {
  if (forceDarkThemeEnabled != [_forceWebpageDarkThemeEnabled value])
    [_forceWebpageDarkThemeEnabled setValue:forceDarkThemeEnabled];
}

- (void)setPreferenceCustomAccentColor:(NSString*)accentColor {
  [VivaldiAppearanceSettingsPrefsHelper setCustomAccentColor:accentColor];
}

- (void)setPreferenceDynamicAccentColorEnabled:(BOOL)dynamicAccentColorEnabled {
  if (dynamicAccentColorEnabled != [_dynamicAccentColorEnabled value])
    [_dynamicAccentColorEnabled setValue:dynamicAccentColorEnabled];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == vivaldiprefs::kVivaldiWebsiteAppearanceStyle) {
    [self.consumer
        setPreferenceWebsiteAppearanceStyle:[self websiteAppearanceStyle]];
  } else if (preferenceName == vivaldiprefs::kVivaldiCustomAccentColor) {
    [self.consumer
        setPreferenceCustomAccentColor:[self customAccentColor]];
  }
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _forceWebpageDarkThemeEnabled) {
    [self.consumer
        setPreferenceWebpageForceDarkThemeEnabled:[observableBoolean value]];
  } else if (observableBoolean == _dynamicAccentColorEnabled) {
    [self.consumer
        setPreferenceDynamicAccentColorEnabled:[observableBoolean value]];
  } else if (observableBoolean == _bottomOmniboxEnabled) {
    [self.consumer setPreferenceForOmniboxAtBottom:[observableBoolean value]];
  } else if (observableBoolean == _tabBarEnabled) {
    [self.consumer setPreferenceForTabBarEnabled:[observableBoolean value]];
  }
}

#pragma mark - Private
- (int)websiteAppearanceStyle {
  return [VivaldiAppearanceSettingsPrefsHelper getWebsiteAppearanceStyle];
}

- (NSString*)customAccentColor {
  return [VivaldiAppearanceSettingsPrefsHelper getCustomAccentColor];
}

@end
