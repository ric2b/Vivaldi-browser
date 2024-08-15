// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/tabs/vivaldi_tab_settings_mediator.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "prefs/vivaldi_pref_names.h"

@interface VivaldiTabSettingsMediator () <BooleanObserver> {
  PrefBackedBoolean* _bottomOmniboxEnabled;
  PrefBackedBoolean* _reverseSearchResultsEnabled;
  PrefBackedBoolean* _tabBarEnabled;
}
@end

@implementation VivaldiTabSettingsMediator

- (instancetype)initWithOriginalPrefService:(PrefService*)originalPrefService {
  self = [super init];
  if (self) {
    _bottomOmniboxEnabled =
        [[PrefBackedBoolean alloc] initWithPrefService:originalPrefService
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
}

#pragma mark - Properties

- (void)setConsumer:(id<VivaldiTabsSettingsConsumer>)consumer {
  _consumer = consumer;
  [self.consumer setPreferenceForOmniboxAtBottom:[_bottomOmniboxEnabled value]];
  [self.consumer setPreferenceForReverseSearchResultOrder:
      [_reverseSearchResultsEnabled value]];
  [self.consumer setPreferenceForShowTabBar:[_tabBarEnabled value]];
}

#pragma mark - VivaldiTabsSettingsConsumer
- (void)setPreferenceForOmniboxAtBottom:(BOOL)omniboxAtBottom {
  if (omniboxAtBottom != [_bottomOmniboxEnabled value])
    [_bottomOmniboxEnabled setValue:omniboxAtBottom];
}

- (void)setPreferenceForReverseSearchResultOrder:(BOOL)reverseOrder {
  if (reverseOrder != [_reverseSearchResultsEnabled value])
    [_reverseSearchResultsEnabled setValue:reverseOrder];
}

- (void)setPreferenceForShowTabBar:(BOOL)showTabBar {
  if (showTabBar != [_tabBarEnabled value])
    [_tabBarEnabled setValue:showTabBar];
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
  }
}

@end
