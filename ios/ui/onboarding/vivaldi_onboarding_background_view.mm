// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/onboarding/vivaldi_onboarding_background_view.h"

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>

#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/onboarding/vivaldi_onboarding_swift.h"

namespace {
CGFloat const kOverlayDarkOpacity = 0.3;
CGFloat const kOverlayLightOpacity = 0.6;
}

@interface VivaldiOnboardingBackgroundView ()

@property (nonatomic, strong) AVPlayerLayer *playerLayer;
@property (nonatomic, strong) AVPlayer *player;
@property (nonatomic, strong) AVPlayerLooper *playerLooper;

@property (nonatomic, weak) UIView* overlay;

@end

@implementation VivaldiOnboardingBackgroundView

#pragma mark - Initializer
- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    [self setUpUI];
  }
  return self;
}

#pragma mark - Lifecycle

- (void)layoutSubviews {
  [super layoutSubviews];
  [self updatePlayerLayerFrame];
}

- (void)setUpUI {
  [self configurePlayer];

  self.backgroundColor = UIColor.clearColor;
  UIView* overlay = [UIView new];
  _overlay = overlay;
  overlay.backgroundColor = [self overlayColor];

  [self addSubview:overlay];
  [overlay fillSuperview];
}

- (void)updatePlayerLayerFrame {
  self.playerLayer.frame = self.bounds;
}

- (UIColor*)overlayColor {
  UIColor* color =
      self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleLight ?
          [[UIColor whiteColor] colorWithAlphaComponent:kOverlayLightOpacity] :
          [[UIColor blackColor] colorWithAlphaComponent:kOverlayDarkOpacity];
  return color;
}

#pragma mark - Player Configuration

- (void)configurePlayer {
  self.playerLayer = [AVPlayerLayer layer];
  self.playerLayer.videoGravity = AVLayerVideoGravityResizeAspectFill;
  [self.layer addSublayer:self.playerLayer];

  NSURL *fileUrl = [self videoFileURL];
  if (!fileUrl) return;

  AVAsset *asset = [AVAsset assetWithURL:fileUrl];
  AVPlayerItem *item = [AVPlayerItem playerItemWithAsset:asset];

  AVQueuePlayer *queuePlayer = [AVQueuePlayer playerWithPlayerItem:item];
  self.player = queuePlayer;
  self.player.muted = YES;
  self.playerLayer.player = self.player;

  // Set up the player looper
  self.playerLooper =
      [AVPlayerLooper playerLooperWithPlayer:queuePlayer
                                templateItem:item];
  [self.player play];
}

- (NSURL*)videoFileURL {
  NSString *fileName = [VivaldiGlobalHelpers isDeviceTablet] ?
      [VivaldiOnboardingVideoConstants auroraLoopTablet] :
      [VivaldiOnboardingVideoConstants auroraLoop];

  NSURL *fileUrl =
      [[NSBundle mainBundle]
         URLForResource:fileName
          withExtension:[VivaldiOnboardingVideoConstants fileExtension]
           subdirectory:
             [VivaldiOnboardingVideoConstants bundlePath]];
  return fileUrl;
}

- (void)restartPlayer {
  if (!self.player) return;
  CMTime currentTime = self.player.currentTime;
  if (!CMTIME_IS_VALID(currentTime)) {
    return;
  }

  NSURL *fileUrl = [self videoFileURL];
  if (!fileUrl) return;

  AVPlayerItem *playerItem = [AVPlayerItem playerItemWithURL:fileUrl];
  [self.player replaceCurrentItemWithPlayerItem:playerItem];
  [self.player seekToTime:currentTime
          toleranceBefore:kCMTimeZero
           toleranceAfter:kCMTimeZero
        completionHandler:^(BOOL finished) {
    if (finished) {
      [self.player play];
    }
  }];
}

#pragma mark - Trait Collection

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  if (self.traitCollection.userInterfaceStyle !=
      previousTraitCollection.userInterfaceStyle) {
    self.overlay.backgroundColor = [self overlayColor];
    [self restartPlayer];
  }

  if (self.traitCollection.verticalSizeClass !=
      previousTraitCollection.verticalSizeClass ||
      self.traitCollection.horizontalSizeClass !=
      previousTraitCollection.horizontalSizeClass) {
    [self updatePlayerLayerFrame];
  }
}

@end
