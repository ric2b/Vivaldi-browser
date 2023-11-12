// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ntp/vivaldi_speed_dial_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - NOTIFICATION
// Notification name for start page layout change
NSString* vStartPageLayoutChangeDidChange =
    @"vStartPageLayoutChangeDidChange";

#pragma mark - SIZE AND PADDINGS
// Speed dial container top padding
const CGFloat vSpeedDialContainerTopPadding = 8.0;
// Speed Dial item corner radius
const CGFloat vSpeedDialItemCornerRadius = 8.0;
// Speed Dial item shadow offset
const CGSize vSpeedDialItemShadowOffset = CGSizeMake(0.0, 1.0);
// Speed Dial item shadow radius
const CGFloat vSpeedDialItemShadowRadius = 3.0;
// Speed Dial item shadow opacity
const CGFloat vSpeedDialItemShadowOpacity = 1.0;
// Speed Dial folder/Add new icon size for Large layout
// In order - Width, Height
const CGSize vSpeedDialFolderIconSizeRegular = CGSizeMake(44.f, 44.f);
// Speed Dial folder/Add new icon size for Medium layout
const CGSize vSpeedDialFolderIconSizeMedium = CGSizeMake(32.f, 32.f);
// Speed Dial folder/Add new icon size for Small layout
const CGSize vSpeedDialFolderIconSizeSmall = CGSizeMake(22.f, 22.f);
// Speed Dial folder/Add new icon size for List layout
const CGSize vSpeedDialFolderIconSizeList = CGSizeMake(28.f, 28.f);
// Speed Dial item favicon size for layout 'Regular'
const CGSize vSpeedDialItemFaviconSizeRegularLayout = CGSizeMake(16.0, 16.0);
// Speed Dial item favicon size for layout 'Small'
const CGSize vSpeedDialItemFaviconSizeSmallLayout = CGSizeMake(28.0, 28.0);
// Speed Dial item favicon size for layout 'Small' on tablet.
const CGSize vSpeedDialItemFaviconSizeSmallLayoutTablet = CGSizeMake(20.0, 20.0);
// Speed Dial item favicon size for layout 'List'
const CGSize vSpeedDialItemFaviconSizeListLayout = CGSizeMake(36.0, 36.0);
// Speed Dial favicon corner radius
const CGFloat vSpeedDialFaviconCornerRadius = 4.0;

// LAYOUT --> Common
// Speed dial container top padding
const CGFloat vSDContainerTopPadding = 24.f;
// Speed dial container bottom padding
const CGFloat vSDContainerBottomPadding = 24.f;
// Speed dial item height for layout style 'List'
const CGFloat vSDItemHeightListLayout = 76.f;

// LAYOUT --> IPhone
// Speed dial item size multiplier for iPhone 'Large' style
const CGFloat vSDWidthiPhoneLarge = 0.5;
// Speed dial item size multiplier for iPhone 'Large' style landscape
const CGFloat vSDWidthiPhoneLargeLand = 0.3333;
// Speed dial item size multiplier for iPhone 'Medium' style
const CGFloat vSDWidthiPhoneMedium = 0.3333;
// Speed dial item size multiplier for iPhone 'Medium' style landscape
const CGFloat vSDWidthiPhoneMediumLand = 0.2f;
// Speed dial item size multiplier for iPhone 'Small' style
const CGFloat vSDWidthiPhoneSmall = 0.25;
// Speed dial item size multiplier for iPhone 'Small' style landscape
const CGFloat vSDWidthiPhoneSmallLand = 0.1666;
// Speed dial item size multiplier for iPhone 'List' style
const CGFloat vSDWidthiPhoneList = 1.f;
// Speed dial item size multiplier for iPhone 'List' style landscape
const CGFloat vSDWidthiPhoneListLand = 0.5;

// Speed dial section padding for iPhone portrait
const CGFloat vSDSectionPaddingiPhonePortrait = 8.f;
// Speed dial section padding for iPhone landscape
const CGFloat vSDSectionPaddingiPhoneLandscape = 36.f;
// Speed dial item padding for iPhone
const CGFloat vSDPaddingiPhone = 8.f;

// LAYOUT --> IPad
// Speed dial item size multiplier for iPad 'Large' style
const CGFloat vSDWidthiPadLarge = 0.25;
// Speed dial item size multiplier for iPad 'Medium' style
const CGFloat vSDWidthiPadMedium = 0.1666;
// Speed dial item size multiplier for iPad 'Small' style
const CGFloat vSDWidthiPadSmall = 0.125;
// Speed dial item size multiplier for iPad 'List' style
const CGFloat vSDWidthiPadList = 0.5;

// Speed dial item padding for iPad
const CGFloat vSDPaddingiPad = 12.f;
// Speed dial section padding for iPad portrait
const CGFloat vSDSectionPaddingiPadPortrait = 64.f;
// Speed dial section padding for iPad landscape
const CGFloat vSDSectionPaddingiPadLandscape = 250.f;

#pragma mark - COLORS
// Color for the shadow of the speed dial item
NSString* vSpeedDialItemShadowColor =
    @"vivaldi_ntp_speed_dial_cell_shadow_color";
// Color for the new tab page speed dial container
NSString* vNTPSpeedDialContainerbackgroundColor =
    @"vivaldi_ntp_speed_dial_container_background_color";
// Color for the new tab page speed dial grid cells
NSString* vNTPSpeedDialCellBackgroundColor =
    @"vivaldi_ntp_speed_dial_cell_background_color";
// Color for the new tab page speed dial domain/website name text
NSString* vNTPSpeedDialDomainTextColor =
    @"vivaldi_ntp_speed_dial_domain_text_color";

#pragma mark - ICONS
// Image name for add new speed dial
NSString* vNTPAddNewSpeedDialIcon = @"vivaldi_ntp_add_new_speed_dial";
// Image name for add new speed dial folder
NSString* vNTPSpeedDialFolderIcon =
    @"vivaldi_ntp_add_new_speed_dial_folder";
// Image name for the icon on the speed dial empty view
NSString* vNTPSDEmptyViewIcon = @"vivaldi_ntp_sd_empty_view_icon";
// Image name for the icon for favicon fallback
NSString* vNTPSDFallbackFavicon = @"vivaldi_ntp_fallback_favicon";
// Image name for the icon for internal pages.
NSString* vNTPSDInternalPageFavicon = @"vivaldi_ntp_internal_favicon";
