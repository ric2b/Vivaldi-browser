// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"

#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_constants+vivaldi.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/ntp/vivaldi_ntp_constants.h"
#import "ios/ui/toolbar/vivaldi_toolbar_constants.h"

using vivaldi::IsVivaldiRunning;
// End Vivaldi

@implementation ToolbarConfiguration

- (instancetype)initWithStyle:(ToolbarStyle)style {
  self = [super init];
  if (self) {
    _style = style;
  }
  return self;
}

- (UIColor*)NTPBackgroundColor {

  if (IsVivaldiRunning())
    return self.backgroundColor; // End Vivaldi

  return ntp_home::NTPBackgroundColor();
}

- (UIColor*)backgroundColor {

  if (IsVivaldiRunning()) {
    switch (self.style) {
     case ToolbarStyle::kNormal:
       return [UIColor colorNamed:vNTPBackgroundColor];
     case ToolbarStyle::kIncognito:
       return [UIColor colorNamed:vPrivateModeToolbarBackgroundColor];
    }
  } else {
  return [UIColor colorNamed:kBackgroundColor];
  } // End Vivaldi

}

- (UIColor*)focusedBackgroundColor {
  return [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
}

- (UIColor*)focusedLocationBarBackgroundColor {
  return [UIColor colorNamed:kTextfieldFocusedBackgroundColor];
}

#if !defined(VIVALDI_BUILD)
- (UIColor*)buttonsTintColor {
  return [UIColor colorNamed:kToolbarButtonColor];
}
#endif  // End Vivaldi

- (UIColor*)buttonsTintColorHighlighted {
  return [UIColor colorNamed:@"tab_toolbar_button_color_highlighted"];
}

- (UIColor*)buttonsTintColorIPHHighlighted {
  return [UIColor colorNamed:kSolidButtonTextColor];
}

- (UIColor*)buttonsIPHHighlightColor {
  return [UIColor colorNamed:kBlueColor];
}

- (UIColor*)locationBarBackgroundColorWithVisibility:(CGFloat)visibilityFactor {

  if (IsVivaldiRunning()) {
    switch (self.style) {
      case ToolbarStyle::kNormal:
        return [[UIColor colorNamed:vSearchbarBackgroundColor]
            colorWithAlphaComponent:visibilityFactor];
      case ToolbarStyle::kIncognito:
        return [[UIColor colorNamed:vPrivateNTPBackgroundColor]
            colorWithAlphaComponent:visibilityFactor];
    }
  } else {
  // For the omnibox specifically, the background should be different in
  // incognito compared to dark mode.
  switch (self.style) {
    case ToolbarStyle::kNormal:
      return [[UIColor colorNamed:kTextfieldBackgroundColor]
          colorWithAlphaComponent:visibilityFactor];
    case ToolbarStyle::kIncognito:
      return [[UIColor colorNamed:@"omnibox_incognito_background_color"]
          colorWithAlphaComponent:visibilityFactor];
  }
  } // End Vivaldi

}

- (NSString*)accessibilityLabelForOpenNewTabButtonInGroup:(BOOL)inGroup {
  switch (self.style) {
    case ToolbarStyle::kNormal:
      return l10n_util::GetNSString(inGroup
                                        ? IDS_IOS_TOOLBAR_OPEN_NEW_TAB_IN_GROUP
                                        : IDS_IOS_TOOLBAR_OPEN_NEW_TAB);
    case ToolbarStyle::kIncognito:
      return l10n_util::GetNSString(
          inGroup ? IDS_IOS_TOOLBAR_OPEN_NEW_TAB_INCOGNITO_IN_GROUP
                  : IDS_IOS_TOOLBAR_OPEN_NEW_TAB_INCOGNITO);
  }
}

#pragma mark - Vivaldi
- (UIColor*)primaryToolbarAccentColor {
  switch (self.style) {
   case ToolbarStyle::kNormal:
     return [UIColor colorNamed:vRegularToolbarBackgroundColor];
   case ToolbarStyle::kIncognito:
     return [UIColor colorNamed:vPrivateModeToolbarBackgroundColor];
  }
}

- (UIColor*)locationBarBackgroundColorForAccentColor:(UIColor*)accentColor {
  switch (self.style) {
    case ToolbarStyle::kNormal: {
      BOOL shouldUseDarkColor =
          [VivaldiGlobalHelpers shouldUseDarkTextForColor:accentColor];
      UIColor* locationBarColor = shouldUseDarkColor ?
          [UIColor colorNamed:vLocationBarDarkBGColor] :
          [UIColor colorNamed:vLocationBarLightBGColor];
      return locationBarColor;
    }
    case ToolbarStyle::kIncognito:
      return [UIColor colorNamed:vPrivateNTPBackgroundColor];
  }
}

- (UIColor*)locationBarSteadyViewTintColorForAccentColor:(UIColor*)accentColor {
  switch (self.style) {
    case ToolbarStyle::kNormal: {
      BOOL shouldUseDarkColor =
          [VivaldiGlobalHelpers shouldUseDarkTextForColor:accentColor];
      UIColor* locationBarColor = shouldUseDarkColor ?
          [UIColor blackColor] : [UIColor whiteColor];
      return locationBarColor;
    }
    case ToolbarStyle::kIncognito:
      return [UIColor whiteColor];
  }
}

- (UIColor*)buttonsTintColorForAccentColor:(UIColor*)accentColor {
  switch (self.style) {
    case ToolbarStyle::kNormal: {
      BOOL shouldUseDarkColor =
          [VivaldiGlobalHelpers shouldUseDarkTextForColor:accentColor];
      UIColor* locationBarColor = shouldUseDarkColor ?
          [UIColor colorNamed:vToolbarDarkButton] :
          [UIColor colorNamed:vToolbarLightButton];
      return locationBarColor;
    }
    case ToolbarStyle::kIncognito:
      return [UIColor colorNamed:vToolbarLightButton];
  }
  // End Vivaldi
}

@end
