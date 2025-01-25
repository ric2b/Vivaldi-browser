// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_LOTTIE_SIWFTUI_VIVALDI_LOTTIE_VIEW_OBJC_WRAPPER_H_
#define IOS_UI_LOTTIE_SIWFTUI_VIVALDI_LOTTIE_VIEW_OBJC_WRAPPER_H_

#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"

/// An Obj- wrapper class for Lottie view.
/// This is required to use Lottie on Swift or SwiftUI because
/// namespace ios::provider is not directly useable on Swift
/// This exposes all necessary API's and duplicates
/// vivaldi_lottie_animation.mm.
@interface VivaldiLottieViewObjcWrapper : NSObject

// Instantiates a LottieAnimationImpl with the given configuration.
//
// @param config The LottieAnimation configuration parameters to use.
// @return An instance of VivaldiLottieAnimation.
- (instancetype)initWithConfig:(LottieAnimationConfiguration*)config;
- (instancetype)init NS_UNAVAILABLE;

#pragma mark: Public methods

// Called to plays the lottie animation.
- (void)play;

// Called to plays the lottie animation with a completion block for
// finished state.
- (void)playWithCompletion:(void (^)(BOOL finished))completion;

// Called to stop the lottie animation.
- (void)stop;

// Update the animation with a different file.
- (void)updateAnimationWithName:(NSString*)animationName;

// If animation is playing currently.
- (BOOL)isAnimationPlaying;

// Returns the lottie animation view.
- (UIView*)animationView;

@end


#endif  // IOS_UI_LOTTIE_SIWFTUI_VIVALDI_LOTTIE_VIEW_OBJC_WRAPPER_H_
