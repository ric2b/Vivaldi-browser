// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_page_control.h"

#import <CoreGraphics/CoreGraphics.h>

#import <algorithm>

#import "base/check_op.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/raccoon/raccoon_api.h"
#import "ui/base/l10n/l10n_util.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/vivaldi_tab_grid_page_control_constants.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

using vivaldi::IsVivaldiRunning;
// End Vivaldi

UIControlEvents TabGridPageChangeByTapEvent = 1 << 24;
UIControlEvents TabGridPageChangeByDragEvent = 1 << 25;

// Structure of this control:
//
// The page control is similar to a UISegmentedControl in appearance, but not in
// function. This control doesn't have segments that highlight; instead there
// is a white "slider" that moves across the view onto whichever segment is
// active. Each segment has an image and (optionally) a label. When the slider
// is over a segment, the corresponding image and label are colored black-on-
// white and are slightly larger. This is implemented by having two versions of
// each image and label; the larger "selected" versions are positioned above the
// smaller ones in the view hierarchy but are masked out by the slider view, so
// they are only seen when the slider is over them.
//
// This control is built out of several views. From the (z-axis) bottom up, they
// are:
//
//  * The background view, a grey roundrect.
//  * The separators between the segment.
//  * The background image views.
//  * The numeric label on the regular tab icon.
//  * The hover views, which allow for pointer interactions.
//  * The "slider" view -- a white roundrect that's taller and wider than each
//    of the background segments. It clips its subview to its bounds, and it
//    adjusts its subview's frame so that it (the subview) remains fixed
//    relative to the background.
//     * The selected image view, which contains the selected images and labels
//       and is a subview of the slider.
//        * The selected images and label.

// Notes on layout:
// This control has an intrinsic size, and generally ignores frame changes. It's
// not expected that its bounds will ever change.
// Given that, it's generally simpler to used fixed (frame-based) layout for
// most of the content of this control. However, in order to accommodate RTL
// layout, three layout guides are used to define the position of the
// incognito, regular, and third panel sections. The layout frames of these
// guides are used to map points in the view to specific TabGridPage values.
// This means that the initial view layout for this control happens in two
// phases. -setupViews creates all of the subviews and the layout guides, but
// the positions of the images and labels is set in -layoutSubviews, after the
// constraints for the guides have been applied.

namespace {

// Height and width of the slider.
const CGFloat kSliderHeight = 40.0;
const CGFloat kSliderWidth = 65.0;

// Height and width of each segment.
const CGFloat kSegmentHeight = 44.0;
const CGFloat kSegmentWidth = 65.0;

// Margin between the slider and the leading/trailing segments.
const CGFloat kSliderMargin = 2.0;

// Vertical margin between the slider and the segment on each side.
const CGFloat kSliderVerticalMargin =
    std::max((kSegmentHeight - kSliderHeight) / 2.0, 0.0);

// Width and height of the separator bars between segments.
const CGFloat kSeparatorWidth = 1.0;
const CGFloat kSeparatorHeight = 22.0;

// Overall height of the control -- the larger of the slider and segment
// heights.
const CGFloat kOverallHeight = std::max(kSliderHeight, kSegmentHeight);
// Overall width -- three segments plus two separators plus two margins between
// leading/trailing segments and the slider.
const CGFloat kOverallWidth =
    3 * kSegmentWidth + 2 * kSeparatorWidth + 2 * kSliderMargin;

// Radius used to draw the background and the slider.
const CGFloat kSliderCornerRadius = 13.0;
const CGFloat kBackgroundCornerRadius = 15.0;

// Sizes for the labels and their selected counterparts.
const CGFloat kLabelSize = 20.0;
const CGFloat kSelectedLabelSize = 23.0;
const CGFloat kLabelSizeToFontSize = 0.6;

// Maximum duration of slider motion animation.
const NSTimeInterval kSliderMoveDuration = 0.2;

// Alpha for the background view.
const CGFloat kBackgroundAlpha = 0.15;
const CGFloat kScrolledToTopBackgroundAlpha = 0.25;

// The sizes of the symbol images.
const CGFloat kUnselectedSymbolSize = 22.;
const CGFloat kSelectedSymbolSize = 24.;

// The animation timing for the highlight background.
const CGFloat kHighlightAnimationDuration = 0.15;

// Returns the point that's at the center of `rect`.
CGPoint RectCenter(CGRect rect) {
  return CGPointMake(CGRectGetMidX(rect), CGRectGetMidY(rect));
}

// Returns an UIImageView for the given `symbol_name` and `selected` state.
UIImageView* ImageViewForSymbol(NSString* symbol_name,
                                bool selected,
                                bool is_system_symbol = false) {
  CGFloat size = selected ? kSelectedSymbolSize : kUnselectedSymbolSize;
  UIImage* image = is_system_symbol
                       ? DefaultSymbolTemplateWithPointSize(symbol_name, size)
                       : CustomSymbolTemplateWithPointSize(symbol_name, size);
  return [[UIImageView alloc] initWithImage:image];
}

// Returns the page that the third panel represents given the current
// experiments.
TabGridPage ThirdTabGridPage() {
  return IsTabGroupSyncEnabled() ? TabGridPageTabGroups : TabGridPageRemoteTabs;
}

}  // namespace

@interface TabGridPageControl () <UIGestureRecognizerDelegate,
                                  UIPointerInteractionDelegate>

// The grey background for all the segments.
@property(nonatomic, weak) UIView* background;

// Layout guides used to position segment-specific content.
@property(nonatomic, weak) UILayoutGuide* incognitoGuide;
@property(nonatomic, weak) UILayoutGuide* regularGuide;
@property(nonatomic, weak) UILayoutGuide* thirdPanelGuide;
// The separator between incognito and regular tabs.
@property(nonatomic, weak) UIView* firstSeparator;
// The separator between the regular and third panels.
@property(nonatomic, weak) UIView* secondSeparator;


