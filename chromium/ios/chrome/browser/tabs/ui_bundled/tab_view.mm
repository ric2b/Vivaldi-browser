// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/ui_bundled/tab_view.h"

#import <MaterialComponents/MaterialActivityIndicator.h>

#import "base/ios/ios_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/elements/fade_truncating_label.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/image/image_util.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/highlight_button.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/resource/resource_bundle.h"
#import "ui/gfx/image/image.h"
#import "ui/gfx/ios/uikit_util.h"
#import "url/gurl.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
#import "ios/chrome/browser/ui/tab_strip/vivaldi_tab_strip_constants.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/ntp/vivaldi_ntp_constants.h"
#import "ios/ui/ntp/vivaldi_speed_dial_constants.h"
#import "ios/ui/settings/vivaldi_settings_constants.h"

using vivaldi::IsVivaldiRunning;
// End Vivaldi

namespace {

// The size of the xmark symbol image.
NSInteger kXmarkSymbolPointSize = 17;

// Tab close button insets.
const CGFloat kTabCloseTopInset = 1.0;
const CGFloat kTabCloseLeftInset = 0.0;
const CGFloat kTabCloseBottomInset = 0.0;
const CGFloat kTabCloseRightInset = 0.0;
const CGFloat kFaviconLeftInset = 28;
const CGFloat kFaviconVerticalOffset = 1.0;
const CGFloat kTabStripLineMargin = 2.5;
const CGFloat kTabStripLineHeight = 0.5;
const CGFloat kCloseButtonHorizontalShift = 22;
const CGFloat kTitleLeftMargin = 8.0;
const CGFloat kTitleRightMargin = 0.0;

const CGFloat kCloseButtonSize = 24.0;
const CGFloat kFaviconSize = 16.0;

const CGFloat kFontSize = 14.0;

// Returns a default favicon with `UIImageRenderingModeAlwaysTemplate`.
UIImage* DefaultFaviconImage() {

  if (IsVivaldiRunning())
    return [[UIImage imageNamed:vNTPSDFallbackFavicon]
              imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  // End Vivaldi

  return [[UIImage imageNamed:@"default_world_favicon"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
}

}  // namespace

@interface TabView () <UIPointerInteractionDelegate> {
  __weak id<TabViewDelegate> _delegate;

  // Close button for this tab.
  UIButton* _closeButton;

  // View that draws the tab title.
  FadeTruncatingLabel* _titleLabel;

  // Background image for this tab.
  UIImageView* _backgroundImageView;
  BOOL _incognitoStyle;

  // Set to YES when the layout constraints have been initialized.
  BOOL _layoutConstraintsInitialized;

  // Image view used to draw the favicon and spinner.
  UIImageView* _faviconView;

  // If `YES`, this view will adjust its appearance and draw as a collapsed tab.
  BOOL _collapsed;

  MDCActivityIndicator* _activityIndicator;

  // Adds hover interaction to background tabs.
  UIPointerInteraction* _pointerInteraction;

  // Vivaldi
  // Background view for tab view
  UIView *tabViewBackground;
  // End Vivaldi
}

// Vivaldi
// Constraint for the tab view background when omnibox is as the top.
@property(nonatomic, strong) NSLayoutConstraint*
    tabViewBackgroundTopConstraint;
// Constraint for the tab view background when omnibox is as the bottom.
@property(nonatomic, strong) NSLayoutConstraint*
    tabViewBackgroundBottomConstraint;
@property (nonatomic, strong) NSLayoutConstraint *closeButtonWidthConstraint;
// End Vivaldi

@end

@interface TabView (Private)

// Creates the close button, favicon button, and title.
- (void)createButtonsAndLabel;

// Returns the rect in which to draw the favicon.
- (CGRect)faviconRectForBounds:(CGRect)bounds;

// Returns the rect in which to draw the tab title.
- (CGRect)titleRectForBounds:(CGRect)bounds;

// Returns the frame rect for the close button.
- (CGRect)closeRectForBounds:(CGRect)bounds;

@end

@implementation TabView

- (id)initWithEmptyView:(BOOL)emptyView selected:(BOOL)selected {
  if ((self = [super initWithFrame:CGRectZero])) {
    [self setOpaque:NO];
    [self createCommonViews];

    if (!emptyView)
      [self createButtonsAndLabel];

    // -setSelected only calls -updateStyleForSelected if the selected state
    // changes.  `isSelected` defaults to NO, so if `selected` is also NO,
    // -updateStyleForSelected needs to be called explicitly.
    [self setSelected:selected];
    if (!selected) {
      [self updateStyleForSelected:selected];
    }

    [self addTarget:self
                  action:@selector(tabWasTapped)
        forControlEvents:UIControlEventTouchUpInside];

    if (@available(iOS 17, *)) {
      NSArray<UITrait>* traits = TraitCollectionSetForTraits(@[
        UITraitUserInterfaceIdiom.class, UITraitUserInterfaceStyle.class,
        UITraitDisplayGamut.class, UITraitAccessibilityContrast.class,
        UITraitUserInterfaceLevel.class
      ]);
      __weak __typeof(self) weakSelf = self;
      UITraitChangeHandler handler = ^(id<UITraitEnvironment> traitEnvironment,
                                       UITraitCollection* previousCollection) {
        [weakSelf updateStyleOnTraitChange:previousCollection];
      };
      [self registerForTraitChanges:traits withHandler:handler];
    }
  }
  return self;
}

- (void)setSelected:(BOOL)selected {
  if ([super isSelected] == selected) {
    return;
  }
  [super setSelected:selected];
  [self updateStyleForSelected:selected];
}

- (void)setCollapsed:(BOOL)collapsed {
  if (_collapsed != collapsed) {
    [_closeButton setHidden:collapsed];
  }

  _collapsed = collapsed;
}

- (void)setTitle:(NSString*)title {
  if ([_titleLabel.text isEqualToString:title])
    return;
  _titleLabel.text = title;
  [_closeButton setAccessibilityValue:title];
}

- (UIImage*)favicon {
  return [_faviconView image];
}

- (void)setFavicon:(UIImage*)favicon {

  if (IsVivaldiRunning() && !favicon) {
    favicon = [UIImage imageNamed:vNTPSDFallbackFavicon];
  } else {
  if (!favicon)
    favicon = DefaultFaviconImage();
  } // End Vivaldi

  [_faviconView setImage:favicon];
}

- (void)setIncognitoStyle:(BOOL)incognitoStyle {
  if (_incognitoStyle == incognitoStyle) {
    return;
  }
  _incognitoStyle = incognitoStyle;

  self.overrideUserInterfaceStyle = _incognitoStyle
                                        ? UIUserInterfaceStyleDark
                                        : UIUserInterfaceStyleUnspecified;
  return;
}

- (void)startProgressSpinner {
  [_activityIndicator startAnimating];
  [_activityIndicator setHidden:NO];
  [_faviconView setHidden:YES];

  if (IsVivaldiRunning()) {
    [_faviconView setImage:[UIImage imageNamed:vNTPSDFallbackFavicon]];
  } else {
  [_faviconView setImage:DefaultFaviconImage()];
  } // End Vivaldi

}

- (void)stopProgressSpinner {
  [_activityIndicator stopAnimating];
  [_activityIndicator setHidden:YES];
  [_faviconView setHidden:NO];
}

#pragma mark - UIView overrides

- (void)setFrame:(CGRect)frame {
  const CGRect previousFrame = [self frame];
  [super setFrame:frame];
  // We are checking for a zero frame before triggering constraints updates in
  // order to prevent computation of constraints that will never be used for the
  // final layout. We could also initialize with a dummy frame but first this is
  // inefficient and second it's non trivial to compute the minimum valid frame
  // in regard to tweakable constants.
  if (CGRectEqualToRect(CGRectZero, previousFrame) &&
      !_layoutConstraintsInitialized) {
    [self setNeedsUpdateConstraints];
  }
}

- (void)updateConstraints {
  [super updateConstraints];
  if (!_layoutConstraintsInitialized &&
      !CGRectEqualToRect(CGRectZero, self.frame)) {
    _layoutConstraintsInitialized = YES;
    [self addCommonConstraints];
    // Add buttons and labels constraints if needed.
    if (_closeButton) {
      [self addButtonsAndLabelConstraints];
    }
  }
}

- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent*)event {
  // Account for the trapezoidal shape of the tab.  Inset the tab bounds by
  // (y = -2.2x + 56), determined empirically from looking at the tab background
  // images.
  CGFloat inset = MAX(0.0, (point.y - 56) / -2.2);
  return CGRectContainsPoint(CGRectInset([self bounds], inset, 0), point);
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }
  [self updateStyleOnTraitChange:previousTraitCollection];
}
#endif

#pragma mark - Private

- (void)createCommonViews {
  _backgroundImageView = [[UIImageView alloc] init];
  [_backgroundImageView setTranslatesAutoresizingMaskIntoConstraints:NO];
  [self addSubview:_backgroundImageView];
}

- (void)addCommonConstraints {
  NSDictionary* commonViewsDictionary = @{
    @"backgroundImageView" : _backgroundImageView,
  };
  NSArray* commonConstraints = @[
    @"H:|-0-[backgroundImageView]-0-|",
    @"V:|-0-[backgroundImageView]-0-|",
  ];
  NSDictionary* commonMetrics = @{
    @"tabStripLineMargin" : @(kTabStripLineMargin),
    @"tabStripLineHeight" : @(kTabStripLineHeight)
  };
  ApplyVisualConstraintsWithMetrics(commonConstraints, commonViewsDictionary,
                                    commonMetrics);
}

- (void)createButtonsAndLabel {
  _closeButton = [HighlightButton buttonWithType:UIButtonTypeCustom];
  [_closeButton setTranslatesAutoresizingMaskIntoConstraints:NO];

  UIImage* closeButton =
      DefaultSymbolTemplateWithPointSize(kXMarkSymbol, kXmarkSymbolPointSize);

  if (IsVivaldiRunning())
    closeButton =
      [[UIImage imageNamed:@"tabstrip_close_tab"]
          imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  // End Vivaldi

  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.contentInsets =
      NSDirectionalEdgeInsetsMake(kTabCloseTopInset, kTabCloseLeftInset,
                                  kTabCloseBottomInset, kTabCloseRightInset);
  buttonConfiguration.image = closeButton;
  _closeButton.configuration = buttonConfiguration;

  [_closeButton setAccessibilityLabel:l10n_util::GetNSString(
                                          IDS_IOS_TOOLS_MENU_CLOSE_TAB)];
  [_closeButton addTarget:self
                   action:@selector(closeButtonPressed)
         forControlEvents:UIControlEventTouchUpInside];

  _closeButton.pointerInteractionEnabled = YES;

  [self addSubview:_closeButton];

  // Add fade truncating label.
  _titleLabel = [[FadeTruncatingLabel alloc] initWithFrame:CGRectZero];
  [_titleLabel setTranslatesAutoresizingMaskIntoConstraints:NO];
  [self addSubview:_titleLabel];

  CGRect faviconFrame = CGRectMake(0, 0, kFaviconSize, kFaviconSize);
  _faviconView = [[UIImageView alloc] initWithFrame:faviconFrame];
  [_faviconView setTranslatesAutoresizingMaskIntoConstraints:NO];
  [_faviconView setContentMode:UIViewContentModeScaleAspectFit];

  if (IsVivaldiRunning()) {
    [_faviconView setImage:
      [[UIImage imageNamed:vToolbarMenu]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate]];
  } else {
  [_faviconView setImage:DefaultFaviconImage()];
  } // End Vivaldi

  [_faviconView setAccessibilityIdentifier:@"Favicon"];
  [self addSubview:_faviconView];

  _activityIndicator =
      [[MDCActivityIndicator alloc] initWithFrame:faviconFrame];
  [_activityIndicator setTranslatesAutoresizingMaskIntoConstraints:NO];
  [_activityIndicator setCycleColors:@[ [UIColor colorNamed:kBlueColor] ]];
  [_activityIndicator setRadius:ui::AlignValueToUpperPixel(kFaviconSize / 2)];
  [self addSubview:_activityIndicator];
}

- (void)addButtonsAndLabelConstraints {
  // Constraints on the Top bar, snapshot view, and shadow view.
  NSDictionary* viewsDictionary = @{
    @"close" : _closeButton,
    @"title" : _titleLabel,
    @"favicon" : _faviconView,
  };
  NSArray* constraints = @[
    @"H:|-faviconLeftInset-[favicon(faviconSize)]",
    @"V:|-faviconVerticalOffset-[favicon]-0-|",
    @"H:[close(==closeButtonSize)]-closeButtonHorizontalShift-|",
    @"V:|-0-[close]-0-|",
    @"H:[favicon]-titleLeftMargin-[title]-titleRightMargin-[close]",
    @"V:[title(==titleHeight)]",
  ];

  NSDictionary* metrics = @{
    @"closeButtonSize" : @(kCloseButtonSize),
    @"closeButtonHorizontalShift" : @(kCloseButtonHorizontalShift),
    @"titleLeftMargin" : @(kTitleLeftMargin),
    @"titleRightMargin" : @(kTitleRightMargin),
    @"titleHeight" : @(kFaviconSize),
    @"faviconLeftInset" : @(kFaviconLeftInset),
    @"faviconVerticalOffset" : @(kFaviconVerticalOffset),
    @"faviconSize" : @(kFaviconSize),
  };
  ApplyVisualConstraintsWithMetrics(constraints, viewsDictionary, metrics);
  AddSameCenterXConstraint(self, _faviconView, _activityIndicator);
  AddSameCenterYConstraint(self, _faviconView, _activityIndicator);
  AddSameCenterYConstraint(self, _faviconView, _titleLabel);

  if (IsVivaldiRunning()) {
    // Define and add the width constraint for the close button
    self.closeButtonWidthConstraint =
        [NSLayoutConstraint constraintWithItem:_closeButton
                                     attribute:NSLayoutAttributeWidth
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:nil
                                     attribute:NSLayoutAttributeNotAnAttribute
                                    multiplier:1.0
                                      constant:kCloseButtonSize];
    [_closeButton addConstraint:self.closeButtonWidthConstraint];
  } // End Vivaldi
}

// Updates this tab's style based on the value of `selected` and the current
// incognito style.
- (void)updateStyleForSelected:(BOOL)selected {
  // Style the background image first.

  if (!IsVivaldiRunning()) {
  NSString* state = (selected ? @"foreground" : @"background");
  NSString* imageName = [NSString stringWithFormat:@"tabstrip_%@_tab", state];
  _backgroundImageView.image = [UIImage imageNamed:imageName];
  } // End Vivaldi

  if (selected) {
    if (_pointerInteraction)
      [self removeInteraction:_pointerInteraction];
  } else {
    if (!_pointerInteraction)
      _pointerInteraction =
          [[UIPointerInteraction alloc] initWithDelegate:self];
    [self addInteraction:_pointerInteraction];
  }

  // Note:(prio@vivaldi.com) - We have our own logic of tint color below.
  if (!IsVivaldiRunning()) {
  // Style the close button tint color.
  NSString* closeButtonColorName = selected ? kGrey600Color : kGrey500Color;
  _closeButton.tintColor = [UIColor colorNamed:closeButtonColorName];

  // Style the favicon tint color.
  NSString* faviconColorName = selected ? kGrey600Color : kGrey500Color;
  _faviconView.tintColor = [UIColor colorNamed:faviconColorName];

  // Style the title tint color.
  NSString* titleColorName = selected ? kTextPrimaryColor : kGrey600Color;
  _titleLabel.textColor = [UIColor colorNamed:titleColorName];
  } // End Vivaldi

  _titleLabel.font = [UIFont systemFontOfSize:kFontSize
                                       weight:UIFontWeightMedium];
  // It would make more sense to set active/inactive on tab_view itself, but
  // tab_view is not an an accessible element, and making it one would add
  // several complicated layers to UIA.  Instead, simply set active/inactive
  // here to be used by UIA.
  [_titleLabel setAccessibilityValue:(selected ? @"active" : @"inactive")];
}

// Bezier path for the border shape of the tab. While the shape of the tab is an
// illusion achieved with a background image, the actual border path is required
// for the hover pointer interaction.
- (UIBezierPath*)borderPath {
  CGFloat margin = 15;
  CGFloat width = self.frame.size.width - margin * 2;
  CGFloat height = self.frame.size.height;
  CGFloat cornerRadius = 12;
  UIBezierPath* path = [UIBezierPath bezierPath];
  [path moveToPoint:CGPointMake(margin - cornerRadius, height)];
  // Lower left arc.
  [path
      addArcWithCenter:CGPointMake(margin - cornerRadius, height - cornerRadius)
                radius:cornerRadius
            startAngle:M_PI / 2
              endAngle:0
             clockwise:NO];
  // Left vertical line.
  [path addLineToPoint:CGPointMake(margin, cornerRadius)];
  // Upper left arc.
  [path addArcWithCenter:CGPointMake(margin + cornerRadius, cornerRadius)
                  radius:cornerRadius
              startAngle:M_PI
                endAngle:3 * M_PI / 2
               clockwise:YES];
  // Top horizontal line.
  [path addLineToPoint:CGPointMake(width + margin - cornerRadius, 0)];
  // Upper right arc.
  [path
      addArcWithCenter:CGPointMake(width + margin - cornerRadius, cornerRadius)
                radius:cornerRadius
            startAngle:3 * M_PI / 2
              endAngle:0
             clockwise:YES];
  // Right vertical line.
  [path addLineToPoint:CGPointMake(width + margin, height - cornerRadius)];
  // Lower right arc.
  [path addArcWithCenter:CGPointMake(width + margin + cornerRadius,
                                     height - cornerRadius)
                  radius:cornerRadius
              startAngle:M_PI
                endAngle:M_PI / 2
               clockwise:NO];
  [path closePath];
  return path;
}

#pragma mark UIPointerInteractionDelegate

- (UIPointerRegion*)pointerInteraction:(UIPointerInteraction*)interaction
                      regionForRequest:(UIPointerRegionRequest*)request
                         defaultRegion:(UIPointerRegion*)defaultRegion {
  return defaultRegion;
}

- (UIPointerStyle*)pointerInteraction:(UIPointerInteraction*)interaction
                       styleForRegion:(UIPointerRegion*)region {
  // Hovering over this tab view and closing the tab simultaneously could result
  // in this tab view having been removed from the window at the beginning of
  // this method. If this tab view has already been removed from the view
  // hierarchy, a nil pointer style should be returned so that the pointer
  // remains with a default style. Attempting to construct a UITargetedPreview
  // with a tab view that has already been removed from the hierarchy will
  // result in a crash with an exception stating that the view has no window.
  if (!_backgroundImageView.window) {
    return nil;
  }

  UIPreviewParameters* parameters = [[UIPreviewParameters alloc] init];
  parameters.visiblePath = [self borderPath];

  // Use the background view for the preview so that z-order of overlapping tabs
  // is respected.
  UIPointerHoverEffect* effect = [UIPointerHoverEffect
      effectWithPreview:[[UITargetedPreview alloc]
                            initWithView:_backgroundImageView
                              parameters:parameters]];
  effect.prefersScaledContent = NO;
  effect.prefersShadow = NO;
  return [UIPointerStyle styleWithEffect:effect shape:nil];
}

// Invokes the `updateStyleForSelected` function if the view's color appearance
// was changed.
- (void)updateStyleOnTraitChange:(UITraitCollection*)previousTraitCollection {
  // As of iOS 13 Beta 4, resizable images are flaky for dark mode.
  // This triggers the styling again, where the image is resolved instead of
  // relying in the system's magic. Radar filled:
  // b/137942721.hasDifferentColorAppearanceComparedToTraitCollection
  if ([self.traitCollection
          hasDifferentColorAppearanceComparedToTraitCollection:
              previousTraitCollection]) {
    [self updateStyleForSelected:self.selected];
  }
}

#pragma mark - Touch events

- (void)closeButtonPressed {
  [_delegate tabViewCloseButtonPressed:self];
}

- (void)tabWasTapped {
  [_delegate tabViewTapped:self];
}

#pragma mark - Properties

- (UILabel*)titleLabel {
  return _titleLabel;
}

#pragma mark - VIVALDI
- (id)initWithEmptyView:(BOOL)emptyView
             isSelected:(BOOL)isSelected
       bottomOmniboxEnabled:(BOOL)bottomOmniboxEnabled
  showXButtonBackgroundTabs:(BOOL)showXButton
                 themeColor:(UIColor*)themeColor
                  tintColor:(UIColor*)tintColor {
  if ((self = [super initWithFrame:CGRectZero])) {
    [self setOpaque:NO];
    [self createCommonViews];

    if (!emptyView) {
      [self createTabViewBackrgound:isSelected
               bottomOmniboxEnabled:bottomOmniboxEnabled
                         themeColor:themeColor
                          tintColor:tintColor];
      [self createButtonsAndLabel];
    }

    // -setSelected only calls -updateStyleForSelected if the selected state
    // changes.  `isSelected` defaults to NO, so if `selected` is also NO,
    // -updateStyleForSelected needs to be called explicitly.
    [self setSelected:isSelected];
    if (!isSelected) {
      [self updateStyleForSelected:isSelected];
      [self updateTabViewStyleWithThemeColor:themeColor
                                   tintColor:tintColor
                                  isSelected:isSelected
                   showXButtonBackgroundTabs:showXButton];
    }

    [self addTarget:self
                  action:@selector(tabWasTapped)
        forControlEvents:UIControlEventTouchUpInside];
  }
  return self;
}

- (void)createTabViewBackrgound:(BOOL)selected
           bottomOmniboxEnabled:(BOOL)bottomOmniboxEnabled
                     themeColor:(UIColor*)themeColor
                      tintColor:(UIColor*)tintColor {
  tabViewBackground = [[UIView alloc] init];
  tabViewBackground.layer.cornerRadius = vTabViewBackgroundCornerRadius;
  tabViewBackground.layer.borderWidth = 1.0;
  tabViewBackground.layer.borderColor = [UIColor clearColor].CGColor;

  tabViewBackground.backgroundColor =
      [self tabViewBackgroundColor:selected
                        themeColor:themeColor
                         tintColor:tintColor];

  [self insertSubview:tabViewBackground atIndex:0];
  [self setupTabViewBackgroundConstraints];
  [self updateTabViewStyleWithBottomOmniboxEnabled:bottomOmniboxEnabled];
  [self setupTabViewTapGesture];
}

- (void)setupTabViewBackgroundConstraints {
  [tabViewBackground anchorTop:nil
                       leading:self.leadingAnchor
                        bottom:nil
                      trailing:self.trailingAnchor
                       padding:vTabViewBackgroundPaddingBottomAddressBar];

  self.tabViewBackgroundTopConstraint =
      [tabViewBackground.topAnchor constraintEqualToAnchor:self.topAnchor];
  self.tabViewBackgroundBottomConstraint =
      [tabViewBackground.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor];

  self.tabViewBackgroundTopConstraint.active = YES;
  self.tabViewBackgroundBottomConstraint.active = YES;
}

- (void)setupTabViewTapGesture {
  UITapGestureRecognizer *tap =
      [[UITapGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(tabWasTapped)];
  [tabViewBackground addGestureRecognizer:tap];
}

- (void)updateTabViewStyleWithBottomOmniboxEnabled:(BOOL)bottomOmniboxEnabled {
  UIRectCorner corners = bottomOmniboxEnabled ?
      (kCALayerMinXMaxYCorner | kCALayerMaxXMaxYCorner) :
      (kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner);
  tabViewBackground.layer.maskedCorners = corners;
  self.tabViewBackgroundTopConstraint.constant = bottomOmniboxEnabled ? -1 : 0;
  self.tabViewBackgroundBottomConstraint.constant =
      bottomOmniboxEnabled ? 0 : 1;
}

- (void)updateTabViewStyleWithThemeColor:(UIColor*)themeColor
                               tintColor:(UIColor*)tintColor
                              isSelected:(BOOL)isSelected
               showXButtonBackgroundTabs:(BOOL)showXButton {
  tabViewBackground.backgroundColor =
      [self tabViewBackgroundColor:isSelected
                        themeColor:themeColor
                         tintColor:tintColor];

  UIColor* defaultTintColor = isSelected ?
      [UIColor colorNamed:vTabViewSelectedTintColor] :
      [UIColor colorNamed:vTabViewNotSelectedTintColor];

  if (_incognitoStyle) {
    defaultTintColor = isSelected ?
        UIColor.whiteColor : [UIColor colorNamed:vTabViewNotSelectedTintColor];
  }
  _closeButton.tintColor = defaultTintColor;
  _faviconView.tintColor = defaultTintColor;
  _titleLabel.textColor = defaultTintColor;

  // Update close button visibility based on settings.
  [self updateTabViewWithXButtonVisibleBackgroundTabs:showXButton
                                           isSelected:isSelected];
}

- (void)updateTabViewWithXButtonVisibleBackgroundTabs:(BOOL)visible
                                           isSelected:(BOOL)isSelected {
  if (isSelected) {
    _closeButton.hidden = NO;
    self.closeButtonWidthConstraint.constant = kCloseButtonSize;
  } else {
    _closeButton.hidden = !visible;
    self.closeButtonWidthConstraint.constant = visible ? kCloseButtonSize : 0;
  }
}

// Returns background color for tab view based on selected state, current color
// scheme, and theme color.
- (UIColor*)tabViewBackgroundColor:(BOOL)isSelected
                        themeColor:(UIColor*)themeColor
                         tintColor:(UIColor*)tintColor {
  NSString *backgroundColorName;
  UIColor *defaultBackgroundColor;
  if (_incognitoStyle) {
    backgroundColorName = isSelected ?
        vPrivateModeToolbarBackgroundColor :
        vPrivateModeTabSelectedBackgroundColor;
    defaultBackgroundColor = [UIColor colorNamed:backgroundColorName];
  } else {
    if (isSelected) {
      defaultBackgroundColor =
          [UIColor colorNamed:vTabViewSelectedBackgroundColor];
    }
  }

  if (!isSelected && !_incognitoStyle) {
    BOOL isDarkTintColor = ![VivaldiGlobalHelpers
                                shouldUseDarkTextForColor:themeColor];
    UIColor* inactiveTabColor =
        (!isDarkTintColor || self.isDarkScheme) ?
            UIColor.blackColor : UIColor.whiteColor;
    return [inactiveTabColor colorWithAlphaComponent:
                isDarkTintColor ? vTabViewDarkTintOpacity :
                    vTabViewLightTintOpacity];
  } else {
    return defaultBackgroundColor;
  }
}

- (BOOL)isDarkScheme {
  return [VivaldiGlobalHelpers keyWindow].traitCollection.userInterfaceStyle
      == UIUserInterfaceStyleDark;
}

@end
