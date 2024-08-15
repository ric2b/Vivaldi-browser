// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_factory.h"

#import "base/ios/ios_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_actions_handler.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_visibility_configuration.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tab_grid_button.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/grit/ios_theme_resources.h"
#import "ios/public/provider/chrome/browser/raccoon/raccoon_api.h"
#import "ui/base/l10n/l10n_util.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_constants.h"
#import "ios/ui/helpers/vivaldi_colors_helper.h"
#import "ios/ui/settings/vivaldi_settings_constants.h"
#import "ios/ui/toolbar/vivaldi_toolbar_constants.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

using l10n_util::GetNSString;
using vivaldi::IsVivaldiRunning;
// End Vivaldi

namespace {

// The size of the symbol image.
const CGFloat kSymbolToolbarPointSize = 24;

}  // namespace

@implementation ToolbarButtonFactory

- (instancetype)initWithStyle:(ToolbarStyle)style {
  self = [super init];
  if (self) {
    _style = style;
    _toolbarConfiguration = [[ToolbarConfiguration alloc] initWithStyle:style];
  }
  return self;
}

#pragma mark - Buttons

- (ToolbarButton*)backButton {
  auto loadImageBlock = ^UIImage* {
    UIImage* backImage =
        DefaultSymbolWithPointSize(kBackSymbol, kSymbolToolbarPointSize);

    if (IsVivaldiRunning())
      backImage = [UIImage imageNamed:vToolbarBackButtonIcon]; // End Vivaldi

    return [backImage imageFlippedForRightToLeftLayoutDirection];
  };

  ToolbarButton* backButton = nil;
  if (base::FeatureList::IsEnabled(kEnableStartupImprovements)) {
    backButton = [[ToolbarButton alloc] initWithImageLoader:loadImageBlock];
  } else {
    backButton = [[ToolbarButton alloc] initWithImage:loadImageBlock()];
  }

  [self configureButton:backButton width:kAdaptiveToolbarButtonWidth];
  backButton.accessibilityLabel = l10n_util::GetNSString(IDS_ACCNAME_BACK);
  [backButton addTarget:self.actionHandler
                 action:@selector(backAction)
       forControlEvents:UIControlEventTouchUpInside];
  backButton.visibilityMask = self.visibilityConfiguration.backButtonVisibility;
  return backButton;
}

// Returns a forward button without visibility mask configured.
- (ToolbarButton*)forwardButton {
  auto loadImageBlock = ^UIImage* {
    UIImage* forwardImage =
        DefaultSymbolWithPointSize(kForwardSymbol, kSymbolToolbarPointSize);

    if (IsVivaldiRunning())
      forwardImage = [UIImage imageNamed:vToolbarForwardButtonIcon];

    return [forwardImage imageFlippedForRightToLeftLayoutDirection];
  };

  ToolbarButton* forwardButton = nil;
  if (base::FeatureList::IsEnabled(kEnableStartupImprovements)) {
    forwardButton = [[ToolbarButton alloc] initWithImageLoader:loadImageBlock];
  } else {
    forwardButton = [[ToolbarButton alloc] initWithImage:loadImageBlock()];
  }

  [self configureButton:forwardButton width:kAdaptiveToolbarButtonWidth];
  forwardButton.visibilityMask =
      self.visibilityConfiguration.forwardButtonVisibility;
  forwardButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_ACCNAME_FORWARD);
  [forwardButton addTarget:self.actionHandler
                    action:@selector(forwardAction)
          forControlEvents:UIControlEventTouchUpInside];
  return forwardButton;
}