// Vivaldi
@property(nonatomic, weak) UILayoutGuide* closedGuide;
// End Vivaldi

// The view for the slider.
@property(nonatomic, weak) UIView* sliderView;
// The view for the selected images and labels (a subview of `sliderView).
@property(nonatomic, weak) UIView* selectedImageView;
// The labels for the incognito and regular sections, in regular and selected
// variants.
@property(nonatomic, weak) UIView* incognitoNotSelectedIcon;
@property(nonatomic, weak) UIView* incognitoSelectedIcon;
@property(nonatomic, weak) UIView* regularNotSelectedIcon;
@property(nonatomic, weak) UIView* regularSelectedIcon;
@property(nonatomic, weak) UILabel* regularLabel;
@property(nonatomic, weak) UILabel* regularSelectedLabel;

#if defined(VIVALDI_BUILD)
// Declare as UIImageView since we want to update the icon for this panel
// after setting up based on sync state.
@property(nonatomic, weak) UIImageView* thirdPanelNotSelectedIcon;
@property(nonatomic, weak) UIImageView* thirdPanelSelectedIcon;
#else
@property(nonatomic, weak) UIView* thirdPanelNotSelectedIcon;
@property(nonatomic, weak) UIView* thirdPanelSelectedIcon;
#endif // End Vivaldi

// Vivaldi
@property(nonatomic, weak) UIView* closedNotSelectedIcon;
@property(nonatomic, weak) UIView* closedSelectedIcon;
// End Vivaldi

// Standard pointer interactions provided UIKit require views on which to attach
// interactions. These transparent views are the size of the whole segment and
// are visually below the slider. All touch events are properly received by the
// parent page control. And these views properly receive hover events by a
// pointer.
@property(nonatomic, weak) UIView* incognitoHoverView;
@property(nonatomic, weak) UIView* regularHoverView;
@property(nonatomic, weak) UIView* thirdPanelHoverView;

// Vivaldi
@property(nonatomic, weak) UIView* closedHoverView;
// End Vivaldi

// The center point for the slider corresponding to a `sliderPosition` of 0.
@property(nonatomic) CGFloat sliderOrigin;
// The (signed) x-coordinate distance the slider moves over. The slider's
// position is set by adding a fraction of this distance to `sliderOrigin`, so
// that when `sliderRange` is negative (in RTL layout), the slider will move in
// the negative-x direction from `sliderOrigin`, and otherwise it will move in
// the positive-x direction.
@property(nonatomic) CGFloat sliderRange;
// State properties to track the point and position (in the 0.0-1.0 range) of
// drags.
@property(nonatomic) CGPoint dragStart;
@property(nonatomic) CGFloat dragStartPosition;
@property(nonatomic) BOOL draggingSlider;
// Gesture recognizer used to handle taps. Owned by `self` as a UIView, so this
// property is just a weak pointer to refer to it in some touch logic.
@property(nonatomic, weak) UIGestureRecognizer* tapRecognizer;

// Whether the content below is scrolled to the edge or displayed behind.
@property(nonatomic, assign) BOOL scrolledToEdge;

// Vivaldi
// Whether sync is enabled currently which affects which icon we show for
// Remote tabs.
@property(nonatomic, assign) BOOL syncEnabled;
// End Vivaldi

@end

@implementation TabGridPageControl {
  UIAccessibilityElement* _incognitoAccessibilityElement;
  UIAccessibilityElement* _regularAccessibilityElement;
  UIAccessibilityElement* _thirdPanelAccessibilityElement;

  // Highlight view for the last control.
  UIView* _highlightView;

  // Vivaldi
  UIAccessibilityElement* _closedAccessibilityElement;
  // End Vivaldi

}

+ (instancetype)pageControl {
  return [[TabGridPageControl alloc] init];
}

- (instancetype)init {
  CGRect frame = CGRectMake(0, 0, kOverallWidth, kOverallHeight);

  if (IsVivaldiRunning()) {
    frame = CGRectMake(0, 0, vOverallWidth, vOverallHeight);
  } // End Vivaldi

  if ((self = [super initWithFrame:frame])) {
    // Default to the regular tab page as the selected page.
    _selectedPage = TabGridPageRegularTabs;

    _incognitoAccessibilityElement =
        [[UIAccessibilityElement alloc] initWithAccessibilityContainer:self];
    _incognitoAccessibilityElement.accessibilityTraits =
        UIAccessibilityTraitButton;
    _incognitoAccessibilityElement.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_TAB_GRID_INCOGNITO_TABS_TITLE);
    _incognitoAccessibilityElement.accessibilityIdentifier =
        kTabGridIncognitoTabsPageButtonIdentifier;

    _regularAccessibilityElement =
        [[UIAccessibilityElement alloc] initWithAccessibilityContainer:self];
    _regularAccessibilityElement.accessibilityTraits =
        UIAccessibilityTraitButton;
    _regularAccessibilityElement.accessibilityLabel =
        IsTabGroupInGridEnabled()
            ? l10n_util::GetNSString(
                  IDS_IOS_TAB_GRID_REGULAR_TABS_WITH_GROUPS_TITLE)
            : l10n_util::GetNSString(IDS_IOS_TAB_GRID_REGULAR_TABS_TITLE);
    _regularAccessibilityElement.accessibilityIdentifier =
        kTabGridRegularTabsPageButtonIdentifier;

    _thirdPanelAccessibilityElement =
        [[UIAccessibilityElement alloc] initWithAccessibilityContainer:self];
    _thirdPanelAccessibilityElement.accessibilityTraits =
        UIAccessibilityTraitButton;
    if (IsTabGroupSyncEnabled()) {
      _thirdPanelAccessibilityElement.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_GRID_TAB_GROUPS_TITLE);
      _thirdPanelAccessibilityElement.accessibilityIdentifier =
          kTabGridTabGroupsPageButtonIdentifier;
    } else {
      _thirdPanelAccessibilityElement.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_GRID_REMOTE_TABS_TITLE);
      _thirdPanelAccessibilityElement.accessibilityIdentifier =
          kTabGridRemoteTabsPageButtonIdentifier;
    }

    self.accessibilityElements = @[
      _incognitoAccessibilityElement, _regularAccessibilityElement,
      _thirdPanelAccessibilityElement
    ];

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(accessibilityBoldTextStatusDidChange)
               name:UIAccessibilityBoldTextStatusDidChangeNotification
             object:nil];

    [self setupViews];

    // Vivaldi
    [self setupRecentlyClosedTabAccessibilityElement];
    // End Vivaldi

  }
  return self;
}

