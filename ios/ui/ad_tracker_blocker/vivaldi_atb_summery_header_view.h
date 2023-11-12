// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_SUMMERY_HEADER_VIEW_H_
#define IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_SUMMERY_HEADER_VIEW_H_

#import <UIKit/UIKit.h>

@protocol VivaldiATBSummeryHeaderViewDelegate
- (void)didTapAds;
- (void)didTapTrackers;
@end

// A view to hold the summery of the tracker and blocker blocked for the visited
// site.
@interface VivaldiATBSummeryHeaderView : UIView

// INITIALIZERS
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// DELEGATE
@property(nonatomic, weak) id<VivaldiATBSummeryHeaderViewDelegate> delegate;

// SETTERS
- (void)setValueWithBlockedTrackers:(NSInteger)trackers
                                ads:(NSInteger)ads;

@end

#endif  // IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_SUMMERY_HEADER_VIEW_H_
