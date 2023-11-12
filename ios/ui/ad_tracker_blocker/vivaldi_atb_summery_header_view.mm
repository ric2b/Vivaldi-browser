// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ad_tracker_blocker/vivaldi_atb_summery_header_view.h"

#import "UIKit/UIKit.h"

#import "ios/ui/ad_tracker_blocker/vivaldi_atb_blocked_count_view.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_constants.h"
#import "ios/ui/helpers/vivaldi_colors_helper.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"
#import "vivaldi/mobile_common/grit/vivaldi_mobile_common_native_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Spacing between two view on stack view
const CGFloat stackSpacing = 12.f;
} // End Namespace


@interface VivaldiATBSummeryHeaderView ()<VivaldiATBBlockCountViewDelegate>
// View to show the blocked trackers count.
@property (weak, nonatomic) VivaldiATBBlockCountView* trackerView;
// View to show the blocked ads count.
@property (weak, nonatomic) VivaldiATBBlockCountView* adsView;
@end

@implementation VivaldiATBSummeryHeaderView

@synthesize trackerView = _trackerView;
@synthesize adsView = _adsView;

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

  UIView* separator = [UIView new];
  separator.backgroundColor = UIColor.quaternaryLabelColor;
  [containerView addSubview:separator];
  [separator anchorTop:containerView.topAnchor
               leading:containerView.leadingAnchor
                bottom:nil
              trailing:containerView.trailingAnchor
                  size:CGSizeMake(0, 1.f)];

  VivaldiATBBlockCountView* trackerView = [VivaldiATBBlockCountView new];
  _trackerView = trackerView;
  trackerView.delegate = self;

  VivaldiATBBlockCountView* adsView = [VivaldiATBBlockCountView new];
  _adsView = adsView;
  adsView.delegate = self;

  UIStackView* stack = [[UIStackView alloc] initWithArrangedSubviews:@[
    trackerView, adsView
  ]];
  stack.distribution = UIStackViewDistributionFillEqually;
  stack.spacing = stackSpacing;
  stack.axis = UILayoutConstraintAxisHorizontal;

  [containerView addSubview:stack];
  [stack fillSuperview];
}

#pragma mark - SETTERS
- (void)setValueWithBlockedTrackers:(NSInteger)trackers
                                ads:(NSInteger)ads {
  // Tracker
  NSString* trackerTitleString = trackers > 0 ?
    l10n_util::GetNSString(IDS_IOS_TRACKERS_BLOCKED_TEXT) :
    l10n_util::GetNSString(IDS_IOS_TRACKER_BLOCKED_TEXT);

  [self.trackerView setTitle:trackerTitleString
                       value:trackers];

  // Ad
  NSString* adsTitleString = trackers > 0 ?
    l10n_util::GetNSString(IDS_IOS_ADS_BLOCKED_TEXT) :
    l10n_util::GetNSString(IDS_IOS_AD_BLOCKED_TEXT);

  [self.adsView setTitle:adsTitleString
                   value:ads];
}

#pragma mark VIVALDI ATB BLOCKED COUNT VIEW DELEGATE
- (void)didTapItem:(UIView*)sender {
  if (!self.delegate)
    return;

  if (sender == self.adsView)
      [self.delegate didTapAds];

  if (sender == self.trackerView)
      [self.delegate didTapTrackers];
}

@end
