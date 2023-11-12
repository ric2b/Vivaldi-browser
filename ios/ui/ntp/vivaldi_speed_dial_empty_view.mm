// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ntp/vivaldi_speed_dial_empty_view.h"

#import "UIKit/UIKit.h"

#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/ntp/vivaldi_speed_dial_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
CGFloat const iconSize = 150.f;
} // End Namespace

@implementation VivaldiSpeedDialEmptyView

#pragma mark - INITIALIZER
- (instancetype)init {
  if (self = [super initWithFrame:CGRectZero]) {
    self.backgroundColor =
      [UIColor colorNamed:vNTPSpeedDialContainerbackgroundColor];
    [self setUpUI];
  }
  return self;
}

#pragma mark - PRIVATE
- (void)setUpUI {
  UIImageView* folderIconView = [UIImageView new];
  folderIconView.contentMode = UIViewContentModeScaleAspectFit;
  folderIconView.image = [UIImage imageNamed:vNTPSDEmptyViewIcon];
  folderIconView.backgroundColor = UIColor.clearColor;

  [self addSubview:folderIconView];
  [folderIconView centerInSuperviewWithSize:CGSizeMake(iconSize, iconSize)];
}

@end
