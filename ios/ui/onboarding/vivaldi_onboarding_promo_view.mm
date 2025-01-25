// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/onboarding/vivaldi_onboarding_promo_view.h"

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>

#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/onboarding/vivaldi_onboarding_swift.h"

namespace {
NSString *const kOnboardingStartImageName = @"onboarding_start";
CGFloat const kPlayerViewSize = 144.0;
CGFloat const kLogoViewSize = 120.0;
UIEdgeInsets const kTextStackViewPadding = {32, 8, 0, 8};
CGFloat const kTextStackViewSpacing = 12.0;
CGFloat const kLogoPlayerViewYAnchorConstant = -120.0;
NSTimeInterval const kAnimationDuration = 0.5;
NSTimeInterval const kAnimationDurationPlayerView = 0.2;
NSTimeInterval const kAnimationDelay = 0.0;
}

@interface VivaldiOnboardingPromoView ()

@property (nonatomic,strong) AVPlayerLayer *playerLayer;
@property (nonatomic,strong) AVPlayer *player;
// This view holds the video player.
@property (nonatomic,strong) UIView *playerView;

// Logo view holds the Vivaldi logo that stays after the logo animation video
// is finished.
@property (nonatomic,weak) UIImageView *logoView;
@property (nonatomic,weak) UILabel *titleLabel;
@property (nonatomic,weak) UILabel *subtitleLabel;

// The model that holds the data for title and subtitle.
@property (nonatomic,strong) VivaldiOnboardingDataModel *dataModel;

// Y constraint to animate the logo from center when promo animation is finished
@property (nonatomic,strong) NSLayoutConstraint *logoPlayerViewYAnchor;

@end

@implementation VivaldiOnboardingPromoView

#pragma mark - Initializer
- (instancetype)initWithModel:(VivaldiOnboardingDataModel*)model {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _dataModel = model;
    self.backgroundColor = UIColor.clearColor;
    [self setUpUI];
  }
  return self;
}

#pragma mark - Lifecycle