- (ToolbarTabGridButton*)tabGridButton {
  auto loadImageBlock = ^UIImage* {

    if (IsVivaldiRunning())
      return [UIImage imageNamed:vToolbarTabSwitcherButtonIcon];
    // End Vivaldi

    return CustomSymbolWithPointSize(kSquareNumberSymbol,
                                     kSymbolToolbarPointSize);
  };

  ToolbarTabGridButton* tabGridButton = nil;
  if (base::FeatureList::IsEnabled(kEnableStartupImprovements)) {
    tabGridButton =
        [[ToolbarTabGridButton alloc] initWithImageLoader:loadImageBlock];
  } else {
    tabGridButton =
        [[ToolbarTabGridButton alloc] initWithImage:loadImageBlock()];
  }

  [self configureButton:tabGridButton width:kAdaptiveToolbarButtonWidth];
  SetA11yLabelAndUiAutomationName(tabGridButton, IDS_IOS_TOOLBAR_SHOW_TABS,
                                  kToolbarStackButtonIdentifier);
  [tabGridButton addTarget:self.actionHandler
                    action:@selector(tabGridTouchDown)
          forControlEvents:UIControlEventTouchDown];
  [tabGridButton addTarget:self.actionHandler
                    action:@selector(tabGridTouchUp)
          forControlEvents:UIControlEventTouchUpInside];
  tabGridButton.visibilityMask =
      self.visibilityConfiguration.tabGridButtonVisibility;
  return tabGridButton;
}

- (ToolbarButton*)toolsMenuButton {
  auto loadImageBlock = ^UIImage* {
    return DefaultSymbolWithPointSize(kMenuSymbol, kSymbolToolbarPointSize);
  };

  ToolbarButton* toolsMenuButton = nil;
  if (base::FeatureList::IsEnabled(kEnableStartupImprovements)) {
    toolsMenuButton =
        [[ToolbarButton alloc] initWithImageLoader:loadImageBlock];
  } else {
    toolsMenuButton = [[ToolbarButton alloc] initWithImage:loadImageBlock()];
  }

  if (IsVivaldiRunning()) {
    UIImage *menuImage = [UIImage imageNamed:vToolbarMenu];
    toolsMenuButton = [[ToolbarButton alloc] initWithImage:menuImage];
  } // End Vivaldi

  SetA11yLabelAndUiAutomationName(toolsMenuButton, IDS_IOS_TOOLBAR_SETTINGS,
                                  kToolbarToolsMenuButtonIdentifier);
  [self configureButton:toolsMenuButton width:kAdaptiveToolbarButtonWidth];
  [toolsMenuButton.heightAnchor
      constraintEqualToConstant:kAdaptiveToolbarButtonWidth]
      .active = YES;
  [toolsMenuButton addTarget:self.actionHandler
                      action:@selector(toolsMenuAction)
            forControlEvents:UIControlEventTouchUpInside];
  toolsMenuButton.visibilityMask =
      self.visibilityConfiguration.toolsMenuButtonVisibility;
  return toolsMenuButton;
}

- (ToolbarButton*)shareButton {
  auto loadImageBlock = ^UIImage* {
    return DefaultSymbolWithPointSize(kShareSymbol, kSymbolToolbarPointSize);
  };

  ToolbarButton* shareButton = nil;
  if (base::FeatureList::IsEnabled(kEnableStartupImprovements)) {
    shareButton = [[ToolbarButton alloc] initWithImageLoader:loadImageBlock];
  } else {
    shareButton = [[ToolbarButton alloc] initWithImage:loadImageBlock()];
  }

  [self configureButton:shareButton width:kAdaptiveToolbarButtonWidth];
  SetA11yLabelAndUiAutomationName(shareButton, IDS_IOS_TOOLS_MENU_SHARE,
                                  kToolbarShareButtonIdentifier);
  shareButton.titleLabel.text = @"Share";
  [shareButton addTarget:self.actionHandler
                  action:@selector(shareAction)
        forControlEvents:UIControlEventTouchUpInside];
  shareButton.visibilityMask =
      self.visibilityConfiguration.shareButtonVisibility;
  return shareButton;
}

