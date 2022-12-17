// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/chrome/browser/ui/ntp/vivaldi_private_mode_view.h"

#import "UIKit/UIKit.h"

#import "ios/chrome/browser/ui/ntp/vivaldi_ntp_constants.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

using ui::GetDeviceFormFactor;
using ui::DEVICE_FORM_FACTOR_TABLET;

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Common padding for the components. Used as both horizontal and vertical
// padding where needed.
CGFloat const commonPadding = 16.0;
// Container top padding. We will use different padding for iPad and iPhone.
// to make the container look centered on the tab grid snapshot.
CGFloat const containerTopPaddingiPhone = 60.0;
CGFloat const containerTopPaddingiPad = 140.0;
// Ghost icon height and width. We will use different size for iPad and iPhone.
CGFloat const ghostIconSizeiPhone = 70.0;
CGFloat const ghostIconSizeiPad = 110.0;
// Top padding for description label
CGFloat const descriptionLabelTopPadding = 24.0;
} // End Namespace

@implementation VivaldiPrivateModeView

#pragma mark - INITIALIZER
- (instancetype)init {
  if (self = [super initWithFrame:CGRectZero]) {
    self.backgroundColor =
      [UIColor colorNamed:vNTPBackgroundColor];
    [self setUpUI];
  }
  return self;
}

#pragma mark - PRIVATE
- (void)setUpUI {
  // Background
  UIImageView* bgView = [UIImageView new];
  bgView.contentMode = UIViewContentModeScaleAspectFill;
  bgView.image = [UIImage imageNamed:vNTPPrivateTabBG];
  bgView.backgroundColor = UIColor.clearColor;

  [self addSubview:bgView];
  [bgView fillSuperview];

  // Container View
  UIView* containerView = [UIView new];
  containerView.backgroundColor = UIColor.clearColor;
  [self addSubview:containerView];
  [containerView anchorTop:self.safeTopAnchor
                   leading:self.safeLeftAnchor
                    bottom:nil
                  trailing:self.safeRightAnchor
                   padding:UIEdgeInsetsMake([self isCurrentDeviceTablet] ?
                                                  containerTopPaddingiPad :
                                                  containerTopPaddingiPhone,
                                            commonPadding,
                                            0,
                                            commonPadding)];

  // Ghost Icon
  UIImageView* ghostIcon = [UIImageView new];
  ghostIcon.contentMode = UIViewContentModeScaleAspectFit;
  ghostIcon.image = [UIImage imageNamed:vNTPPrivateTabGhost];
  ghostIcon.backgroundColor = UIColor.clearColor;

  [containerView addSubview:ghostIcon];
  [ghostIcon anchorTop:containerView.topAnchor
               leading:nil
                bottom:nil
              trailing:nil
               padding:UIEdgeInsetsMake(commonPadding, 0, 0, 0)];
  if ([self isCurrentDeviceTablet]) {
    [ghostIcon setViewSize:CGSizeMake(ghostIconSizeiPad,
                                      ghostIconSizeiPad)];
  } else {
    [ghostIcon setViewSize:CGSizeMake(ghostIconSizeiPhone,
                                      ghostIconSizeiPhone)];
  }
  [ghostIcon centerXInSuperview];

  // Title Label
  UILabel* titleLabel = [UILabel new];
  NSString* titleString = l10n_util::GetNSString(IDS_IOS_NEW_TAB_PRIVATE_TITLE);
  titleLabel.text = titleString;
  titleLabel.accessibilityLabel = titleString;
  titleLabel.textAlignment = NSTextAlignmentCenter;
  titleLabel.numberOfLines = 1;
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];

  [containerView addSubview:titleLabel];
  [titleLabel anchorTop:ghostIcon.bottomAnchor
                leading:containerView.leadingAnchor
                 bottom:nil
               trailing:containerView.trailingAnchor
                padding:UIEdgeInsetsMake(commonPadding, 0, 0, 0)];

  // Description Label
  UILabel* descriptionLabel = [UILabel new];
  NSString* descriptionString =
    l10n_util::GetNSString(IDS_IOS_NEW_TAB_PRIVATE_MESSAGE);
  descriptionLabel.text = descriptionString;
  descriptionLabel.accessibilityLabel = descriptionString;
  descriptionLabel.textAlignment = NSTextAlignmentCenter;
  descriptionLabel.numberOfLines = 0;
  descriptionLabel.font =
    [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];

  [containerView addSubview:descriptionLabel];
  [descriptionLabel anchorTop:titleLabel.bottomAnchor
                      leading:containerView.leadingAnchor
                       bottom:containerView.bottomAnchor
                     trailing:containerView.trailingAnchor
                      padding:UIEdgeInsetsMake(descriptionLabelTopPadding,
                                               0,
                                               commonPadding,
                                               0)];
}

/// Returns whether current device is iPhone or iPad.
- (BOOL)isCurrentDeviceTablet {
  return GetDeviceFormFactor() == DEVICE_FORM_FACTOR_TABLET;
}

@end
