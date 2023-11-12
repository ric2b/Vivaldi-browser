// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/chrome/browser/ui/location_bar/location_bar_constants+vivaldi.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Leading padding for location bar steady view when vivaldi shield button is
// visible.
const CGFloat vLocationBarSteadyViewLeadingPadding = 8.f;
// Trailing padding for location bar steady view when vivaldi menu button is
// visible.
const CGFloat vLocationBarSteadyViewTrailingPadding = -8.f;
// Size for Vivaldi menu item on the location bar in iPhone.
const CGSize vLocationBarVivaldiMenuItemSize = CGSizeMake(28.f, 28.f);
// Size for Vivaldi Shield button. Both with and height are same.
const CGFloat vShieldButtonSize = 16.f;
// Leading padding for Vivaldi Shield button.
const CGFloat vShieldButtonLeadingPadding = 14.f;
// Leading padding for Badge view
const CGFloat vBadgeViewLeadingPadding = 8.f;
// Leading padding for Badge view when Vivaldi shield is hidden
const CGFloat vBadgeViewLeadingPaddingNoShield = 12.f;
// Top padding for Badge view when fullscreen is enabled
const CGFloat vBadgeViewTopPaddingFSEnabled = 4.f;
// Bottom padding for Badge view when fullscreen is enabled
const CGFloat vBadgeViewBottomPaddingFSEnabled = -4.f;
// Top padding for Badge view when fullscreen is disabled
const CGFloat vBadgeViewTopPaddingFSDisabled = 11.f;
// Bottom padding for Badge view when fullscreen is disabled
const CGFloat vBadgeViewBottomPaddingFSDisabled = -11.f;
// Size for Badge view when fullscreen is enabled
const CGSize vBadgeViewSizeFSEnabled = CGSizeMake(12.f, 12.f);
// Top padding for the location bar on new tab page desktop style tab.
const CGFloat vLocationBarTopPaddingDesktopTab = 6.f;
// Space between the location icon and the location label.
const CGFloat vLocationBarSteadyViewLocationImageToLabelSpacing = -6;
// Trailing space between the trailing button and the trailing edge of the
// location bar.
const CGFloat vLocationBarSteadyViewShareButtonTrailingSpacing = -8;