- (void)setScrollViewScrolledToEdge:(BOOL)scrolledToEdge {
  if (_scrolledToEdge == scrolledToEdge) {
    return;
  }

  _scrolledToEdge = scrolledToEdge;

  if (IsVivaldiRunning()) {
    self.background.backgroundColor = [UIColor colorNamed:vBackgroundColor];
  } else {
  CGFloat backgroundAlpha =
      scrolledToEdge ? kScrolledToTopBackgroundAlpha : kBackgroundAlpha;
  self.background.backgroundColor = [UIColor colorWithWhite:1
                                                      alpha:backgroundAlpha];
  } // End Vivaldi

}

#pragma mark - Public Properties

- (void)setSelectedPage:(TabGridPage)selectedPage {
  [self setSelectedPage:selectedPage animated:NO];
}

- (void)setSliderPosition:(CGFloat)sliderPosition {
  // Clamp `selectionOffset` to (0.0 - 1.0).
  sliderPosition = std::clamp<CGFloat>(sliderPosition, 0.0, 1.0);
  CGPoint center = self.sliderView.center;
  center.x = self.sliderOrigin + self.sliderRange * sliderPosition;
  self.sliderView.center = center;
  // Reposition the selected image view so that it's still centered in the
  // control itself.
  self.selectedImageView.center = [self convertPoint:RectCenter(self.bounds)
                                              toView:self.sliderView];
  _sliderPosition = sliderPosition;

  // `_selectedPage` should be kept in sync with the slider position.
  TabGridPage previousSelectedPage = _selectedPage;

  // The position ranges from 0.0 to 1.0 for four tabs
  if (IsVivaldiRunning()) {
    if (sliderPosition < 0.25)
      _selectedPage = TabGridPageIncognitoTabs;
    else if (sliderPosition < 0.50)
      _selectedPage = TabGridPageRegularTabs;
    else if (sliderPosition < 0.75)
      _selectedPage = TabGridPageRemoteTabs;
    else
      _selectedPage = TabGridPageClosedTabs;

    if (_selectedPage != previousSelectedPage)
      [self updateSelectedPageAccessibility];

    return;
  } // End Vivaldi

  if (sliderPosition < 0.25) {
    _selectedPage = TabGridPageIncognitoTabs;
  } else if (sliderPosition < 0.75) {
    _selectedPage = TabGridPageRegularTabs;
  } else {
    _selectedPage = ThirdTabGridPage();
  }

  // Hide/show the separator based on the slider position. Add a delta for the
  // comparison to avoid issues when the regular tabs are selected.
  const CGFloat kDelta = 0.001;
  self.firstSeparator.hidden = sliderPosition < 0.5 + kDelta;
  self.secondSeparator.hidden = sliderPosition > 0.5 - kDelta;

  if (_selectedPage != previousSelectedPage) {
    [self updateSelectedPageAccessibility];
  }
}

// Setters for the control's text values. These need to update three things:
// the text in both labels (the regular and the  "selected" versions that's
// visible when the slider is over a segment), and an ivar to store values that
// are set before the labels are created.
- (void)setTabCount:(NSUInteger)tabCount {
  _tabCount = tabCount;
  [self updateRegularLabels];
}

#pragma mark - Public methods

- (void)setSelectedPage:(TabGridPage)selectedPage animated:(BOOL)animated {
  CGFloat newPosition;
  switch (selectedPage) {
    case TabGridPageIncognitoTabs:
      newPosition = 0.0;
      break;
    case TabGridPageRegularTabs:
      newPosition = 0.5;
      break;
    case TabGridPageRemoteTabs:
    case TabGridPageTabGroups:
      newPosition = 1.0;
      break;

    // Vivaldi
    case TabGridPageClosedTabs:
      break;
    // End Vivaldi

  }

  // Vivaldi
  newPosition = [self getPositionFromPage:selectedPage];
  // End Vivaldi

  if (self.selectedPage == selectedPage && newPosition == self.sliderPosition) {
    return;
  }

  _selectedPage = selectedPage;
  [self updateSelectedPageAccessibility];
  if (animated) {
    // Scale duration to the distance the slider travels, but cap it at
    // the slider move duration. This means that for motion induced by
    // tapping on a section, the duration will be the same even if the slider
    // is moving across two segments.
    CGFloat offsetDelta = abs(newPosition - self.sliderPosition);
    NSTimeInterval duration = offsetDelta * kSliderMoveDuration;
    [UIView animateWithDuration:std::min(duration, kSliderMoveDuration)
                     animations:^{
                       self.sliderPosition = newPosition;
                     }];
  } else {
    self.sliderPosition = newPosition;
  }
}

