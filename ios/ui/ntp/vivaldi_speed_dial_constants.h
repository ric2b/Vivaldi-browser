// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NTP_VIVALDI_SPEED_DIAL_CONSTANTS_H_
#define IOS_UI_NTP_VIVALDI_SPEED_DIAL_CONSTANTS_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#pragma mark - NOTIFICATION
// Notification name for start page layout change
extern NSString* vStartPageLayoutChangeDidChange;

#pragma mark - SIZE AND PADDINGS
// Speed dial container top padding
extern const CGFloat vSpeedDialContainerTopPadding;
// Speed Dial item corner radius
extern const CGFloat vSpeedDialItemCornerRadius;
// Speed Dial item shadow offset
extern const CGSize vSpeedDialItemShadowOffset;
// Speed Dial item shadow radius
extern const CGFloat vSpeedDialItemShadowRadius;
// Speed Dial item shadow opacity
extern const CGFloat vSpeedDialItemShadowOpacity;
// Speed Dial folder/Add new icon size for Large layout
extern const CGSize vSpeedDialFolderIconSizeRegular;
// Speed Dial folder/Add new icon size for Medium layout
extern const CGSize vSpeedDialFolderIconSizeMedium;
// Speed Dial folder/Add new icon size for Small layout
extern const CGSize vSpeedDialFolderIconSizeSmall;
// Speed Dial folder/Add new icon size for List layout
extern const CGSize vSpeedDialFolderIconSizeList;
// Speed Dial item favicon size for layout 'Regular'
extern const CGSize vSpeedDialItemFaviconSizeRegularLayout;
// Speed Dial item favicon size for layout 'Small'
extern const CGSize vSpeedDialItemFaviconSizeSmallLayout;
// Speed Dial item favicon size for layout 'Small' on tablet.
extern const CGSize vSpeedDialItemFaviconSizeSmallLayoutTablet;
// Speed Dial item favicon size for layout 'List'
extern const CGSize vSpeedDialItemFaviconSizeListLayout;
// Speed Dial favicon corner radius
extern const CGFloat vSpeedDialFaviconCornerRadius;

// LAYOUT --> Common
// Speed dial container top padding
extern const CGFloat vSDContainerTopPadding;
// Speed dial container bottom padding
extern const CGFloat vSDContainerBottomPadding;
// Speed dial item height for layout style 'List'
extern const CGFloat vSDItemHeightListLayout;

// LAYOUT --> IPhone
// Speed dial item size multiplier for iPhone 'Large' style
extern const CGFloat vSDWidthiPhoneLarge;
// Speed dial item size multiplier for iPhone 'Large' style landscape
extern const CGFloat vSDWidthiPhoneLargeLand;
// Speed dial item size multiplier for iPhone 'Medium' style
extern const CGFloat vSDWidthiPhoneMedium;
// Speed dial item size multiplier for iPhone 'Medium' style landscape
extern const CGFloat vSDWidthiPhoneMediumLand;
// Speed dial item size multiplier for iPhone 'Small' style
extern const CGFloat vSDWidthiPhoneSmall;
// Speed dial item size multiplier for iPhone 'Small' style landscape
extern const CGFloat vSDWidthiPhoneSmallLand;
// Speed dial item size multiplier for iPhone 'List' style
extern const CGFloat vSDWidthiPhoneList;
// Speed dial item size multiplier for iPhone 'List' style landscape
extern const CGFloat vSDWidthiPhoneListLand;

// Speed dial section padding for iPhone portrait
extern const CGFloat vSDSectionPaddingiPhonePortrait;
// Speed dial section padding for iPhone landscape
extern const CGFloat vSDSectionPaddingiPhoneLandscape;
// Speed dial item padding for iPhone
extern const CGFloat vSDPaddingiPhone;

// LAYOUT --> IPad
// Speed dial item size multiplier for iPad 'Large' style
extern const CGFloat vSDWidthiPadLarge;
// Speed dial item size multiplier for iPad 'Medium' style
extern const CGFloat vSDWidthiPadMedium;
// Speed dial item size multiplier for iPad 'Small' style
extern const CGFloat vSDWidthiPadSmall;
// Speed dial item size multiplier for iPad 'List' style
extern const CGFloat vSDWidthiPadList;

// Speed dial item padding for iPad
extern const CGFloat vSDPaddingiPad;
// Speed dial section padding for iPad portrait
extern const CGFloat vSDSectionPaddingiPadPortrait;
// Speed dial section padding for iPad landscape
extern const CGFloat vSDSectionPaddingiPadLandscape;

#pragma mark - COLORS
// Color for the shadow of the speed dial item
extern NSString* vSpeedDialItemShadowColor;
// Color for the new tab page speed dial container
extern NSString* vNTPSpeedDialContainerbackgroundColor;
// Color for the new tab page speed dial grid cells
extern NSString* vNTPSpeedDialCellBackgroundColor;
// Color for the new tab page speed dial domain/website name text
extern NSString* vNTPSpeedDialDomainTextColor;

#pragma mark - ICONS
// Image name for add new speed dial
extern NSString* vNTPAddNewSpeedDialIcon;
// Image name for speed dial folder
extern NSString* vNTPSpeedDialFolderIcon;
// Image name for the icon on the speed dial empty view
extern NSString* vNTPSDEmptyViewIcon;
// Image name for the icon for favicon fallback
extern NSString* vNTPSDFallbackFavicon;
// Image name for the icon for internal pages.
extern NSString* vNTPSDInternalPageFavicon;

#endif  // IOS_UI_NTP_VIVALDI_SPEED_DIAL_CONSTANTS_H_
