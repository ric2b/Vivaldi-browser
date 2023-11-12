// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_SETTING_TYPE_H_
#define IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_SETTING_TYPE_H_

#import <Foundation/Foundation.h>

// Enum for the ad and tracker blocker setting types.
typedef NS_ENUM(NSUInteger, ATBSettingType) {
  ATBSettingNoBlocking = 0,
  ATBSettingBlockTrackers = 1,
  ATBSettingBlockTrackersAndAds = 2,
  ATBSettingNone = 3
};

#endif  // IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_SETTING_TYPE_H_
