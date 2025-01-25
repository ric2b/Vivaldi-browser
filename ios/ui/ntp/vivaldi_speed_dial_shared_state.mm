// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ntp/vivaldi_speed_dial_shared_state.h"

@implementation VivaldiSpeedDialSharedState

@synthesize selectedIndex;

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
  self = [super init];
  if (self) {
    selectedIndex = 0;
  }
  return self;
}

@end
