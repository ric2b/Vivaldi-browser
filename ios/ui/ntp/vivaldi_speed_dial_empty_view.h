// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NTP_VIVALDI_SPEED_DIAL_EMPTY_VIEW_H_
#define IOS_UI_NTP_VIVALDI_SPEED_DIAL_EMPTY_VIEW_H_

#import <UIKit/UIKit.h>

// A view to show on the start page when there's no speed dial folder available.
@interface VivaldiSpeedDialEmptyView : UIView

// INITIALIZERS
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_UI_NTP_VIVALDI_SPEED_DIAL_EMPTY_VIEW_H_