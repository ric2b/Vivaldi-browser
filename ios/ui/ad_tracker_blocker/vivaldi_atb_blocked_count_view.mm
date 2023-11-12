// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ad_tracker_blocker/vivaldi_atb_blocked_count_view.h"

#import "UIKit/UIKit.h"

#import "ios/ui/ad_tracker_blocker/vivaldi_atb_constants.h"
#import "ios/ui/helpers/vivaldi_colors_helper.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Size for the value container
const CGSize valueContainerSize = CGSizeMake(62.f, 62.f);
// Padding for the title label
const UIEdgeInsets titleLabelPadding = UIEdgeInsetsMake(0.f, 12.f, 0.f, 0.f);
} // End Namespace

@interface VivaldiATBBlockCountView ()
// Title of the block item type, e.g tracker, ads.
@property (weak, nonatomic) UILabel* titleLabel;
// Label to render number of items blocked
@property (weak, nonatomic) UILabel* valueLabel;
@end

@implementation VivaldiATBBlockCountView

@synthesize titleLabel = _titleLabel;
@synthesize valueLabel = _valueLabel;


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

  // Container for the value label
  UIView* valueContainerView = [UIView new];
  valueContainerView.backgroundColor = UIColor.vSystemBlue;
  valueContainerView.layer.cornerRadius = vBlockedCountBgCornerRadius;
  valueContainerView.clipsToBounds = YES;

  [self addSubview:valueContainerView];
  [valueContainerView anchorTop:nil
                        leading:self.leadingAnchor
                         bottom:nil
                       trailing:nil
                           size:valueContainerSize];
  [valueContainerView centerYInSuperview];

  // Value label
  UILabel* valueLabel = [UILabel new];
  _valueLabel = valueLabel;
  valueLabel.textColor = UIColor.whiteColor;
  valueLabel.adjustsFontForContentSizeCategory = YES;
  valueLabel.font =
    [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
  valueLabel.numberOfLines = 0;
  valueLabel.textAlignment = NSTextAlignmentCenter;

  [valueContainerView addSubview:valueLabel];
  [valueLabel fillSuperview];

  // Title label
  UILabel* titleLabel = [UILabel new];
  _titleLabel = titleLabel;
  titleLabel.textColor = UIColor.labelColor;
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.font =
    [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  titleLabel.numberOfLines = 0;
  titleLabel.textAlignment = NSTextAlignmentLeft;

  [self addSubview:titleLabel];
  [titleLabel anchorTop:nil
                leading:valueContainerView.trailingAnchor
                 bottom:nil
               trailing:self.trailingAnchor
                padding:titleLabelPadding];
  [titleLabel centerYInSuperview];

  self.userInteractionEnabled = YES;
  UITapGestureRecognizer* tap =
    [[UITapGestureRecognizer alloc]
      initWithTarget:self action:@selector(didTapView)];
  [self addGestureRecognizer:tap];
}

#pragma mark - SETTERS
- (void)setTitle:(NSString*)title
           value:(NSInteger)value {
  self.titleLabel.text = title;
  self.valueLabel.text = [NSString stringWithFormat:@"%tu", value];
}

#pragma mark - PRIVATE
- (void)didTapView {
  if (self.delegate)
      [self.delegate didTapItem:self];
}

@end
