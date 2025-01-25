// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/sync/cells/vivaldi_table_view_sync_user_info_item.h"

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"

namespace {

const CGFloat kStackMargin = 13.0;
const CGFloat kHorizontalStackViewSpacing = 20.0;
const CGFloat kVerticalStackViewSpacing = 4.0;
const CGFloat kAvatarImageSize = 54.0;

const CGFloat kBadgeImageSize = 20.0;

const UIFontTextStyle nameTextStyle = UIFontTextStyleHeadline;
const UIFontTextStyle sessionTextStyle = UIFontTextStyleFootnote;

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
  header.sessionNameLabel.text = self.sessionName;
  [header setUserAvatar:self.userAvatar];
  [header setBadgeImage:self.supporterBadge];
}

@end

#pragma mark - VivaldiTableViewSyncUserInfoView

@interface VivaldiTableViewSyncUserInfoView ()
@property(nonatomic, strong) UIImageView* profileImageView;
@property(nonatomic, strong) UIImageView* badgeImageView;
@end

@implementation VivaldiTableViewSyncUserInfoView

- (instancetype)initWithReuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithReuseIdentifier:reuseIdentifier];
  if (self) {
    _userNameLabel = [[UILabel alloc] init];
    _userNameLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _userNameLabel.font = [UIFont preferredFontForTextStyle:nameTextStyle];
    _userNameLabel.textAlignment = NSTextAlignmentLeft;
    _userNameLabel.numberOfLines = 0;

    _sessionNameLabel = [[UILabel alloc] init];
    _sessionNameLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _sessionNameLabel.font =
        [UIFont preferredFontForTextStyle:sessionTextStyle];
    _sessionNameLabel.textAlignment = NSTextAlignmentCenter;
    _sessionNameLabel.numberOfLines = 0;

    UIStackView* verticalStackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[_userNameLabel, _sessionNameLabel]];
    verticalStackView.axis = UILayoutConstraintAxisVertical;
    verticalStackView.alignment = UIStackViewAlignmentLeading;
    verticalStackView.spacing = kVerticalStackViewSpacing;

    _profileImageView = [UIImageView new];
    _profileImageView.contentMode = UIViewContentModeScaleAspectFill;
    _profileImageView.layer.cornerRadius = kAvatarImageSize/2;
    _profileImageView.layer.masksToBounds = YES;

    // Set up constraints for the badgeImageView
    _badgeImageView = [UIImageView new];
    _badgeImageView.contentMode = UIViewContentModeScaleAspectFit;

    UIStackView* stackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[_profileImageView, verticalStackView]];
    stackView.axis = UILayoutConstraintAxisHorizontal;
    stackView.alignment = UIStackViewAlignmentCenter;
    stackView.spacing = kHorizontalStackViewSpacing;
    [self.contentView addSubview:stackView];

    [stackView fillSuperviewWithPadding:UIEdgeInsetsMake(
              kStackMargin,kStackMargin,
              kStackMargin,kStackMargin)];
  }
  // Add badgeImageView as a sibling of imageView
  [self.contentView addSubview:_badgeImageView];
  // Set up constraints for the badgeImageView relative to imageView
  _badgeImageView.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [_badgeImageView.widthAnchor constraintEqualToConstant:kBadgeImageSize],
    [_badgeImageView.heightAnchor constraintEqualToConstant:kBadgeImageSize],
    [_badgeImageView.bottomAnchor constraintEqualToAnchor:
      _profileImageView.bottomAnchor],
    [_badgeImageView.rightAnchor constraintEqualToAnchor:
      _profileImageView.rightAnchor]
  ]];
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];

  self.userNameLabel.hidden = NO;
  self.sessionNameLabel.hidden = NO;
  self.profileImageView.hidden = NO;
  self.badgeImageView.hidden = NO;
  [self setUserAvatar:nil];
  [self setBadgeImage:nil];
}

- (void)setBadgeImage:(UIImage*)badgeImage {
  _badgeImageView.image = badgeImage;
}

- (void)setUserAvatar:(UIImage*)image {
  CGSize size = CGSizeMake(kAvatarImageSize, kAvatarImageSize);
  self.profileImageView.layer.cornerRadius = kAvatarImageSize/2;
  self.profileImageView.layer.masksToBounds = YES;
  self.profileImageView.image =
    ResizeImage(image, size, ProjectionMode::kAspectFill);
}

@end
