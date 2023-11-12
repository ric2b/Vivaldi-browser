// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_CONSTANTS_H_
#define IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_CONSTANTS_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#pragma mark - SIZE AND PADDINGS
// Corner radius for tracker blocker count background
extern const CGFloat vBlockedCountBgCornerRadius;
// Tableview footer height
extern const CGFloat tableFooterHeight;
// Corner radius for the action button used on the footer.
extern const CGFloat actionButtonCornerRadius;
// Padding for the action button used on the footer.
extern const UIEdgeInsets actionButtonPadding;
// Common container view padding for the cells
extern const UIEdgeInsets commonContainerPadding;
// Size for the bottom underline used to seperate one item from another
extern const CGSize separatorSize;
// Spacing between two view on a vertical stack view
extern const CGFloat vStackSpacing;

#pragma mark - ICONS
// Image name for tracker blocker shield
extern NSString* vATBShield;
// Image name for no blocking shield
extern NSString* vATBShieldNone;
// Image name for trackers blocking shield
extern NSString* vATBShieldTrackers;
// Image name for trackers and ads blocking shield
extern NSString* vATBShieldTrackesAndAds;
// Image name for tracker blocker selection check
extern NSString* vATBSettingsSelectionCheck;

#endif  // IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_CONSTANTS_H_
