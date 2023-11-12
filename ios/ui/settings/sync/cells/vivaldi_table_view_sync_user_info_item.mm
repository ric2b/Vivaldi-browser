// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/sync/cells/vivaldi_table_view_sync_user_info_item.h"

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kStackMargin = 13.0;
const CGFloat kHorizontalStackViewSpacing = 20.0;
const CGFloat kVerticalStackViewSpacing = 4.0;
const CGFloat kAvatarImageSize = 54.0;

const UIFontTextStyle nameTextStyle = UIFontTextStyleHeadline;
const UIFontTextStyle emailTextStyle = UIFontTextStyleSubheadline;

}

#pragma mark - VivaldiTableViewSyncUserInfoItem

@implementation VivaldiTableViewSyncUserInfoItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [VivaldiTableViewSyncUserInfoView class];
  }
  return self;
}

- (void)configureHeaderFooterView:(VivaldiTableViewSyncUserInfoView*)header
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureHeaderFooterView:header withStyler:styler];
  header.userNameLabel.text = self.userName;
  header.userEmailLabel.text = self.userEmail;
  [header setUserAvatar:self.userAvatar];
}

@end

#pragma mark - VivaldiTableViewSyncUserInfoView

@interface VivaldiTableViewSyncUserInfoView ()
@property(nonatomic, strong) UIImageView* imageView;
@end

@implementation VivaldiTableViewSyncUserInfoView

- (instancetype)initWithReuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithReuseIdentifier:reuseIdentifier];
  if (self) {
    _userNameLabel = [[UILabel alloc] init];
    _userNameLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _userNameLabel.font = [UIFont preferredFontForTextStyle:nameTextStyle];
    _userNameLabel.textAlignment = NSTextAlignmentCenter;
    _userNameLabel.numberOfLines = 0;

    _userEmailLabel = [[UILabel alloc] init];
    _userEmailLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _userEmailLabel.font =
        [UIFont preferredFontForTextStyle:emailTextStyle];
    _userEmailLabel.textAlignment = NSTextAlignmentCenter;
    _userEmailLabel.numberOfLines = 0;

    UIStackView* verticalStackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[_userNameLabel, _userEmailLabel]];
    verticalStackView.axis = UILayoutConstraintAxisVertical;
    verticalStackView.alignment = UIStackViewAlignmentLeading;
    verticalStackView.spacing = kVerticalStackViewSpacing;

    _imageView = [UIImageView new];
    _imageView.contentMode = UIViewContentModeScaleAspectFill;
    _imageView.layer.cornerRadius = kAvatarImageSize/2;
    _imageView.layer.masksToBounds = YES;

    UIStackView* stackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[_imageView, verticalStackView]];
    stackView.axis = UILayoutConstraintAxisHorizontal;
    stackView.alignment = UIStackViewAlignmentCenter;
    stackView.spacing = kHorizontalStackViewSpacing;
    [self.contentView addSubview:stackView];

    [stackView fillSuperviewWithPadding:UIEdgeInsetsMake(
              kStackMargin,kStackMargin,
              kStackMargin,kStackMargin)];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];

  self.userNameLabel.hidden = NO;
  self.userEmailLabel.hidden = NO;
  self.imageView.hidden = NO;
  [self setUserAvatar:nil];
}

- (void)setUserAvatar:(UIImage*)image {
  CGSize size = CGSizeMake(kAvatarImageSize, kAvatarImageSize);
  self.imageView.layer.cornerRadius = kAvatarImageSize/2;
  self.imageView.layer.masksToBounds = YES;
  self.imageView.image = ResizeImage(image, size, ProjectionMode::kAspectFill);
}

@end
