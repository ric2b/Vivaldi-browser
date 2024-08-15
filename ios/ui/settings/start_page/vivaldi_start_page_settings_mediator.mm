// Copyright 2024 Vivaldi Technologies. All rights reserved.
#import "ios/ui/settings/start_page/vivaldi_start_page_settings_mediator.h"

#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/ui/settings/start_page/vivaldi_start_page_prefs_helper.h"
#import "ios/ui/settings/start_page/vivaldi_start_page_prefs.h"
#import "prefs/vivaldi_pref_names.h"

@interface VivaldiStartPageSettingsMediator ()<BooleanObserver,
                                              PrefObserverDelegate>
@end

@implementation VivaldiStartPageSettingsMediator {
  // Preference service from the application context.
  PrefService* _prefs;
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
  // Observer for frequently visited pages visibility state
  PrefBackedBoolean* _showFrequentlyVisited;
  // Observer for speed dials visibility state
  PrefBackedBoolean* _showSpeedDials;
  // Observer for start page customize button visibility state
  PrefBackedBoolean* _showCustomizeStartPageButton;
}

- (instancetype)initWithOriginalPrefService:(PrefService*)originalPrefService {
  self = [super init];
  if (self) {
    _prefs = originalPrefService;
    _prefChangeRegistrar.Init(_prefs);
    _prefObserverBridge.reset(new PrefObserverBridge(self));
    _prefObserverBridge->ObserveChangesForPreference(
        vivaldiprefs::kVivaldiStartPageLayoutStyle, &_prefChangeRegistrar);

    _showFrequentlyVisited =
        [[PrefBackedBoolean alloc]
            initWithPrefService:originalPrefService
                prefName:vivaldiprefs::kVivaldiStartPageShowFrequentlyVisited];
    [_showFrequentlyVisited setObserver:self];

    _showSpeedDials =
        [[PrefBackedBoolean alloc]
            initWithPrefService:originalPrefService
                prefName:vivaldiprefs::kVivaldiStartPageShowSpeedDials];
    [_showSpeedDials setObserver:self];
    [self booleanDidChange:_showSpeedDials];

    _showCustomizeStartPageButton =
        [[PrefBackedBoolean alloc]
             initWithPrefService:_prefs
                prefName:vivaldiprefs::kVivaldiStartPageShowCustomizeButton];
    [_showCustomizeStartPageButton setObserver:self];

    [self booleanDidChange:_showCustomizeStartPageButton];

    [VivaldiStartPagePrefs setPrefService:_prefs];
  }
  return self;
}

- (void)disconnect {
  _prefChangeRegistrar.RemoveAll();
  _prefObserverBridge.reset();
  _prefs = nil;

  [_showFrequentlyVisited stop];
  [_showFrequentlyVisited setObserver:nil];
  _showFrequentlyVisited = nil;

  [_showSpeedDials stop];
  [_showSpeedDials setObserver:nil];
  _showSpeedDials = nil;

  [_showCustomizeStartPageButton stop];
  [_showCustomizeStartPageButton setObserver:nil];
  _showCustomizeStartPageButton = nil;
}

#pragma mark - Properties

- (void)setConsumer:(id<VivaldiStartPageSettingsConsumer>)consumer {
  _consumer = consumer;
  [self.consumer setPreferenceShowSpeedDials:[_showSpeedDials value]];
  [self.consumer
      setPreferenceShowFrequentlyVisitedPages:[_showFrequentlyVisited value]];
  [self.consumer setPreferenceSpeedDialLayout:[self currentLayoutStyle]];
  [self.consumer setPreferenceShowCustomizeStartPageButton:
      [_showCustomizeStartPageButton value]];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == vivaldiprefs::kVivaldiStartPageLayoutStyle) {
    [self.consumer setPreferenceSpeedDialLayout:[self currentLayoutStyle]];
  }
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _showSpeedDials) {
    [self.consumer setPreferenceShowSpeedDials:[observableBoolean value]];
  } else if (observableBoolean == _showFrequentlyVisited) {
    [self.consumer
        setPreferenceShowFrequentlyVisitedPages:[observableBoolean value]];
  } else if (observableBoolean == _showCustomizeStartPageButton) {
    [self.consumer
        setPreferenceShowCustomizeStartPageButton:[observableBoolean value]];
  }
}

#pragma mark - VivaldiStartPageSettingsConsumer

- (void)setPreferenceShowFrequentlyVisitedPages:(BOOL)showFrequentlyVisited {
  if (showFrequentlyVisited != [_showFrequentlyVisited value])
    [_showFrequentlyVisited setValue:showFrequentlyVisited];
}

- (void)setPreferenceShowSpeedDials:(BOOL)showSpeedDials {
  if (showSpeedDials != [_showSpeedDials value])
    [_showSpeedDials setValue:showSpeedDials];
}

- (void)setPreferenceShowCustomizeStartPageButton:(BOOL)showCustomizeButton {
  if (showCustomizeButton != [_showCustomizeStartPageButton value])
    [_showCustomizeStartPageButton setValue:showCustomizeButton];
}

- (void)setPreferenceSpeedDialLayout:
    (VivaldiStartPageLayoutStyle)layoutStyle {
  // No op.
}

- (void)setPreferenceSpeedDialColumn:(VivaldiStartPageLayoutColumn)column {
  // No op.
}

#pragma mark - Private
- (VivaldiStartPageLayoutStyle)currentLayoutStyle {
  return [VivaldiStartPagePrefsHelper getStartPageLayoutStyle];
}

@end
