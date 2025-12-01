// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_CHROME_BROWSER_UI_LOCATION_BAR_CONSTANTS_VIVALDI_H_
#define IOS_CHROME_BROWSER_UI_LOCATION_BAR_CONSTANTS_VIVALDI_H_

#import "UIKit/UIKit.h"

#pragma mark - Sizes and paddings
// Space between the location icon and the location label.
extern const CGFloat vLocationBarSteadyViewLocationImageToLabelSpacing;
// Trailing space between the trailing button and the trailing edge of the
// location bar.
extern const CGFloat vLocationBarSteadyViewShareButtonTrailingSpacing;
// Size for site connection security status icon
extern const CGFloat vLocationBarSiteConnectionStatusIconSize;

#pragma mark - Icons
extern NSString* vLocationBarPageInfo;
extern NSString* vLocationBarReload;
extern NSString* vLocationBarStop;

#pragma mark - Colors
extern NSString* vLocationBarLightBGColor;
extern NSString* vLocationBarDarkBGColor;

extern const CGFloat vLocationBarSteadyViewPlaceholderOpacity;
extern const CGFloat vLocationBarSteadyViewFullAddressOpacity;
extern const CGFloat vLocationBarSteadyViewDomainOpacity;

#endif  // IOS_CHROME_BROWSER_UI_LOCATION_BAR_CONSTANTS_VIVALDI_H_
