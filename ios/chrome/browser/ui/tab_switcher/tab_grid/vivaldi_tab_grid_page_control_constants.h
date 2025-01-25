// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_PAGE_CONTROL_CONSTANTS_VIVALDI_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_PAGE_CONTROL_CONSTANTS_VIVALDI_H_

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

// Height and width of the slider.
extern const CGFloat vSliderHeight;
extern const CGFloat vSliderWidth;

// Height and width of each segment.
extern const CGFloat vSegmentHeight;
extern const CGFloat vSegmentWidth;

// Points that the slider overhangs a segment on each side, or 0 if the slider
// is narrower than a segment.
extern const CGFloat vSliderOverhang;

// Width of the background -- three segments plus two separators.
extern const CGFloat vBackgroundWidth;

// Overall height of the control -- the larger of the slider and segment
// heights.
extern const CGFloat vOverallHeight;
// Overall width of the control -- the background width plus twice the slider
// overhang.
extern const CGFloat vOverallWidth;

// Radius used to draw the background and the slider.
extern const CGFloat vCornerRadius;
extern const CGFloat vSliderCornerRadius;

// Color for background
extern NSString* const vBackgroundColor;
// Color for slider
extern UIColor* const vSliderColor;

// Color for icons in unselected state
extern NSString* const vNotSelectedColor;

// Image names for the different icon state.
extern NSString* const kImagePageControlRegular;
extern NSString* const kImagePageControlIncognito;
extern NSString* const kImagePageControlRemote;
extern NSString* const kImagePageControlRemoteSynced;
extern NSString* const kImagePageControlClosed;

// Color for the regular tab count label and icons.
extern const CGFloat kLegacySelectedColor;

// Slider shadow offset
extern const CGSize vSliderShadowOffset;
// Slider shadow radius
extern const CGFloat vSliderShadowRadius;
// Slider shadow opacity
extern const CGFloat vSliderShadowOpacity;
// Slider shadow color
extern UIColor* const vSliderShadowColor;

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_PAGE_CONTROL_CONSTANTS_VIVALDI_H_
