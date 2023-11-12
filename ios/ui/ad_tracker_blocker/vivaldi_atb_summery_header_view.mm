// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ad_tracker_blocker/vivaldi_atb_summery_header_view.h"

#import "UIKit/UIKit.h"

#import "ios/ui/ad_tracker_blocker/vivaldi_atb_constants.h"
#import "ios/ui/helpers/vivaldi_colors_helper.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using l10n_util::GetNSString;

namespace {
const UIEdgeInsets statusContainerPadding = UIEdgeInsetsMake(4, 0, 4, 0);
const UIEdgeInsets statusLabelPadding = UIEdgeInsetsMake(6, 4, 6, 4);
} // End Namespace


@interface VivaldiATBSummeryHeaderView ()
@property (weak, nonatomic) UIView* statusContainer;
@property (weak, nonatomic) UIView* pulseAnimationView;
@property (weak, nonatomic) UILabel* statusLabel;
@end

@implementation VivaldiATBSummeryHeaderView

@synthesize statusContainer = _statusContainer;
@synthesize pulseAnimationView = _pulseAnimationView;
@synthesize statusLabel = _statusLabel;

#pragma mark - INITIALIZER
- (instancetype)init {
  if (self = [super initWithFrame:CGRectZero]) {
    [self setUpUI];
  }
  return self;
}

#pragma mark - PRIVATE
- (void)setUpUI {
  self.backgroundColor = UIColor.clearColor;

  // Container to hold the components
  UIView* containerView = [UIView new];
  containerView.backgroundColor = UIColor.clearColor;
  containerView.clipsToBounds = YES;

  [self addSubview:containerView];
  [containerView fillSuperview];

  // Container for the status label.
  UIView* statusContainer = [UIView new];
  _statusContainer = statusContainer;
  statusContainer.layer.cornerRadius = vBlockedCountBgCornerRadius;
  statusContainer.clipsToBounds = YES;

  [containerView addSubview:statusContainer];
  [statusContainer fillSuperviewWithPadding:statusContainerPadding];

  // Pulse animation view
  UIView *pulseAnimationView = [UIView new];
  _pulseAnimationView = pulseAnimationView;
  [statusContainer addSubview:pulseAnimationView];
  [pulseAnimationView fillSuperview];

  // Status label
  UILabel* statusLabel = [UILabel new];
  _statusLabel = statusLabel;
  statusLabel.adjustsFontForContentSizeCategory = YES;
  statusLabel.font =
    [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  statusLabel.numberOfLines = 0;
  statusLabel.textAlignment = NSTextAlignmentCenter;
  statusLabel.textColor = UIColor.whiteColor;

  [statusContainer addSubview:statusLabel];
  [statusLabel fillSuperviewWithPadding:statusLabelPadding];
}

#pragma mark - SETTERS
- (void)setStatusFromSetting:(ATBSettingType)settingType {
  switch (settingType) {
    case ATBSettingNoBlocking:
      _statusContainer.backgroundColor = UIColor.vSystemOrange;
      _statusLabel.text = GetNSString(IDS_VIVALDI_IOS_BLOCKING_NONE);
      break;
    case ATBSettingBlockTrackers:
      _statusContainer.backgroundColor = UIColor.vSystemGreen;
      _statusLabel.text = GetNSString(IDS_VIVALDI_IOS_BLOCKING_TRACKERS);
      break;
    case ATBSettingBlockTrackersAndAds:
      _statusContainer.backgroundColor = UIColor.vSystemGreen;
      _statusLabel.text = GetNSString(IDS_VIVALDI_IOS_BLOCKING_TRACKERS_ADS);
      break;
    default: break;
  }
}

- (void)setRulesGroupApplying:(BOOL)isApplying {
  _statusLabel.text =
      GetNSString(IDS_IOS_VIVALDI_AD_AND_TRACKER_BLOCKER_APPLYING_SETTINGS);

  // Pulse animation.
  UIColor *regularBlue = UIColor.vSystemPurple;
  UIColor *pulsingBlue = UIColor.vSystemBlue;

  // Create animation
  CABasicAnimation *pulseAnimation =
      [CABasicAnimation animationWithKeyPath:@"backgroundColor"];
  pulseAnimation.fromValue = (id)regularBlue.CGColor;
  pulseAnimation.toValue = (id)pulsingBlue.CGColor;
  pulseAnimation.duration = 1.0;
  pulseAnimation.autoreverses = YES;
  pulseAnimation.repeatCount = HUGE_VALF;

  // Add the animation to the layer
  [_pulseAnimationView.layer addAnimation:pulseAnimation
                                  forKey:@"pulseAnimation"];
}

@end