- (void)layoutSubviews {
  [super layoutSubviews];
  [self updatePlayerLayerFrame];
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

#pragma mark - UI Configuration

- (void)setUpUI {
  [self configureView];
  [self configurePlayer];
  [self setupNotificationObserver];
}

- (void)configureView {
  // Player view
  self.playerView = [UIView new];
  [self addSubview:self.playerView];
  [self.playerView setViewSize:CGSizeMake(kPlayerViewSize, kPlayerViewSize)];
  [self.playerView centerXInSuperview];
  self.logoPlayerViewYAnchor =
      [self.playerView.centerYAnchor
          constraintEqualToAnchor:self.centerYAnchor];
  self.logoPlayerViewYAnchor.active = YES;

  // Logo view
  UIImage *logoImage = [UIImage imageNamed:kOnboardingStartImageName];
  UIImageView *logoView = [[UIImageView alloc] initWithImage:logoImage];
  _logoView = logoView;
  [self addSubview:logoView];

  [logoView setViewSize:CGSizeMake(kLogoViewSize, kLogoViewSize)];
  [logoView centerXInSuperview];

  [logoView.centerYAnchor
      constraintEqualToAnchor:self.playerView.centerYAnchor].active = YES;
  logoView.alpha = 0;
  [self sendSubviewToBack:logoView];

  // Title and subtitle
  UILabel *titleLabel = [UILabel new];
  _titleLabel = titleLabel;
  titleLabel.textColor = UIColor.labelColor;
  titleLabel.adjustsFontForContentSizeCategory = YES;
  UIFontDescriptor *boldFontDescriptor =
  [[UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleTitle1]
        fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  titleLabel.font = [UIFont fontWithDescriptor:boldFontDescriptor size:0.0];
  titleLabel.numberOfLines = 0;
  titleLabel.textAlignment = NSTextAlignmentCenter;
  titleLabel.alpha = 0;

  UILabel *subtitleLabel = [UILabel new];
  _subtitleLabel = subtitleLabel;
  subtitleLabel.textColor = UIColor.secondaryLabelColor;
  subtitleLabel.adjustsFontForContentSizeCategory = YES;
  subtitleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  subtitleLabel.numberOfLines = 0;
  subtitleLabel.textAlignment = NSTextAlignmentCenter;
  subtitleLabel.alpha = 0;

  // Stackview for title and subtitle
  UIStackView *textStackView =
      [[UIStackView alloc] initWithArrangedSubviews:@[self.titleLabel,
                                                      self.subtitleLabel]];
  textStackView.distribution = UIStackViewDistributionFill;
  textStackView.spacing = kTextStackViewSpacing;
  textStackView.axis = UILayoutConstraintAxisVertical;
  [self addSubview:textStackView];
  [textStackView anchorTop:self.logoView.bottomAnchor
                   leading:self.safeLeftAnchor
                    bottom:nil
                  trailing:self.safeRightAnchor
                   padding:kTextStackViewPadding];
  self.titleLabel.text = self.dataModel.title;
  self.subtitleLabel.text = self.dataModel.subtitle;
}

- (void)updatePlayerLayerFrame {
  self.playerLayer.frame = self.playerView.bounds;
}

#pragma mark - Player Configuration

- (void)configurePlayer {
  self.playerLayer = [AVPlayerLayer layer];
  self.playerLayer.videoGravity = AVLayerVideoGravityResizeAspectFill;
  [self.playerView.layer addSublayer:self.playerLayer];

  NSURL *fileUrl = [self videoFileURL];
  if (!fileUrl) return;

  AVAsset *asset = [AVAsset assetWithURL:fileUrl];
  AVPlayerItem *item = [AVPlayerItem playerItemWithAsset:asset];

  self.player = [AVQueuePlayer playerWithPlayerItem:item];
  ((AVQueuePlayer*)self.player).actionAtItemEnd = AVPlayerActionAtItemEndPause;
  self.player.muted = YES;
  self.playerLayer.player = self.player;

  [self.player play];
}

- (NSURL*)videoFileURL {
  NSString *fileName =
      self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark ?
        [VivaldiOnboardingVideoConstants logoAnimDark] :
        [VivaldiOnboardingVideoConstants logoAnimLight];

  NSURL *fileUrl =
      [[NSBundle mainBundle]
         URLForResource:fileName
          withExtension:[VivaldiOnboardingVideoConstants fileExtension]
           subdirectory:[VivaldiOnboardingVideoConstants bundlePath]];
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

#pragma mark - Animation Methods

- (void)animateAppearance {
  self.logoView.alpha = 1;
  [UIView animateWithDuration:kAnimationDurationPlayerView
                        delay:kAnimationDelay
                      options:UIViewAnimationCurveEaseOut
                   animations:^{
    self.playerView.alpha = 0;
    [self layoutIfNeeded];
  } completion:^(BOOL alphaFinished) {
    [self animateTextAppearance];
  }];
}

- (void)animateTextAppearance {
  self.logoAnimationFinished = YES;
  if (self.logoAnimationFinishedHandler) {
    self.logoAnimationFinishedHandler(self.logoAnimationFinished);
  }
  [UIView animateWithDuration:kAnimationDuration
                        delay:kAnimationDelay
                      options:UIViewAnimationCurveEaseOut
                   animations:^{
    self.logoPlayerViewYAnchor.constant = kLogoPlayerViewYAnchorConstant;
    self.titleLabel.alpha = 1;
    self.subtitleLabel.alpha = 1;
    [self layoutIfNeeded];
  } completion:nil];
}

#pragma mark - Notifications
- (void)setupNotificationObserver {
  [[NSNotificationCenter defaultCenter]
       addObserver:self
          selector:@selector(playerItemDidPlayToEndTime:)
              name:AVPlayerItemDidPlayToEndTimeNotification
            object:nil];
}

- (void)playerItemDidPlayToEndTime:(NSNotification *)notification {
  [self animateAppearance];
}

#pragma mark - Trait Collection

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  if (self.traitCollection.userInterfaceStyle !=
      previousTraitCollection.userInterfaceStyle) {
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
