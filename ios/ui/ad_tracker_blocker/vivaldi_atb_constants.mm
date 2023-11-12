// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ad_tracker_blocker/vivaldi_atb_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - SIZE AND PADDINGS
// Corner radius for tracker blocker count background
const CGFloat vBlockedCountBgCornerRadius = 6.f;
// Tableview footer height
const CGFloat tableFooterHeight = 64.f;
// Corner radius for the action button used on the footer.
const CGFloat actionButtonCornerRadius = 8.f;
// Padding for the action button used on the footer.
const UIEdgeInsets actionButtonPadding = UIEdgeInsetsMake(4.f, 20.f, 4.f, 20.f);
// Common container view padding for the cells
const UIEdgeInsets commonContainerPadding =
  UIEdgeInsetsMake(0.f, 16.f, 0.f, 16.f);
// Size for the bottom underline used to seperate one item from another
const CGSize separatorSize = CGSizeMake(0.f, 1.f);
// Spacing between two view on a vertical stack view
const CGFloat vStackSpacing = 4.f;

#pragma mark - ICONS
// Image name for tracker blocker shield
NSString* vATBShield = @"vivaldi_atb_shield";
// Image name for no blocking shield
NSString* vATBShieldNone = @"vivaldi_atb_shield_notrack";
// Image name for trackers blocking shield
NSString* vATBShieldTrackers = @"vivaldi_atb_shield_trackers";
// Image name for trackers and ads blocking shield
NSString* vATBShieldTrackesAndAds = @"vivaldi_atb_shield_trackers_and_ads";
// Image name for tracker blocker selection check
NSString* vATBSettingsSelectionCheck =
  @"vivaldi_atb_settings_selection_check";
