// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ntp/vivaldi_speed_dial_shared_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation VivaldiSpeedDialSharedState

@synthesize selectedIndex;
@synthesize isSpeedDialPositionModified;

#pragma mark Singleton Methods
+ (id)manager {
  static VivaldiSpeedDialSharedState *manager = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    manager = [[self alloc] init];
  });
  return manager;
}

- (id)init {
  if (self = [super init]) {
    selectedIndex = 0;
    isSpeedDialPositionModified = NO;
  }
  return self;
}

@end