- (void)highlightLastPageControl {
  UIView* highlightBackground = [[UIView alloc] init];
  highlightBackground.translatesAutoresizingMaskIntoConstraints = NO;
  highlightBackground.backgroundColor = [UIColor colorNamed:kBlueColor];
  highlightBackground.layer.cornerRadius = kSliderCornerRadius;

  [self insertSubview:highlightBackground aboveSubview:self.background];

  [NSLayoutConstraint activateConstraints:@[
    [highlightBackground.trailingAnchor
        constraintEqualToAnchor:self.thirdPanelGuide.trailingAnchor],
    [highlightBackground.topAnchor
        constraintEqualToAnchor:self.sliderView.topAnchor],
    [highlightBackground.bottomAnchor
        constraintEqualToAnchor:self.sliderView.bottomAnchor],
    [highlightBackground.leadingAnchor
        constraintEqualToAnchor:self.regularGuide.centerXAnchor],
  ]];

  highlightBackground.alpha = 0;
  [UIView animateWithDuration:kHighlightAnimationDuration
                   animations:^{
                     highlightBackground.alpha = 1;
                   }];

  self.thirdPanelNotSelectedIcon.tintColor = UIColor.blackColor;

  _highlightView = highlightBackground;
}

- (void)resetLastPageControlHighlight {
  UIView* highlightView = _highlightView;
  [UIView animateWithDuration:kHighlightAnimationDuration
      animations:^{
        highlightView.alpha = 0;
      }
      completion:^(BOOL finished) {
        [highlightView removeFromSuperview];
      }];
  _highlightView = nil;
  self.thirdPanelNotSelectedIcon.tintColor =
      [UIColor colorNamed:kStaticGrey300Color];
}

- (CGRect)lastSegmentFrame {
  return [self.thirdPanelGuide.owningView
      convertRect:self.thirdPanelGuide.layoutFrame
           toView:nil];
}

#pragma mark - UIResponder

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [super touchesBegan:touches withEvent:event];
  DCHECK(!self.multipleTouchEnabled);
  DCHECK_EQ(1U, touches.count);
  if (self.draggingSlider) {
    return;
  }
  UITouch* touch = [touches anyObject];
  CGPoint locationInSlider = [touch locationInView:self.sliderView];
  if ([self.sliderView pointInside:locationInSlider withEvent:event]) {
    self.dragStart = [touch locationInView:self];
    self.dragStartPosition = self.sliderPosition;
    self.draggingSlider = YES;
  }
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [super touchesMoved:touches withEvent:event];
  if (!self.draggingSlider) {
    return;
  }
  DCHECK(!self.multipleTouchEnabled);
  DCHECK_EQ(1U, touches.count);
  UITouch* touch = [touches anyObject];
  CGPoint position = [touch locationInView:self];
  CGFloat deltaX = position.x - self.dragStart.x;
  // Convert to position change.
  CGFloat positionChange = deltaX / self.sliderRange;

  self.sliderPosition = self.dragStartPosition + positionChange;
  [self sendActionsForControlEvents:UIControlEventValueChanged];
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [super touchesEnded:touches withEvent:event];
  DCHECK(!self.multipleTouchEnabled);
  DCHECK_EQ(1U, touches.count);
  self.draggingSlider = NO;
  [self setSelectedPage:self.selectedPage animated:YES];
  [self sendActionsForControlEvents:TabGridPageChangeByDragEvent];
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [super touchesCancelled:touches withEvent:event];
  DCHECK(!self.multipleTouchEnabled);
  DCHECK_EQ(1U, touches.count);
  self.draggingSlider = NO;
  // The tap recognizer will cancel the touches it recognizes as the last step
  // of handling the gesture, so in that case, control events have already been
  // sent and don't need to be sent again here.
  if (self.tapRecognizer.state != UIGestureRecognizerStateEnded) {
    [self setSelectedPage:self.selectedPage animated:YES];
    [self sendActionsForControlEvents:TabGridPageChangeByDragEvent];
  }
}

#pragma mark - UIView

- (CGSize)intrinsicContentSize {
  if (IsVivaldiRunning()) {
    return CGSizeMake(vOverallWidth, vOverallHeight);
  } // End Vivaldi

  return CGSizeMake(kOverallWidth, kOverallHeight);
}

- (void)layoutSubviews {
  // The superclass call should be made first, so the constraint-based layout
  // guides can be set correctly.
  [super layoutSubviews];
  [self updateAccessibilityFrames];

  // Position the section images, labels and hover views, which depend on the
  // layout guides.
  self.incognitoNotSelectedIcon.center =
      [self centerOfSegment:TabGridPageIncognitoTabs];
  self.incognitoSelectedIcon.center =
      [self centerOfSegment:TabGridPageIncognitoTabs];

  self.regularNotSelectedIcon.center =
      [self centerOfSegment:TabGridPageRegularTabs];
  self.regularSelectedIcon.center =
      [self centerOfSegment:TabGridPageRegularTabs];
  self.regularLabel.center = [self centerOfSegment:TabGridPageRegularTabs];
  self.regularSelectedLabel.center =
      [self centerOfSegment:TabGridPageRegularTabs];

  self.thirdPanelNotSelectedIcon.center =
      [self centerOfSegment:ThirdTabGridPage()];
  self.thirdPanelSelectedIcon.center =
      [self centerOfSegment:ThirdTabGridPage()];

  // Vivaldi
  self.closedNotSelectedIcon.center =
      [self centerOfSegment:TabGridPageClosedTabs];
  self.closedSelectedIcon.center = [self centerOfSegment:TabGridPageClosedTabs];
  // End Vivaldi

  self.incognitoHoverView.center =
      [self centerOfSegment:TabGridPageIncognitoTabs];
  self.regularHoverView.center = [self centerOfSegment:TabGridPageRegularTabs];
  self.thirdPanelHoverView.center = [self centerOfSegment:ThirdTabGridPage()];

  // Vivaldi
  self.closedHoverView.center = [self centerOfSegment:TabGridPageClosedTabs];
  // End Vivaldi

  // Determine the slider origin and range; this is based on the layout guides
  // and can't be computed until they are determined.
  self.sliderOrigin = CGRectGetMidX(self.incognitoGuide.layoutFrame);
  self.sliderRange =
      CGRectGetMidX(self.thirdPanelGuide.layoutFrame) - self.sliderOrigin;

  // Set slider range from recently closed tab layout guide
  if (IsVivaldiRunning()) {
    self.sliderRange =
        CGRectGetMidX(self.closedGuide.layoutFrame) - self.sliderOrigin;
  } // End Vivaldi

  // Set the slider position using the new slider origin and range.
  self.sliderPosition = _sliderPosition;
}

