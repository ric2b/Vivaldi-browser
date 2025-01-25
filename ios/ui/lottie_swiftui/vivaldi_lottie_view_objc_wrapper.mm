// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/lottie_swiftui/vivaldi_lottie_view_objc_wrapper.h"

#import "base/apple/bundle_locations.h"
#import "build/build_config.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"

#if !BUILDFLAG(IS_IOS_MACCATALYST)
#import "vivaldi/Lottie/Lottie-Swift.h"
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)

@implementation VivaldiLottieViewObjcWrapper {

#if !BUILDFLAG(IS_IOS_MACCATALYST)
  CompatibleAnimationView* _lottieAnimation;
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)

  LottieAnimationConfiguration* _config;
}

- (instancetype)initWithConfig:(LottieAnimationConfiguration*)config {
  self = [super init];
  if (self) {
    _config = config;
#if !BUILDFLAG(IS_IOS_MACCATALYST)
    CompatibleAnimation* compatibleAnimation =
        [[CompatibleAnimation alloc]
            initWithName:config.animationName
            subdirectory:config.subdirectory
                  bundle:config.bundle == nil ? base::apple::FrameworkBundle()
                            : config.bundle];

    _lottieAnimation = [[CompatibleAnimationView alloc]
                            initWithCompatibleAnimation:compatibleAnimation];
    _lottieAnimation.contentMode = UIViewContentModeScaleAspectFit;
    _lottieAnimation.loopAnimationCount = config.loopAnimationCount;
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
  }
  return self;
}

- (void)play {
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  [_lottieAnimation play];
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
}

- (void)playWithCompletion:(void (^)(BOOL finished))completion {
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  [_lottieAnimation playWithCompletion:^(BOOL animationFinished) {
      if (completion) {
          completion(animationFinished);
      }
  }];
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
}

- (void)stop {
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  [_lottieAnimation stop];
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
}

- (void)updateAnimationWithName:(NSString*)animationName {
  [self stop];

#if !BUILDFLAG(IS_IOS_MACCATALYST)
  // Update the animation name in _config
  _config.animationName = animationName;

  // Create a new CompatibleAnimation with the new animation name
  CompatibleAnimation* newCompatibleAnimation =
      [[CompatibleAnimation alloc]
          initWithName:animationName
          subdirectory:_config.subdirectory
                bundle:_config.bundle ?: base::apple::FrameworkBundle()];

  // Update the animation in _lottieAnimation
  _lottieAnimation.compatibleAnimation = newCompatibleAnimation;
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)

}

- (BOOL)isAnimationPlaying {
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  return _lottieAnimation.isAnimationPlaying;
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
  return NO;
}

- (UIView*)animationView {
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  return _lottieAnimation;
#else
  return [[UIView alloc] initWithFrame:CGRectZero];
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
}

@end
