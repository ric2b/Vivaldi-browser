// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_ONBOARDING_VIVALDI_ONBOARDING_BACKGROUND_VIEW_H_
#define IOS_UI_ONBOARDING_VIVALDI_ONBOARDING_BACKGROUND_VIEW_H_

#import <UIKit/UIKit.h>

// View that holds a video player to play the aurora videos in the onboarding
// background.
@interface VivaldiOnboardingBackgroundView: UIView

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_UI_ONBOARDING_VIVALDI_ONBOARDING_BACKGROUND_VIEW_H_
