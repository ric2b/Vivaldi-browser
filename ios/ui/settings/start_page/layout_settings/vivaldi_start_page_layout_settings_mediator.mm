// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/start_page/layout_settings/vivaldi_start_page_layout_settings_mediator.h"

#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/ui/settings/start_page/layout_settings/vivaldi_start_page_layout_column.h"
#import "ios/ui/settings/start_page/layout_settings/vivaldi_start_page_layout_style.h"
#import "ios/ui/settings/start_page/vivaldi_start_page_prefs_helper.h"
#import "ios/ui/settings/start_page/vivaldi_start_page_prefs.h"
#import "prefs/vivaldi_pref_names.h"

@interface VivaldiStartPageLayoutSettingsMediator ()<PrefObserverDelegate>
@end

@implementation VivaldiStartPageLayoutSettingsMediator {
  // Preference service from the application context.
  PrefService* _prefs;
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
}

- (instancetype)initWithOriginalPrefService:(PrefService*)originalPrefService {
  self = [super init];
  if (self) {
    _prefs = originalPrefService;
    _prefChangeRegistrar.Init(_prefs);
    _prefObserverBridge.reset(new PrefObserverBridge(self));

    _prefObserverBridge->ObserveChangesForPreference(
          vivaldiprefs::kVivaldiStartPageLayoutStyle, &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
          vivaldiprefs::kVivaldiStartPageSDMaximumColumns,
              &_prefChangeRegistrar);

    [VivaldiStartPagePrefs setPrefService:_prefs];
  }
  return self;
}

- (void)disconnect {
  _prefChangeRegistrar.RemoveAll();
  _prefObserverBridge.reset();
  _prefs = nil;
}

#pragma mark - Properties

- (void)setConsumer:(id<VivaldiStartPageSettingsConsumer>)consumer {
  _consumer = consumer;

  [self.consumer setPreferenceSpeedDialLayout: [self currentLayoutStyle]];
  [self.consumer setPreferenceSpeedDialColumn:[self currentLayoutColumn]];
}

#pragma mark - VivaldiStartPageSettingsConsumer
- (void)setPreferenceShowFrequentlyVisitedPages:(BOOL)showFrequentlyVisited {
  // No op.
}

- (void)setPreferenceShowSpeedDials:(BOOL)showSpeedDials {
  // No op.
}

- (void)setPreferenceShowCustomizeStartPageButton:(BOOL)showCustomizeButton {
  // No op.
}

- (void)setPreferenceSpeedDialLayout:(VivaldiStartPageLayoutStyle)layout {
  [VivaldiStartPagePrefsHelper setStartPageLayoutStyle:layout];
}

- (void)setPreferenceSpeedDialColumn:(VivaldiStartPageLayoutColumn)column {
  [VivaldiStartPagePrefsHelper setStartPageSpeedDialMaximumColumns:column];
}

- (void)setPreferenceStartPageReopenWithItem:
    (VivaldiStartPageStartItemType)item {
  // No op.
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == vivaldiprefs::kVivaldiStartPageLayoutStyle) {
    [self.consumer setPreferenceSpeedDialLayout:[self currentLayoutStyle]];
  } else if (preferenceName ==
             vivaldiprefs::kVivaldiStartPageSDMaximumColumns) {
    [self.consumer setPreferenceSpeedDialColumn:[self currentLayoutColumn]];
  }
}

#pragma mark - Private

- (VivaldiStartPageLayoutStyle)currentLayoutStyle {
  return [VivaldiStartPagePrefsHelper getStartPageLayoutStyle];
}

- (VivaldiStartPageLayoutColumn)currentLayoutColumn {
  return [VivaldiStartPagePrefsHelper getStartPageSpeedDialMaximumColumns];
}

@end