- (ToolbarButton*)reloadButton {
  auto loadImageBlock = ^UIImage* {
    return CustomSymbolWithPointSize(kArrowClockWiseSymbol,
                                     kSymbolToolbarPointSize);
  };

  ToolbarButton* reloadButton = nil;
  if (base::FeatureList::IsEnabled(kEnableStartupImprovements)) {
    reloadButton = [[ToolbarButton alloc] initWithImageLoader:loadImageBlock];
  } else {
    reloadButton = [[ToolbarButton alloc] initWithImage:loadImageBlock()];
  }

  [self configureButton:reloadButton width:kAdaptiveToolbarButtonWidth];
  reloadButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_ACCNAME_RELOAD);
  [reloadButton addTarget:self.actionHandler
                   action:@selector(reloadAction)
         forControlEvents:UIControlEventTouchUpInside];
  reloadButton.visibilityMask =
      self.visibilityConfiguration.reloadButtonVisibility;
  return reloadButton;
}

- (ToolbarButton*)stopButton {
  auto loadImageBlock = ^UIImage* {
    return DefaultSymbolWithPointSize(kXMarkSymbol, kSymbolToolbarPointSize);
  };

  ToolbarButton* stopButton = nil;
  if (base::FeatureList::IsEnabled(kEnableStartupImprovements)) {
    stopButton = [[ToolbarButton alloc] initWithImageLoader:loadImageBlock];
  } else {
    stopButton = [[ToolbarButton alloc] initWithImage:loadImageBlock()];
  }

  [self configureButton:stopButton width:kAdaptiveToolbarButtonWidth];
  stopButton.accessibilityLabel = l10n_util::GetNSString(IDS_IOS_ACCNAME_STOP);
  [stopButton addTarget:self.actionHandler
                 action:@selector(stopAction)
       forControlEvents:UIControlEventTouchUpInside];
  stopButton.visibilityMask = self.visibilityConfiguration.stopButtonVisibility;
  return stopButton;
}