#pragma mark - UIAccessibility (informal protocol)

- (BOOL)isAccessibilityElement {
  return NO;
}

#pragma mark - UIAccessibilityContainer Helpers

- (NSString*)accessibilityIdentifierForPage:(TabGridPage)page {
  switch (page) {
    case TabGridPageIncognitoTabs:
      return kTabGridIncognitoTabsPageButtonIdentifier;
    case TabGridPageRegularTabs:
      return kTabGridRegularTabsPageButtonIdentifier;
    case TabGridPageRemoteTabs:
      return kTabGridRemoteTabsPageButtonIdentifier;
    case TabGridPageTabGroups:
      return kTabGridTabGroupsPageButtonIdentifier;

    // Vivaldi
    case TabGridPageClosedTabs:
      return vTabGridRecentlyClosedTabsPageButtonIdentifier;
    // End Vivaldi
  }
}

- (void)updateSelectedPageAccessibility {
  NSString* selectedPageID =
      [self accessibilityIdentifierForPage:self.selectedPage];
  for (UIAccessibilityElement* element in self.accessibilityElements) {
    element.accessibilityTraits = UIAccessibilityTraitButton;
    if ([element.accessibilityIdentifier isEqualToString:selectedPageID]) {
      element.accessibilityTraits |= UIAccessibilityTraitSelected;
    }
  }
}

- (void)updateAccessibilityFrames {
  _incognitoAccessibilityElement.accessibilityFrameInContainerSpace =
      self.incognitoGuide.layoutFrame;
  _regularAccessibilityElement.accessibilityFrameInContainerSpace =
      self.regularGuide.layoutFrame;
  _thirdPanelAccessibilityElement.accessibilityFrameInContainerSpace =
      self.thirdPanelGuide.layoutFrame;

  // Vivaldi
  _closedAccessibilityElement.accessibilityFrameInContainerSpace =
      self.closedGuide.layoutFrame;
  // End Vivaldi

}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer*)gestureRecognizer {
  // Don't recognize taps if drag touches are being tracked.
  return !self.draggingSlider;
}

#pragma mark - Private

// Configures and Adds icon to the tab grid page control for the given tab.
- (void)addTabsIcon:(TabGridPage)tab {
  UIImageView* iconSelected;
  UIImageView* iconNotSelected;
  switch (tab) {
    case TabGridPageRegularTabs: {
      if (IsVivaldiRunning()) {
        iconSelected =
            ImageViewForSymbol(kImagePageControlRegular, true);
        iconNotSelected =
            ImageViewForSymbol(kImagePageControlRegular, false);
      } else {
      iconSelected = ImageViewForSymbol(kSquareNumberSymbol, /*selected=*/true);
      iconNotSelected =
          ImageViewForSymbol(kSquareNumberSymbol, /*selected=*/false);
      } // End Vivaldi

      self.regularSelectedIcon = iconSelected;
      self.regularNotSelectedIcon = iconNotSelected;
      break;
    }
    case TabGridPageIncognitoTabs: {
      if (IsVivaldiRunning()) {
        iconSelected =
            ImageViewForSymbol(kImagePageControlIncognito, true);
        iconNotSelected =
            ImageViewForSymbol(kImagePageControlIncognito, false);
      } else {
      iconSelected = ImageViewForSymbol(kIncognitoSymbol, /*selected=*/true);
      iconNotSelected =
          ImageViewForSymbol(kIncognitoSymbol, /*selected=*/false);
      } // End Vivaldi

      self.incognitoSelectedIcon = iconSelected;
      self.incognitoNotSelectedIcon = iconNotSelected;
      break;
    }
    case TabGridPageRemoteTabs: {

      if (IsVivaldiRunning()) {
        NSString* imageSymbol =
            self.syncEnabled ?
                kImagePageControlRemoteSynced : kImagePageControlRemote;
        iconSelected =
            ImageViewForSymbol(imageSymbol, true);
        iconNotSelected =
            ImageViewForSymbol(imageSymbol, false);
      } else {
      iconSelected = ImageViewForSymbol(kRecentTabsSymbol, /*selected=*/true);
      iconNotSelected =
          ImageViewForSymbol(kRecentTabsSymbol, /*selected=*/false);
      } // End Vivaldi
      self.thirdPanelSelectedIcon = iconSelected;
      self.thirdPanelNotSelectedIcon = iconNotSelected;
      break;
    }
    case TabGridPageTabGroups: {
      iconSelected = ImageViewForSymbol(kTabGroupsSymbol, /*selected=*/true,
                                        /*is_system_symbol=*/true);
      iconNotSelected = ImageViewForSymbol(kTabGroupsSymbol, /*selected=*/false,
                                           /*is_system_symbol=*/true);
      self.thirdPanelSelectedIcon = iconSelected;
      self.thirdPanelNotSelectedIcon = iconNotSelected;
      break;
    }

    // Vivaldi
    case TabGridPageClosedTabs: {
      iconSelected =
          ImageViewForSymbol(kImagePageControlClosed, true);
      iconNotSelected =
          ImageViewForSymbol(kImagePageControlClosed, false);
      self.closedSelectedIcon = iconSelected;
      self.closedNotSelectedIcon = iconNotSelected;
      break;
    }
    // End Vivaldi

  }

  iconNotSelected.tintColor = [UIColor colorNamed:kStaticGrey300Color];
  iconSelected.tintColor = UIColor.blackColor;

  if (IsVivaldiRunning()) {
    iconNotSelected.tintColor = [UIColor colorNamed: vNotSelectedColor];
  } // End Vivaldi

  [self insertSubview:iconNotSelected belowSubview:self.sliderView];
  [self.selectedImageView addSubview:iconSelected];
}

