// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "base/apple/bundle_locations.h"
#import "build/build_config.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"

#if !BUILDFLAG(IS_IOS_MACCATALYST)
#import "vivaldi/Lottie/Lottie-Swift.h"
#import "base/check.h"
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)

@interface VivaldiLottieAnimation : NSObject <LottieAnimation>

// Instantiates a LottieAnimationImpl with the given configuration.
//
// @param config The LottieAnimation configuration parameters to use.
// @return An instance of VivaldiLottieAnimation.
- (instancetype)initWithConfig:(LottieAnimationConfiguration*)config;
- (instancetype)init NS_UNAVAILABLE;

@end

@implementation VivaldiLottieAnimation {
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  CompatibleAnimationView* _lottieAnimation;
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
}

- (instancetype)initWithConfig:(LottieAnimationConfiguration*)config {
  self = [super init];
  if (self) {
#if !BUILDFLAG(IS_IOS_MACCATALYST)
    DCHECK(config);
    DCHECK(config.animationName);

CompatibleAnimation* compatibleAnimation = [[CompatibleAnimation alloc]
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

- (void)pause {
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  [_lottieAnimation pause];
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
}

- (void)stop {
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  [_lottieAnimation stop];
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
}

- (void)setColorValue:(UIColor*)color forKeypath:(NSString*)keypath {
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  [_lottieAnimation setColorValue:color
                       forKeypath:[[CompatibleAnimationKeypath alloc]
                                  initWithKeypath:keypath]];
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
}

- (void)setDictionaryTextProvider:
    (NSDictionary<NSString*, NSString*>*)dictionaryTextProvider {
  _lottieAnimation.compatibleDictionaryTextProvider =
      [[CompatibleDictionaryTextProvider alloc]
          initWithValues:dictionaryTextProvider];;
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

namespace ios {
namespace provider {

// Generate an instance of LottieAnimation.
id<LottieAnimation> GenerateLottieAnimation(
    LottieAnimationConfiguration* config) {
  return [[VivaldiLottieAnimation alloc] initWithConfig:config];
}

}  // namespace provider
}  // namespace ios