- (ToolbarButton*)openNewTabButton {
  UIColor* locationBarBackgroundColor =
      [self.toolbarConfiguration locationBarBackgroundColorWithVisibility:1];
  UIColor* buttonsTintColorIPHHighlighted =
      self.toolbarConfiguration.buttonsTintColorIPHHighlighted;
  UIColor* buttonsIPHHighlightColor =
      self.toolbarConfiguration.buttonsIPHHighlightColor;

  auto loadImageBlock = ^UIImage* {
    return SymbolWithPalette(
        CustomSymbolWithPointSize(kPlusCircleFillSymbol,
                                  kSymbolToolbarPointSize),
        @[ [UIColor colorNamed:kGrey600Color], locationBarBackgroundColor ]);
  };

  auto loadIPHHighlightedImageBlock = ^UIImage* {
    return SymbolWithPalette(CustomSymbolWithPointSize(kPlusCircleFillSymbol,
                                                       kSymbolToolbarPointSize),
                             @[
                               // The color of the 'plus'.
                               buttonsTintColorIPHHighlighted,
                               // The filling color of the circle.
                               buttonsIPHHighlightColor
                             ]);
  };

  ToolbarButton* newTabButton = nil;
  if (IsVivaldiRunning()) {
    UIImage* newTabButtonImage =
      [[UIImage imageNamed:@"toolbar_new_tab_page"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    newTabButton =
      [[ToolbarButton alloc]
          initWithImage:[newTabButtonImage
                        imageFlippedForRightToLeftLayoutDirection]];
  } else
  if (base::FeatureList::IsEnabled(kEnableStartupImprovements)) {
    newTabButton = [[ToolbarButton alloc]
              initWithImageLoader:loadImageBlock
        IPHHighlightedImageLoader:loadIPHHighlightedImageBlock];
  } else {
    newTabButton =
        [[ToolbarButton alloc] initWithImage:loadImageBlock()
                         IPHHighlightedImage:loadIPHHighlightedImageBlock()];
  }

  [newTabButton addTarget:self.actionHandler
                   action:@selector(newTabAction:)
         forControlEvents:UIControlEventTouchUpInside];
  BOOL isIncognito = self.style == ToolbarStyle::kIncognito;

  [self configureButton:newTabButton width:kAdaptiveToolbarButtonWidth];

  newTabButton.accessibilityLabel =
      l10n_util::GetNSString(isIncognito ? IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB
                                         : IDS_IOS_TOOLS_MENU_NEW_TAB);

  newTabButton.accessibilityIdentifier = kToolbarNewTabButtonIdentifier;

  newTabButton.visibilityMask =
      self.visibilityConfiguration.newTabButtonVisibility;
  return newTabButton;
}

- (UIButton*)cancelButton {
  UIButton* cancelButton = [UIButton buttonWithType:UIButtonTypeSystem];
  cancelButton.tintColor = [UIColor colorNamed:kBlueColor];
  [cancelButton setContentHuggingPriority:UILayoutPriorityRequired
                                  forAxis:UILayoutConstraintAxisHorizontal];
  [cancelButton
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];

  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
      0, kCancelButtonHorizontalInset, 0, kCancelButtonHorizontalInset);
  UIFont* font = [UIFont systemFontOfSize:kLocationBarFontSize];
  NSDictionary* attributes = @{NSFontAttributeName : font};
  NSMutableAttributedString* attributedString =
      [[NSMutableAttributedString alloc]
          initWithString:l10n_util::GetNSString(IDS_CANCEL)
              attributes:attributes];
  buttonConfiguration.attributedTitle = attributedString;
  cancelButton.configuration = buttonConfiguration;

  cancelButton.hidden = YES;
  [cancelButton addTarget:self.actionHandler
                   action:@selector(cancelOmniboxFocusAction)
         forControlEvents:UIControlEventTouchUpInside];
  cancelButton.accessibilityIdentifier =
      kToolbarCancelOmniboxEditButtonIdentifier;
  return cancelButton;
}

#pragma mark: - VIVALDI
- (ToolbarButton*)panelButton {
  UIImage* panelImage = [UIImage imageNamed:vToolbarPanelButtonIcon];
  ToolbarButton* panelButton =
    [[ToolbarButton alloc]
        initWithImage:[panelImage
                      imageFlippedForRightToLeftLayoutDirection]];
  [self configureButton:panelButton width:kAdaptiveToolbarButtonWidth];
  panelButton.accessibilityLabel = GetNSString(IDS_ACCNAME_PANEL);
  [panelButton addTarget:self.actionHandler
                 action:@selector(panelAction)
       forControlEvents:UIControlEventTouchUpInside];
  panelButton.visibilityMask =
      self.visibilityConfiguration.toolsMenuButtonVisibility;
  return panelButton;
}

// Vivaldi search button -> Visible only on new tab page.
- (ToolbarButton*)vivaldiSearchButton {
  UIImage* searchImage = [UIImage imageNamed:@"toolbar_search"];
  ToolbarButton* searchButton =
    [[ToolbarButton alloc]
        initWithImage:[searchImage
                      imageFlippedForRightToLeftLayoutDirection]];
  [self configureButton:searchButton width:kAdaptiveToolbarButtonWidth];
  searchButton.accessibilityLabel = GetNSString(IDS_ACCNAME_SEARCH);
  [searchButton addTarget:self.actionHandler
                 action:@selector(vivaldiSearchAction)
       forControlEvents:UIControlEventTouchUpInside];
  searchButton.visibilityMask =
      self.visibilityConfiguration.backButtonVisibility;
  return searchButton;
}

- (ToolbarButton*)shieldButton {
  UIImage* shieldImage = [UIImage imageNamed:vATBShieldNone];
  ToolbarButton* shieldButton =
    [[ToolbarButton alloc]
        initWithImage:[shieldImage
                      imageFlippedForRightToLeftLayoutDirection]];
  [self configureButton:shieldButton width:kAdaptiveToolbarButtonWidth];
  shieldButton.accessibilityLabel = GetNSString(IDS_ACCNAME_ATB);
  [shieldButton addTarget:self.actionHandler
                   action:@selector(showTrackerBlockerManager)
         forControlEvents:UIControlEventTouchUpInside];
  shieldButton.visibilityMask =
    self.visibilityConfiguration.toolsMenuButtonVisibility;
  return shieldButton;
}

// Visible only in iPhone portrait + Tab bar enabled + bottom omnibox enabled
// state.
- (ToolbarButton*)vivaldiMoreButton {
  UIImage* moreImage = [UIImage imageNamed:@"toolbar_more"];
  ToolbarButton* moreButton =
    [[ToolbarButton alloc]
        initWithImage:[moreImage
                      imageFlippedForRightToLeftLayoutDirection]];
  [self configureButton:moreButton width:kAdaptiveToolbarButtonWidth];
  moreButton.accessibilityLabel = GetNSString(IDS_ACCNAME_MORE);
  moreButton.visibilityMask =
    self.visibilityConfiguration.toolsMenuButtonVisibility;
  moreButton.showsMenuAsPrimaryAction = YES;
  return moreButton;
}

- (UIMenu*)overflowMenuWithTrackerBlocker:(BOOL)trackerBlockerEnabled
                           atbSettingType:(ATBSettingType)type
                 navigationForwardEnabled:(BOOL)navigationForwardEnabled
                navigationBackwordEnabled:(BOOL)navigationBackwordEnabled {

  NSMutableArray* overflowActions = [NSMutableArray array];

  // Set correct icon for tracker blocker based on settings before
  // creating the menu.
  if (trackerBlockerEnabled) {
    [overflowActions
        addObject:[self adAndTrackBlockerActionWithSettingType:type]];
  }

  // Add common actions
  [overflowActions addObject:self.tabSwitcherAction];
  [overflowActions addObject:self.panelAction];

  // Conditionally add navigation actions
  if (navigationBackwordEnabled) {
    [overflowActions addObject:self.navigationBackwordAction];
  }

  if (navigationForwardEnabled) {
    [overflowActions addObject:self.navigationForwardAction];
  }

  // Create and return the menu with the actions
  UIMenu* menu = [UIMenu menuWithTitle:@"" children:overflowActions];
  return menu;
}

#pragma mark - Private
- (UIAction*)adAndTrackBlockerActionWithSettingType:(ATBSettingType)type {
  NSString* shieldIcon = vATBShieldNone;
  switch (type) {
    case ATBSettingNoBlocking:
      shieldIcon = vATBShieldNone;
      break;
    case ATBSettingBlockTrackers:
      shieldIcon = vATBShieldTrackers;
      break;
    case ATBSettingBlockTrackersAndAds:
      shieldIcon = vATBShieldTrackesAndAds;
      break;
    default:
      break;
  }
  NSString* atbTitle =
      GetNSString(IDS_IOS_PREFS_VIVALDI_AD_AND_TRACKER_BLOCKER);
  UIImage* buttonIcon =
      [self toolbarButtonWithImage:shieldIcon];
  UIAction* atbAction =
      [UIAction actionWithTitle:atbTitle
                          image:buttonIcon
                     identifier:nil
                        handler:^(__kindof UIAction*_Nonnull
                                  action) {
        [self.actionHandler showTrackerBlockerManager];
      }];
  atbAction.accessibilityLabel = atbTitle;
  return atbAction;
}

- (UIAction*)panelAction {
  NSString* buttonTitle = GetNSString(IDS_IOS_TOOLBAR_VIVALDI_PANEL);
  UIImage* buttonIcon =
      [self toolbarButtonWithImage:vToolbarPanelButtonIcon];
  UIAction* panelAction =
      [UIAction actionWithTitle:buttonTitle
                          image:buttonIcon
                     identifier:nil
                        handler:^(__kindof UIAction*_Nonnull
                                  action) {
        [self.actionHandler panelAction];
      }];
  panelAction.accessibilityLabel = buttonTitle;
  return panelAction;
}

- (UIAction*)navigationForwardAction {
  NSString* buttonTitle = GetNSString(IDS_IOS_TOOLBAR_OVERFLOW_FORWARD);
  UIImage* buttonIcon =
      [self toolbarButtonWithImage:vToolbarForwardButtonIcon];
  UIAction* forwardAction =
      [UIAction actionWithTitle:buttonTitle
                          image:buttonIcon
                     identifier:nil
                        handler:^(__kindof UIAction*_Nonnull
                                  action) {
        [self.actionHandler forwardAction];
      }];
  forwardAction.accessibilityLabel = buttonTitle;
  return forwardAction;
}

- (UIAction*)navigationBackwordAction {
  NSString* buttonTitle = GetNSString(IDS_IOS_TOOLBAR_OVERFLOW_BACK);
  UIImage* buttonIcon =
      [self toolbarButtonWithImage:vToolbarBackButtonIcon];
  UIAction* backAction =
      [UIAction actionWithTitle:buttonTitle
                          image:buttonIcon
                     identifier:nil
                        handler:^(__kindof UIAction*_Nonnull
                                  action) {
        [self.actionHandler backAction];
      }];
  backAction.accessibilityLabel = buttonTitle;
  return backAction;
}

- (UIAction*)tabSwitcherAction {
  NSString* buttonTitle = GetNSString(IDS_IOS_TOOLBAR_OVERFLOW_TAB_SWITCHER);
  UIImage* buttonIcon =
      [self toolbarButtonWithImage:vToolbarTabSwitcherOveflowButtonIcon];
  UIAction* tabSwitcherAction =
      [UIAction actionWithTitle:buttonTitle
                          image:buttonIcon
                     identifier:nil
                        handler:^(__kindof UIAction*_Nonnull
                                  action) {
        [self.actionHandler tabGridTouchDown];
        [self.actionHandler tabGridTouchUp];
      }];
  tabSwitcherAction.accessibilityLabel = buttonTitle;
  return tabSwitcherAction;
}

// Apply default tint color to the image and return.
- (UIImage*)toolbarButtonWithImage:(NSString*)image {
  UIColor* tintColor = [UIColor colorNamed:vToolbarButtonColor];
  UIImage* buttonIcon =
      [[UIImage imageNamed:image]
          imageWithTintColor:tintColor
               renderingMode:UIImageRenderingModeAlwaysOriginal];
  return buttonIcon;
}

// End Vivaldi

#pragma mark - Helpers

// Sets the `button` width to `width` with a priority of
// UILayoutPriorityRequired - 1. If the priority is `UILayoutPriorityRequired`,
// there is a conflict when the buttons are hidden as the stack view is setting
// their width to 0. Setting the priority to UILayoutPriorityDefaultHigh doesn't
// work as they would have a lower priority than other elements.
- (void)configureButton:(ToolbarButton*)button width:(CGFloat)width {
  NSLayoutConstraint* constraint =
      [button.widthAnchor constraintEqualToConstant:width];
  constraint.priority = UILayoutPriorityRequired - 1;
  constraint.active = YES;
  button.toolbarConfiguration = self.toolbarConfiguration;
  button.exclusiveTouch = YES;
  button.pointerInteractionEnabled = YES;
  if (ios::provider::IsRaccoonEnabled()) {
    if (@available(iOS 17.0, *)) {
      button.hoverStyle = [UIHoverStyle
          styleWithShape:[UIShape rectShapeWithCornerRadius:width / 4]];
    }
  }
  button.pointerStyleProvider =
      ^UIPointerStyle*(UIButton* uiButton, UIPointerEffect* proposedEffect,
                       UIPointerShape* proposedShape) {
    // This gets rid of a thin border on a spotlighted bookmarks button.
    // This is applied to all toolbar buttons for consistency.
    CGRect rect = CGRectInset(uiButton.frame, 1, 1);
    UIPointerShape* shape = [UIPointerShape shapeWithRoundedRect:rect];
    return [UIPointerStyle styleWithEffect:proposedEffect shape:shape];
  };
}

@end