// Sets up all of the subviews for this control, as well as the layout guides
// used to position the section content.
- (void)setupViews {
  self.scrolledToEdge = YES;

  UIView* backgroundView = [[UIView alloc]
      initWithFrame:CGRectMake(0, 0, kOverallWidth, kSegmentHeight)];
  backgroundView.backgroundColor =
      [UIColor colorWithWhite:1 alpha:kScrolledToTopBackgroundAlpha];
  backgroundView.userInteractionEnabled = NO;
  backgroundView.layer.cornerRadius = kBackgroundCornerRadius;
  backgroundView.layer.masksToBounds = YES;
  [self addSubview:backgroundView];
  backgroundView.center =
      CGPointMake(kOverallWidth / 2.0, kOverallHeight / 2.0);
  self.background = backgroundView;

  if (IsVivaldiRunning()) {
    backgroundView.frame = CGRectMake(0, 0, vOverallWidth, vOverallHeight);
    backgroundView.backgroundColor =
        [UIColor colorNamed:vBackgroundColor];
    backgroundView.layer.cornerRadius = vCornerRadius;
    backgroundView.center =
      CGPointMake(vOverallWidth / 2.0, vOverallHeight / 2.0);
  } // End Vivaldi

  // Set up the layout guides for the segments.
  UILayoutGuide* incognitoGuide = [[UILayoutGuide alloc] init];
  [self addLayoutGuide:incognitoGuide];
  self.incognitoGuide = incognitoGuide;
  UILayoutGuide* regularGuide = [[UILayoutGuide alloc] init];
  [self addLayoutGuide:regularGuide];
  self.regularGuide = regularGuide;
  UILayoutGuide* thirdPanelGuide = [[UILayoutGuide alloc] init];
  [self addLayoutGuide:thirdPanelGuide];
  self.thirdPanelGuide = thirdPanelGuide;

  // Vivaldi
  UILayoutGuide* closedGuide = [[UILayoutGuide alloc] init];
  [self addLayoutGuide:closedGuide];
  self.closedGuide = closedGuide;
  // End Vivaldi

  // All of the guides are of the same height, and vertically centered in the
  // control.
  for (UILayoutGuide* guide in
       @[ incognitoGuide, regularGuide, thirdPanelGuide ]) {

    if (IsVivaldiRunning()) {
      [guide.heightAnchor
          constraintEqualToConstant:vOverallHeight].active = YES;
      [guide.widthAnchor constraintEqualToConstant:vSegmentWidth].active = YES;
    } else {
    [guide.heightAnchor constraintEqualToConstant:kOverallHeight].active = YES;
    // Guides are all the same width. The regular guide is centered in the
    // control, and the incognito and third panel guides are on the leading and
    // trailing sides of it, with separators in between.
    [guide.widthAnchor constraintEqualToConstant:kSegmentWidth].active = YES;
    } // End Vivaldi

    [guide.centerYAnchor constraintEqualToAnchor:self.centerYAnchor].active =
        YES;
  }

  if (IsVivaldiRunning()) {

    [closedGuide.heightAnchor
        constraintEqualToConstant:vOverallHeight].active = YES;
    [closedGuide.widthAnchor
        constraintEqualToConstant:vSegmentWidth].active = YES;
    [closedGuide.centerYAnchor
        constraintEqualToAnchor:self.centerYAnchor].active = YES;

    [NSLayoutConstraint activateConstraints:@[
      [incognitoGuide.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor],
      [regularGuide.leadingAnchor
          constraintEqualToAnchor:incognitoGuide.trailingAnchor],
      [thirdPanelGuide.leadingAnchor
          constraintEqualToAnchor:regularGuide.trailingAnchor],
      [closedGuide.leadingAnchor
          constraintEqualToAnchor:thirdPanelGuide.trailingAnchor],
    ]];
  } else {
  UIView* firstSeparator = [self newSeparator];
  [self addSubview:firstSeparator];
  self.firstSeparator = firstSeparator;
  UIView* secondSeparator = [self newSeparator];
  [self addSubview:secondSeparator];
  self.secondSeparator = secondSeparator;

  [NSLayoutConstraint activateConstraints:@[
    [incognitoGuide.trailingAnchor
        constraintEqualToAnchor:firstSeparator.leadingAnchor],
    [firstSeparator.trailingAnchor
        constraintEqualToAnchor:regularGuide.leadingAnchor],
    [regularGuide.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
    [regularGuide.trailingAnchor
        constraintEqualToAnchor:secondSeparator.leadingAnchor],
    [secondSeparator.trailingAnchor
        constraintEqualToAnchor:thirdPanelGuide.leadingAnchor],

    [firstSeparator.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [secondSeparator.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
  ]];
  } // End Vivaldi

  // Add the slider above the section images and labels.
  CGRect sliderFrame =
      CGRectMake(0, kSliderVerticalMargin, kSliderWidth, kSliderHeight);

  if (IsVivaldiRunning()) {
    sliderFrame = CGRectMake(0, 4, vSliderWidth, vSliderHeight);
  } // End Vivaldi

  UIView* slider = [[UIView alloc] initWithFrame:sliderFrame];

  slider.layer.cornerRadius = kSliderCornerRadius;

  if (IsVivaldiRunning()) {
    slider.layer.cornerRadius = vSliderCornerRadius;
  } // End Vivaldi

  slider.layer.masksToBounds = YES;
  if (IsVivaldiRunning()) {
    slider.backgroundColor = vSliderColor;
  } else {
  slider.backgroundColor = UIColor.whiteColor;
  } // End Vivaldi

  if (ios::provider::IsRaccoonEnabled()) {
    if (@available(iOS 17.0, *)) {
      slider.hoverStyle = [UIHoverStyle
          styleWithShape:
              [UIShape rectShapeWithCornerRadius:kBackgroundCornerRadius]];
    }
  }
  [self addSubview:slider];
  self.sliderView = slider;

  // Selected images and labels are added to the selected image view so they
  // will be clipped by the slider.
  CGRect selectedImageFrame = CGRectMake(0, 0, kOverallWidth, kOverallHeight);
  UIView* selectedImageView = [[UIView alloc] initWithFrame:selectedImageFrame];

  if (IsVivaldiRunning()) {
    selectedImageView.frame = CGRectMake(0, 0, vOverallWidth, vOverallHeight);
  } // End Vivaldi

  selectedImageView.userInteractionEnabled = NO;
  [self.sliderView addSubview:selectedImageView];
  self.selectedImageView = selectedImageView;

  [self addTabsIcon:TabGridPageRegularTabs];
  [self addTabsIcon:TabGridPageIncognitoTabs];
  [self addTabsIcon:ThirdTabGridPage()];

  // Vivaldi
  [self addTabsIcon:TabGridPageClosedTabs];
  // End Vivaldi

  UILabel* regularLabel = [self labelSelected:NO];
  [self insertSubview:regularLabel belowSubview:self.sliderView];
  self.regularLabel = regularLabel;
  UILabel* regularSelectedLabel = [self labelSelected:YES];
  [self.selectedImageView addSubview:regularSelectedLabel];
  self.regularSelectedLabel = regularSelectedLabel;

  self.incognitoHoverView = [self configureHoverView];
  self.regularHoverView = [self configureHoverView];
  self.thirdPanelHoverView = [self configureHoverView];

  // Vivaldi
  self.closedHoverView = [self configureHoverView];;
  // End Vivaldi

  // Vivaldi
  [self.closedHoverView
      addInteraction:[[UIPointerInteraction alloc] initWithDelegate:self]];
  // End Vivaldi

  [self.sliderView
      addInteraction:[[UIPointerInteraction alloc] initWithDelegate:self]];

  // Update the label text, in case these properties have been set before the
  // views were set up.
  self.tabCount = _tabCount;

  // Mark the control's layout as dirty so the the guides will be computed, then
  // force a layout now so it won't be triggered later (perhaps during an
  // animation).
  [self setNeedsLayout];
  [self layoutIfNeeded];

  // Add the gesture recognizer for taps on this control.
  UITapGestureRecognizer* tapRecognizer =
      [[UITapGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(handleTap:)];
  tapRecognizer.delegate = self;
  [self addGestureRecognizer:tapRecognizer];
  self.tapRecognizer = tapRecognizer;
}

// Updates the labels displaying the regular tab count.
- (void)updateRegularLabels {
  self.regularLabel.attributedText =
      TextForTabCount(_tabCount, kLabelSize * kLabelSizeToFontSize);
  self.regularSelectedLabel.attributedText =
      TextForTabCount(_tabCount, kSelectedLabelSize * kLabelSizeToFontSize);
}

// Creates a label for use in this control.
// Selected labels use a different size and are black.
- (UILabel*)labelSelected:(BOOL)selected {
  CGFloat size = selected ? kSelectedLabelSize : kLabelSize;

  if (IsVivaldiRunning()) {
    size = kLabelSize;
  } // End Vivaldi

  UILabel* label = [[UILabel alloc] initWithFrame:CGRectMake(0, 0, size, size)];
  label.backgroundColor = UIColor.clearColor;
  label.textAlignment = NSTextAlignmentCenter;
  if (IsVivaldiRunning()) {
      label.textColor = selected ?
          UIColorFromRGB(kLegacySelectedColor) :
          [UIColor colorNamed: vNotSelectedColor];
  } else {
  label.textColor =
      selected ? UIColor.blackColor : [UIColor colorNamed:kStaticGrey300Color];
  } // End Vivaldi
  return label;
}

// Handles tap gesture recognizer taps, setting a new selected page if the
// tap was outside the current page and sending the value changed actions.
- (void)handleTap:(UIGestureRecognizer*)tapRecognizer {
  CGPoint point = [tapRecognizer locationInView:self];
  // Determine which section the tap is in by looking at the layout frames of
  // each guide.
  TabGridPage page;
  if (CGRectContainsPoint(self.incognitoGuide.layoutFrame, point)) {
    page = TabGridPageIncognitoTabs;
  } else if (CGRectContainsPoint(self.thirdPanelGuide.layoutFrame, point)) {
    page = ThirdTabGridPage();
  } else {
    // bug: taps in the left- or rightmost `kSliderOverhang` points of the
    // control will fall through to this case.
    // TODO(crbug.com/41366258): Fix this.
    page = TabGridPageRegularTabs;
  }

  // Vivaldi
  page = [self getPageFromTouchPoint:point];
  // End Vivaldi

  if (page != self.selectedPage) {
    [self setSelectedPage:page animated:YES];
    [self sendActionsForControlEvents:TabGridPageChangeByTapEvent];
  }
}

// Returns the point at the center of `segment`.
- (CGPoint)centerOfSegment:(TabGridPage)segment {
  switch (segment) {
    case TabGridPageIncognitoTabs:
      return RectCenter(self.incognitoGuide.layoutFrame);
    case TabGridPageRegularTabs:
      return RectCenter(self.regularGuide.layoutFrame);
    case TabGridPageRemoteTabs:
    case TabGridPageTabGroups:
      return RectCenter(self.thirdPanelGuide.layoutFrame);

    // Vivaldi
    case TabGridPageClosedTabs:
      return RectCenter(self.closedGuide.layoutFrame);
    // End Vivaldi

  }
}

// Creates and returns a new separator, with constraints on its height/width.
- (UIView*)newSeparator {
  UIView* separator = [[UIView alloc] init];
  separator.backgroundColor = [UIColor colorNamed:kStaticGrey300Color];
  separator.layer.cornerRadius = kSeparatorWidth / 2.0;
  separator.translatesAutoresizingMaskIntoConstraints = NO;
  [separator.heightAnchor constraintEqualToConstant:kSeparatorHeight].active =
      YES;
  [separator.widthAnchor constraintEqualToConstant:kSeparatorWidth].active =
      YES;
  return separator;
}

// Callback for the notification that the user changed the bold status.
- (void)accessibilityBoldTextStatusDidChange {
  [self updateRegularLabels];
}

#pragma mark - Private's helpers

- (UIView*)configureHoverView {
  CGRect segmentRect = CGRectMake(0, 0, kSegmentWidth, kSegmentHeight);

  if (IsVivaldiRunning()) {
    segmentRect = CGRectMake(0, 0, vSegmentWidth, vOverallHeight);
  } // End Vivaldi

  UIView* hoverView = [[UIView alloc] initWithFrame:segmentRect];
  if (ios::provider::IsRaccoonEnabled()) {
    if (@available(iOS 17.0, *)) {
      hoverView.hoverStyle = [UIHoverStyle
          styleWithShape:
              [UIShape rectShapeWithCornerRadius:kBackgroundCornerRadius]];
    }
  }
  [self insertSubview:hoverView belowSubview:self.sliderView];
  [hoverView
      addInteraction:[[UIPointerInteraction alloc] initWithDelegate:self]];
  return hoverView;
}

#pragma mark UIPointerInteractionDelegate

- (UIPointerRegion*)pointerInteraction:(UIPointerInteraction*)interaction
                      regionForRequest:(UIPointerRegionRequest*)request
                         defaultRegion:(UIPointerRegion*)defaultRegion {
  return defaultRegion;
}

- (UIPointerStyle*)pointerInteraction:(UIPointerInteraction*)interaction
                       styleForRegion:(UIPointerRegion*)region {
  UIPointerHighlightEffect* effect = [UIPointerHighlightEffect
      effectWithPreview:[[UITargetedPreview alloc]
                            initWithView:interaction.view]];
  UIPointerShape* shape =
      [UIPointerShape shapeWithRoundedRect:interaction.view.frame
                              cornerRadius:kSliderCornerRadius];

  if (IsVivaldiRunning()) {
    shape = [UIPointerShape shapeWithRoundedRect:interaction.view.frame
                              cornerRadius:vCornerRadius];
  } // End Vivaldi

  return [UIPointerStyle styleWithEffect:effect shape:shape];
}

#pragma mark - Vivaldi

- (void)setIconForRemoteTabsWithSyncEnabled:(BOOL)syncEnabled {
  if (self.syncEnabled == syncEnabled)
    return;

  self.syncEnabled = syncEnabled;
  NSString* imageSymbol =
      syncEnabled ? kImagePageControlRemoteSynced : kImagePageControlRemote;

  self.thirdPanelSelectedIcon.image =
      CustomSymbolTemplateWithPointSize(imageSymbol, kUnselectedSymbolSize);
  self.thirdPanelNotSelectedIcon.image =
      CustomSymbolTemplateWithPointSize(imageSymbol, kUnselectedSymbolSize);

  [self setNeedsLayout];
}

- (void)setupRecentlyClosedTabAccessibilityElement {
  _closedAccessibilityElement =
      [[UIAccessibilityElement alloc] initWithAccessibilityContainer:self];
  _closedAccessibilityElement.accessibilityTraits =
      UIAccessibilityTraitButton;
  _closedAccessibilityElement.accessibilityLabel =
      l10n_util::GetNSString(IDS_VIVALDI_TAB_GRID_RECENTLY_CLOSED_TABS_TITLE);
  _closedAccessibilityElement.accessibilityIdentifier =
      vTabGridRecentlyClosedTabsPageButtonIdentifier;

  [self.accessibilityElements arrayByAddingObject:_closedAccessibilityElement];
}

// Returns page position from selected page.
// The position ranges from 0.0 to 1.0 for four pages/tabs
- (CGFloat)getPositionFromPage:(TabGridPage)selectedPage {
  switch (selectedPage) {
    case TabGridPageIncognitoTabs:
      return 0.0;
    case TabGridPageRegularTabs:
      return 0.33;
    case TabGridPageRemoteTabs:
      return 0.66;
    case TabGridPageTabGroups:
    case TabGridPageClosedTabs:
      return 1.0;
  }
}

// Return page based on the touch pointer from the associate layout guide
- (TabGridPage)getPageFromTouchPoint:(CGPoint)point {
  if (CGRectContainsPoint(self.incognitoGuide.layoutFrame, point)) {
    return TabGridPageIncognitoTabs;
  } else if (CGRectContainsPoint(self.thirdPanelGuide.layoutFrame, point)) {
    return TabGridPageRemoteTabs;
  } else if (CGRectContainsPoint(self.closedGuide.layoutFrame, point)) {
    return TabGridPageClosedTabs;
  } else {
    return TabGridPageRegularTabs;
  }
}
// End Vivaldi

@end
