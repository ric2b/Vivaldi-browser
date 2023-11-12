// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_BLOCK_CLOUNT_VIEW_H_
#define IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_BLOCK_CLOUNT_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/ui/ad_tracker_blocker/vivaldi_atb_blocked_count_view_delegate.h"

// The view that contains the title of the blocked item and the count.
@interface VivaldiATBBlockCountView : UIView

// INITIALIZERS
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// DELEGATE
@property(nonatomic, weak) id<VivaldiATBBlockCountViewDelegate> delegate;

// SETTERS
- (void)setTitle:(NSString*)title
           value:(NSInteger)value;

@end

#endif  // IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_BLOCK_CLOUNT_VIEW_H_
