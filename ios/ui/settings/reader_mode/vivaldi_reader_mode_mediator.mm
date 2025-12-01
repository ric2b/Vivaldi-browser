// Copyright 2025 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/reader_mode/vivaldi_reader_mode_mediator.h"

#import "base/check.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/reader_mode/reader_mode_controller.h"
#import "ios/ui/settings/reader_mode/vivaldi_reader_mode_prefs.h"
#import "ios/web/public/web_state.h"
#import "prefs/vivaldi_pref_names.h"

@interface VivaldiReaderModeMediator () {
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;
}

// Preference service.
@property(nonatomic, assign) PrefService* prefService;

@end

@implementation VivaldiReaderModeMediator

- (instancetype)initWithBrowser:(Browser*)browser {
  self = [super init];
  if (self) {
    _browser = browser;
    _prefService = browser->GetProfile()->GetOriginalProfile()->GetPrefs();
    _prefChangeRegistrar = std::make_unique<PrefChangeRegistrar>();
    _prefChangeRegistrar->Init(_prefService);

    // Register for preference changes
    __weak __typeof(self) weakSelf = self;
    _prefChangeRegistrar->Add(
        vivaldiprefs::kReaderModeFontSize,
        base::BindRepeating(^(const std::string& pref_name) {
          [weakSelf updateUIFromPrefs];
        }));

    _prefChangeRegistrar->Add(
        vivaldiprefs::kReaderModeFontFamily,
        base::BindRepeating(^(const std::string& pref_name) {
          [weakSelf updateUIFromPrefs];
        }));

    _prefChangeRegistrar->Add(
        vivaldiprefs::kReaderModeTheme,
        base::BindRepeating(^(const std::string& pref_name) {
          [weakSelf updateUIFromPrefs];
        }));
  }
  // It set reader mode once the ui get initalized
  [self setReaderModeEnabled:true];
  return self;
}

- (void)disconnect {
  // Stop listening to pref changes
  _prefChangeRegistrar.reset();
}

- (web::WebState*)getActiveWebState {
  if (!_browser) {
    return nullptr;
  }

  WebStateList* webStateList = _browser->GetWebStateList();
  if (!webStateList) {
    return nullptr;
  }

  web::WebState* webState = webStateList->GetActiveWebState();
  return webState;
}

- (void)updateUIFromPrefs {
  if (!self.consumer) {
    return;
  }

  // Get reader mode state from preferences using the prefs class
  int fontSize = [self getCurrentFontSize];
  NSString* fontFamily = [self getCurrentFontFamily];
  NSString* theme = [self getCurrentTheme];

  // Update the UI
  [self.consumer setFontSize:static_cast<int32_t>(fontSize)];
  [self.consumer setFontFamily:fontFamily];
  [self.consumer setTheme:theme];

  // Update reader mode enabled state
  web::WebState* webState = [self getActiveWebState];
  if (webState) {
    // Ensure controllers have Browser to present UI from.
    ReaderModeController::SetBrowser(_browser);
    ReaderModeController* controller =
      ReaderModeController::FromWebState(webState);
    if (controller) {
      bool readerModeEnabled = controller->IsReaderModeEnabled();
      [self.consumer setReaderModeEnabled:readerModeEnabled];
    }
  }
}

#pragma mark - Private

- (int)getCurrentFontSize {
  return [VivaldiReaderModePrefs
              getReaderModeFontSizeWithPrefService:_prefService];
}

- (NSString*)getCurrentFontFamily {
  return [VivaldiReaderModePrefs
              getReaderModeFontFamilyWithPrefService:_prefService];
}

- (NSString*)getCurrentTheme {
    return [VivaldiReaderModePrefs
              getReaderModeThemeWithPrefService:_prefService];
}

- (void)setCurrentFontSize:(int)fontSize {
  [VivaldiReaderModePrefs setReaderModeFontSizeWithPrefService:fontSize
                                                inPrefServices:_prefService];
}

- (void)setCurrentFontFamily:(NSString*)fontFamily {
  [VivaldiReaderModePrefs setReaderModeFontFamilyWithPrefService:fontFamily
                                                  inPrefServices:_prefService];
}

- (void)setCurrentTheme:(NSString*)theme {
  [VivaldiReaderModePrefs setReaderModeThemeWithPrefService:theme
                                             inPrefServices:_prefService];
}

#pragma mark - Public methods

- (BOOL)toggleReaderMode {
  web::WebState* webState = [self getActiveWebState];
  if (!webState) {
    return NO;
  }
  ReaderModeController* controller =
      ReaderModeController::FromWebState(webState);
  if (!controller) {
    return NO;
  }
  bool isEnabled = controller->ToggleReaderMode();
  [self.consumer setReaderModeEnabled:isEnabled];
  return isEnabled;
}

- (void)applyReaderModeSettings {
  web::WebState* webState = [self getActiveWebState];
  if (!webState) {
    return;
  }
  ReaderModeController::SetBrowser(_browser);
  ReaderModeController* controller =
    ReaderModeController::FromWebState(webState);
  if (!controller || !controller->IsReaderModeEnabled()) {
    return;
  }
  // Get settings from preferences using the prefs class
  int fontSize = [self getCurrentFontSize];
  NSString* fontFamily = [self getCurrentFontFamily];
  NSString* theme = [self getCurrentTheme];

  // Apply settings through controller
  controller->SetFontSize(fontSize);
  controller->SetFontFamily([fontFamily UTF8String]);
  controller->SetTheme([theme UTF8String]);
}

#pragma mark - VivaldiReaderModeConsumer

- (void)setReaderModeEnabled:(BOOL)enabled {
  web::WebState* webState = [self getActiveWebState];
  if (!webState) {
    return;
  }

  ReaderModeController* controller =
    ReaderModeController::FromWebState(webState);
  if (!controller) {
    return;
  }

  if (enabled) {
    controller->ApplyReaderMode();

    // Apply saved settings after enabling reader mode
    [self applyReaderModeSettings];
  } else {
    controller->DisableReaderMode();
  }
}

- (void)setFontSize:(int)size {
  // Get the active web state
  web::WebState* webState = [self getActiveWebState];
  if (webState) {
    // Apply the font size through controller
    ReaderModeController* controller =
        ReaderModeController::FromWebState(webState);
    if (controller) {
      controller->SetFontSize(size);
    }
  }

  if (!_prefService) {
    return;
  }

  // Save the font size to preferences
  [self setCurrentFontSize:size];
}

- (void)setFontFamily:(NSString*)family {
  // Get the active web state
  web::WebState* webState = [self getActiveWebState];
  if (webState) {
    // Apply the font family through controller
    ReaderModeController* controller =
      ReaderModeController::FromWebState(webState);
    if (controller) {
      controller->SetFontFamily([family UTF8String]);
    }
  }

  if (!_prefService) {
    return;
  }

  // Save the font family to preferences
  [self setCurrentFontFamily:family];
}

- (void)setTheme:(NSString*)theme {
  // Get the active web state
  web::WebState* webState = [self getActiveWebState];
  if (webState) {
    // Apply the theme through controller
    ReaderModeController* controller =
        ReaderModeController::FromWebState(webState);
    if (controller) {
      controller->SetTheme([theme UTF8String]);
    }
  }

  if (!_prefService) {
    return;
  }

  // Save the theme to preferences
  [self setCurrentTheme:theme];
}

@end
