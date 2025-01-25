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
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tab_group_state.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/grit/ios_theme_resources.h"
#import "ios/public/provider/chrome/browser/raccoon/raccoon_api.h"
#import "ui/base/l10n/l10n_util.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
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

  ToolbarButton* backButton =
      [[ToolbarButton alloc] initWithImageLoader:loadImageBlock];

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

  ToolbarButton* forwardButton =
      [[ToolbarButton alloc] initWithImageLoader:loadImageBlock];

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
  auto imageBlock = ^UIImage*(ToolbarTabGroupState tabGroupState) {

    if (IsVivaldiRunning())
      return [UIImage imageNamed:vToolbarTabSwitcherButtonIcon];
    // End Vivaldi

    switch (tabGroupState) {
      case ToolbarTabGroupState::kNormal:
        return CustomSymbolWithPointSize(kSquareNumberSymbol,
                                         kSymbolToolbarPointSize);
      case ToolbarTabGroupState::kTabGroup:
        return DefaultSymbolWithPointSize(kSquareFilledOnSquareSymbol,
                                          kSymbolToolbarPointSize);
    }
  };

  ToolbarTabGridButton* tabGridButton = [[ToolbarTabGridButton alloc]
      initWithTabGroupStateImageLoader:imageBlock];

  [self configureButton:tabGridButton width:kAdaptiveToolbarButtonWidth];
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
  UIColor* locationBarBackgroundColor =
      [self.toolbarConfiguration locationBarBackgroundColorWithVisibility:1];

  auto loadIPHHighlightedImageBlock = ^UIImage* {
    return SymbolWithPalette(
        CustomSymbolWithPointSize(kEllipsisSquareFillSymbol,
                                  kSymbolToolbarPointSize),
        @[ [UIColor colorNamed:kGrey600Color], locationBarBackgroundColor ]);
  };
  ToolbarButton* toolsMenuButton =
      [[ToolbarButton alloc] initWithImageLoader:loadImageBlock
                       IPHHighlightedImageLoader:loadIPHHighlightedImageBlock];

  if (IsVivaldiRunning()) {
    auto iconImageBlock = ^UIImage* {
      return [UIImage imageNamed:vToolbarMenu];
    };
    toolsMenuButton =
        [[ToolbarButton alloc] initWithImageLoader:iconImageBlock];
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

  ToolbarButton* shareButton =
      [[ToolbarButton alloc] initWithImageLoader:loadImageBlock];

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

  ToolbarButton* reloadButton =
      [[ToolbarButton alloc] initWithImageLoader:loadImageBlock];

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

  ToolbarButton* stopButton =
      [[ToolbarButton alloc] initWithImageLoader:loadImageBlock];

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
                               buttonsIPHHighlightColor,
                             ]);
  };

  ToolbarButton* newTabButton = nil;
  if (IsVivaldiRunning()) {
    auto iconImageBlock = ^UIImage* {
      UIImage* newTabButtonImage =
          [[UIImage imageNamed:vToolbarNTPButtonIcon]
              imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      return [newTabButtonImage imageFlippedForRightToLeftLayoutDirection];
    };
    newTabButton =
        [[ToolbarButton alloc] initWithImageLoader:iconImageBlock];
  } else
  newTabButton =
      [[ToolbarButton alloc] initWithImageLoader:loadImageBlock
                       IPHHighlightedImageLoader:loadIPHHighlightedImageBlock];

  [newTabButton addTarget:self.actionHandler
                   action:@selector(newTabAction:)
         forControlEvents:UIControlEventTouchUpInside];

  [self configureButton:newTabButton width:kAdaptiveToolbarButtonWidth];

  newTabButton.accessibilityLabel = [self.toolbarConfiguration
      accessibilityLabelForOpenNewTabButtonInGroup:NO];

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
  auto iconImageBlock = ^UIImage* {
    UIImage* panelImage = [UIImage imageNamed:vToolbarPanelButtonIcon];
    return [panelImage imageFlippedForRightToLeftLayoutDirection];
  };
  ToolbarButton* panelButton =
    [[ToolbarButton alloc] initWithImageLoader:iconImageBlock];
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
  auto iconImageBlock = ^UIImage* {
    UIImage* searchImage = [UIImage imageNamed:vToolbarSearchButtonIcon];
    return [searchImage imageFlippedForRightToLeftLayoutDirection];
  };

  ToolbarButton* searchButton =
      [[ToolbarButton alloc] initWithImageLoader:iconImageBlock];
  [self configureButton:searchButton width:kAdaptiveToolbarButtonWidth];
  searchButton.accessibilityLabel = GetNSString(IDS_ACCNAME_SEARCH);
  [searchButton addTarget:self.actionHandler
                 action:@selector(vivaldiSearchAction)
       forControlEvents:UIControlEventTouchUpInside];
  searchButton.visibilityMask =
      self.visibilityConfiguration.toolsMenuButtonVisibility;
  return searchButton;
}

  // Vivaldi Homepage Buttton
- (ToolbarButton*)vivaldiHomeButton {
  auto iconImageBlock = ^UIImage* {
    UIImage* homeImage = [UIImage imageNamed:vToolbarHomeButtonIcon];
    return [homeImage imageFlippedForRightToLeftLayoutDirection];
  };

  ToolbarButton* homeButton =
    [[ToolbarButton alloc] initWithImageLoader:iconImageBlock];
  [self configureButton:homeButton width:kAdaptiveToolbarButtonWidth];
  [homeButton addTarget:self.actionHandler
                 action:@selector(vivaldiHomeAction)
       forControlEvents:UIControlEventTouchUpInside];
  homeButton.visibilityMask =
      self.visibilityConfiguration.toolsMenuButtonVisibility;
  return homeButton;
}

// Visible only in iPhone portrait + Tab bar enabled + bottom omnibox enabled
// state.
- (ToolbarButton*)vivaldiMoreButton {
  auto iconImageBlock = ^UIImage* {
    UIImage* moreImage = [UIImage imageNamed:vToolbarMoreButtonIcon];
    return [moreImage imageFlippedForRightToLeftLayoutDirection];
  };

  ToolbarButton* moreButton =
      [[ToolbarButton alloc] initWithImageLoader:iconImageBlock];
  [self configureButton:moreButton width:kAdaptiveToolbarButtonWidth];
  moreButton.accessibilityLabel = GetNSString(IDS_ACCNAME_MORE);
  moreButton.visibilityMask =
    self.visibilityConfiguration.toolsMenuButtonVisibility;
  moreButton.showsMenuAsPrimaryAction = YES;
  return moreButton;
}

- (UIMenu*)overflowMenuWithNavForwardEnabled:(BOOL)navigationForwardEnabled
                          navBackwordEnabled:(BOOL)navigationBackwordEnabled {

  NSMutableArray* overflowActions = [NSMutableArray array];

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
