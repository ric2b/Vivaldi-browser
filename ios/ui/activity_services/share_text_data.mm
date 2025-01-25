// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/activity_services/share_text_data.h"

@implementation ShareTextData

- (instancetype)initWithText:(NSString*)text title:(NSString*)title {
  self = [super init];
  if (self) {
    _text = text;
    _title = title;
  }
  return self;
}

@end
