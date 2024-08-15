// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_ONBOARDING_VIVALDI_ONBOARDING_PROMO_VIEW_H_
#define IOS_UI_ONBOARDING_VIVALDI_ONBOARDING_PROMO_VIEW_H_

#import <UIKit/UIKit.h>

@class VivaldiOnboardingDataModel;

// View that holds a video player to play the logo animation and the title
// subtitle.
@interface VivaldiOnboardingPromoView: UIView

- (instancetype)initWithModel:(VivaldiOnboardingDataModel*)model
  NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@property (nonatomic, copy) void (^logoAnimationFinishedHandler)(BOOL);
@property (nonatomic, assign) BOOL logoAnimationFinished;

@end

#endif  // IOS_UI_ONBOARDING_VIVALDI_ONBOARDING_PROMO_VIEW_H_
