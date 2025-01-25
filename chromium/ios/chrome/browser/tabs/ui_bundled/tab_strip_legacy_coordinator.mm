// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/ui_bundled/tab_strip_legacy_coordinator.h"

#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/tab_strip_commands.h"
#import "ios/chrome/browser/tabs/ui_bundled/requirements/tab_strip_presentation.h"
#import "ios/chrome/browser/tabs/ui_bundled/tab_strip_controller.h"

// Vivaldi
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/ui/settings/appearance/vivaldi_appearance_settings_prefs_helper.h"
#import "ios/ui/settings/appearance/vivaldi_appearance_settings_prefs.h"
#import "prefs/vivaldi_pref_names.h"
// End Vivaldi

@interface TabStripLegacyCoordinator ()

// Vivaldi
<BooleanObserver,
PrefObserverDelegate> {
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
  // Observer for dynamic accent color state
  PrefBackedBoolean* _dynamicAccentColorEnabled;
  /// Pref tracking if bottom omnibox is enabled.
  PrefBackedBoolean* _bottomOmniboxEnabled;
  /// Pref tracking if X button for background tabs is enabled.
  PrefBackedBoolean* _showXButtonBackgroundTabsEnabled;
}
// End Vivaldi

@property(nonatomic, assign) BOOL started;
@property(nonatomic, strong) TabStripController* tabStripController;
@end

@implementation TabStripLegacyCoordinator
@synthesize presentationProvider = _presentationProvider;
@synthesize started = _started;
@synthesize tabStripController = _tabStripController;
@synthesize animationWaitDuration = _animationWaitDuration;
@synthesize baseViewController = _baseViewController;

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);
  return [super initWithBaseViewController:nil browser:browser];
}

- (void)setPresentationProvider:(id<TabStripPresentation>)presentationProvider {
  DCHECK(!self.started);
  _presentationProvider = presentationProvider;
}

- (void)setAnimationWaitDuration:(NSTimeInterval)animationWaitDuration {
  DCHECK(!self.started);
  _animationWaitDuration = animationWaitDuration;
}

- (void)setHighlightsSelectedTab:(BOOL)highlightsSelectedTab {
  DCHECK(self.started);
  self.tabStripController.highlightsSelectedTab = highlightsSelectedTab;
}

- (void)hideTabStrip:(BOOL)hidden {
  [self.tabStripController hideTabStrip:hidden];
}

- (void)tabStripSizeDidChange {
  [self.tabStripController tabStripSizeDidChange];
}

#pragma mark - ChromeCoordinator

- (void)start {
  DCHECK(self.browser);
  DCHECK(self.presentationProvider);
  TabStripStyle style =
      self.browser->GetProfile()->IsOffTheRecord() ? INCOGNITO : NORMAL;
  self.tabStripController = [[TabStripController alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           style:style
               layoutGuideCenter:LayoutGuideCenterForBrowser(self.browser)];
  self.tabStripController.presentationProvider = self.presentationProvider;
  self.tabStripController.animationWaitDuration = self.animationWaitDuration;
  [self.presentationProvider showTabStripView:[self.tabStripController view]];
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:_tabStripController
                   forProtocol:@protocol(TabStripCommands)];

  // Vivaldi
  [self startObservingOmniboxPosition:GetApplicationContext()->GetLocalState()];
  PrefService* prefService = self.browser->GetProfile()->GetPrefs();
  if (prefService) {
    [VivaldiAppearanceSettingPrefs setPrefService:prefService];
    [self startObservingTabStyles:prefService];
    [self startObservingAccentColorChange:prefService];
  }
  // End Vivaldi

  self.started = YES;
}

- (void)stop {
  self.started = NO;
  [self.tabStripController disconnect];
  self.tabStripController = nil;
  self.presentationProvider = nil;
  [self.browser->GetCommandDispatcher()
      stopDispatchingForProtocol:@protocol(TabStripCommands)];

  // Vivaldi
  if (_showXButtonBackgroundTabsEnabled) {
    [_showXButtonBackgroundTabsEnabled stop];
    [_showXButtonBackgroundTabsEnabled setObserver:nil];
    _showXButtonBackgroundTabsEnabled = nil;
  }
  if (_bottomOmniboxEnabled) {
    [_bottomOmniboxEnabled stop];
    [_bottomOmniboxEnabled setObserver:nil];
    _bottomOmniboxEnabled = nil;
  }
  if (_dynamicAccentColorEnabled) {
    [_dynamicAccentColorEnabled stop];
    [_dynamicAccentColorEnabled setObserver:nil];
    _dynamicAccentColorEnabled = nil;
  }
  _prefChangeRegistrar.RemoveAll();
  _prefObserverBridge.reset();
  // End Vivaldi

}

#pragma mark VIVALDI
// Private
- (void)startObservingOmniboxPosition:(PrefService*)prefService {
  _bottomOmniboxEnabled =
      [[PrefBackedBoolean alloc] initWithPrefService:prefService
                                            prefName:prefs::kBottomOmnibox];
  [_bottomOmniboxEnabled setObserver:self];
  [self booleanDidChange:_bottomOmniboxEnabled];
}

- (void)startObservingTabStyles:(PrefService*)prefService {
  _showXButtonBackgroundTabsEnabled =
      [[PrefBackedBoolean alloc]
          initWithPrefService:prefService
             prefName:vivaldiprefs::kVivaldiShowXButtonBackgroundTabsEnabled];
  [_showXButtonBackgroundTabsEnabled setObserver:self];
  [self booleanDidChange:_showXButtonBackgroundTabsEnabled];
}

- (void)startObservingAccentColorChange:(PrefService*)prefService {
  // Dynamic accent color toggle
  _dynamicAccentColorEnabled =
      [[PrefBackedBoolean alloc]
          initWithPrefService:prefService
                     prefName:vivaldiprefs::kVivaldiDynamicAccentColorEnabled];
  [_dynamicAccentColorEnabled setObserver:self];
  [self booleanDidChange:_dynamicAccentColorEnabled];

  // Custom accent color
  _prefChangeRegistrar.Init(prefService);
  _prefObserverBridge.reset(new PrefObserverBridge(self));

  _prefObserverBridge->ObserveChangesForPreference(
      vivaldiprefs::kVivaldiCustomAccentColor, &_prefChangeRegistrar);
  [self onPreferenceChanged:vivaldiprefs::kVivaldiCustomAccentColor];
}

- (NSString*)customAccentColor {
  return [VivaldiAppearanceSettingsPrefsHelper getCustomAccentColor];
}

// Setters
- (void)scrollToSelectedTab:(web::WebState*)webState
                   animated:(BOOL)animated {
  [self.tabStripController scrollToSelectedTab:webState
                                      animated:animated];
}

#pragma mark - Boolean Observer
- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _bottomOmniboxEnabled) {
    self.tabStripController.toolbarType = _bottomOmniboxEnabled.value
        ? ToolbarType::kSecondary
        : ToolbarType::kPrimary;
  } else if (observableBoolean == _dynamicAccentColorEnabled) {
    self.tabStripController.dynamicAccentColorEnabled =
        [observableBoolean value];
  } else if (observableBoolean == _showXButtonBackgroundTabsEnabled) {
    self.tabStripController.showXButtonForBackgroundTabs =
        [observableBoolean value];
  }
}

#pragma mark - PrefObserverDelegate
- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == vivaldiprefs::kVivaldiCustomAccentColor) {
    self.tabStripController.customAccentColor = [self customAccentColor];
  }
} // Vivaldi

@end
