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

+ (id)manager;
@end

#endif  // IOS_UI_NTP_VIVALDI_SPEED_DIAL_SHARED_STATE_H_
