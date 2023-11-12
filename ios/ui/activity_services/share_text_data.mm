// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/activity_services/share_text_data.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ShareTextData

- (instancetype)initWithText:(NSString*)text title:(NSString*)title {
  if (self = [super init]) {
    _text = text;
    _title = title;
  }
  return self;
}

@end
