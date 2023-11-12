// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NTP_VIVALDI_SPEED_DIAL_SHARED_STATE_H_
#define IOS_UI_NTP_VIVALDI_SPEED_DIAL_SHARED_STATE_H_

#import <foundation/Foundation.h>

// VivaldiSpeedDialSharedState is a singleton class to keep track of some
// properties that's needed to accessed and modified from more than one class.

@interface VivaldiSpeedDialSharedState : NSObject {
  NSInteger selectedIndex;
  BOOL isSpeedDialPositionModified;
}

// Current selected index of the ment item
@property (nonatomic,assign) NSInteger selectedIndex;

// Property to track whether the speed dial position is modifed by drag-dropping
// item. When this is done, the UI needed to be refreshed when a new tab page
// is opened.
@property (nonatomic,assign) BOOL isSpeedDialPositionModified;

+ (id)manager;
@end

#endif  // IOS_UI_NTP_VIVALDI_SPEED_DIAL_SHARED_STATE_H_
