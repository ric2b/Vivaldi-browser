// Copyright 2022 Vivaldi Technologies. All rights reserved.
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/vivaldi_tab_grid_page_control_constants.h"

#import <algorithm>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Height and width of the slider.
const CGFloat vSliderHeight = 32.0;
const CGFloat vSliderWidth = 46.0;

// Height and width of each segment.
const CGFloat vSegmentHeight = 40.0;
const CGFloat vSegmentWidth = 54.0;

// Points that the slider overhangs a segment on each side, or 0 if the slider
// is narrower than a segment.
const CGFloat vSliderOverhang =
    std::max((vSliderWidth - vSegmentWidth) / 2.0, 0.0);

// Width of the background -- three segments plus two separators.
const CGFloat vBackgroundWidth = 4 * vSegmentWidth;

// Overall height of the control -- the larger of the slider and segment
// heights.
const CGFloat vOverallHeight = std::max(vSliderHeight, vSegmentHeight);
// Overall width of the control -- the background width plus twice the slider
// overhang.
const CGFloat vOverallWidth = vBackgroundWidth + 3 * vSliderOverhang;

// Corner radius of the segment
const CGFloat vCornerRadius = 10.0;

// Corner radius of the slider
const CGFloat vSliderCornerRadius = 6.0;

// Color for the backrgound
NSString* const vBackgroundColor = @"page_control_background_color";

// Color for slider
UIColor* const vSliderColor = UIColor.whiteColor;

// Color for icons in unselected state
NSString* const vNotSelectedColor = @"page_control_icon_not_selected_color";

// Image names for the different icon state.
NSString* const kImagePageControlRegularSelected =
    @"page_control_regular_tabs_selected";
NSString* const kImagePageControlRegularNotSelected =
    @"page_control_regular_tabs";
NSString* const kImagePageControlIncognitoSelected =
    @"page_control_incognito_tabs_selected";
NSString* const kImagePageControlIncognitoNotSelected =
    @"page_control_incognito_tabs";
NSString* const kImagePageControlRemoteSelected =
    @"page_control_remote_tabs_selected";
NSString* const kImagePageControlRemoteNotSelected =
    @"page_control_remote_tabs";
// Icon for recently closed tab selected state
NSString* const kImagePageControlClosedSelected =
    @"page_control_closed_tabs_selected";
// Icon for recently closed tab not selected state
NSString* const kImagePageControlClosedNotSelected =
    @"page_control_closed_tabs";


// Color for the regular tab count label and icons.
const CGFloat kLegacySelectedColor = 0x3C4043;

// Slider shadow offset
const CGSize vSliderShadowOffset = CGSizeMake(0.0, 1.0);
// Slider shadow radius
const CGFloat vSliderShadowRadius = 3.0;
// Slider shadow opacity
const CGFloat vSliderShadowOpacity = 1.0;
// Slider shadow color
UIColor* const vSliderShadowColor =
  [[UIColor blackColor] colorWithAlphaComponent:0.14];
