// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ad_tracker_blocker/vivaldi_atb_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation VivaldiATBItem

@synthesize title = _title;
@synthesize subtitle = _subtitle;
@synthesize option = _option;

#pragma mark - INITIALIZER
- (instancetype) initWithTitle:(NSString*)title
                      subtitle:(NSString*)subtitle
                        option:(ATBSettingOption)option {
  self = [super init];
  if (self) {
    _title = title;
    _subtitle = subtitle;
    _option = option;
  }
  return self;
}

@end
