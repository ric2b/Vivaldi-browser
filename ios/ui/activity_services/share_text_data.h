// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_VIVALDI_UI_ACTIVITY_SERVICES_SHARE_TEXT_DATA_H_
#define IOS_VIVALDI_UI_ACTIVITY_SERVICES_SHARE_TEXT_DATA_H_

#import <UIKit/UIKit.h>

// Data object used to represent an image that will be shared via the activity
// view.
@interface ShareTextData : NSObject

// Designated initializer.
- (instancetype)initWithText:(NSString*)text title:(NSString*)title;

// Image to be shared.
@property(nonatomic, readonly, copy) NSString* text;

// Title of the image to share.
@property(nonatomic, readonly) NSString* title;

@end

#endif  // IOS_VIVALDI_UI_ACTIVITY_SERVICES_SHARE_TEXT_DATA_H_
