// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/sync/cells/vivaldi_table_view_sync_user_info_item.h"

#import "ios/chrome/browser/shared/ui/symbols/symbol_helpers.h"
#import "ios/chrome/browser/shared/ui/symbols/symbol_names.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

namespace {

const CGFloat kStackMargin = 13.0;
const CGFloat kHorizontalStackViewSpacing = 20.0;
const CGFloat kVerticalStackViewSpacing = 4.0;
const CGFloat kAvatarImageSize = 54.0;
const CGFloat kBadgeImageSize = 20.0;
const CGFloat kSessionInfoPillRadius = 16.0;
const CGFloat kSessionInfoEditSymbolPoint = 12.0;

const CGSize kSessionInfoSeparatorSize = CGSizeMake(1, 0);
const CGSize kSessionInfoEditSize = CGSizeMake(0, 16);

const UIEdgeInsets kSessionNameLabelPadding = UIEdgeInsetsMake(8, 12, 8, 0);
const UIEdgeInsets kSessionInfoSeparatorPadding = UIEdgeInsetsMake(8, 8, 8, 0);
const UIEdgeInsets kSessionInfoEditPadding = UIEdgeInsetsMake(0, 8, 0, 12);

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

  // Add extra space before the name to optically align the user name with
  // session/device name pill.
  header.userNameLabel.text =
      [NSString stringWithFormat:@"  %@", self.userName];

  header.sessionNameLabel.text = self.sessionName;
  [header setUserAvatar:self.userAvatar];
  [header setBadgeImage:self.supporterBadge];
}

@end

#pragma mark - VivaldiTableViewSyncUserInfoView

@interface VivaldiTableViewSyncUserInfoView ()
@property(nonatomic, strong) UIView* userNameContainer;
@property(nonatomic, strong) UIView* sessionInfoContainer;
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

    _sessionInfoContainer = [self setupSessionInfoPillView];

    UIStackView* verticalStackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[_userNameLabel, _sessionInfoContainer]];
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

- (UIView*)setupSessionInfoPillView {

  UIView *pillView = [[UIView alloc] init];
  pillView.backgroundColor =
      [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
  pillView.layer.cornerRadius = kSessionInfoPillRadius;
  pillView.clipsToBounds = YES;

  _sessionNameLabel = [[UILabel alloc] init];
  _sessionNameLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  _sessionNameLabel.font =
      [UIFont preferredFontForTextStyle:sessionTextStyle];
  _sessionNameLabel.textAlignment = NSTextAlignmentCenter;
  _sessionNameLabel.numberOfLines = 0;
  [pillView addSubview:_sessionNameLabel];
  [_sessionNameLabel anchorTop:pillView.topAnchor
                       leading:pillView.leadingAnchor
                        bottom:pillView.bottomAnchor
                      trailing:nil
                       padding:kSessionNameLabelPadding];

  UIView *separator = [[UIView alloc] init];
  separator.backgroundColor = [UIColor tertiarySystemGroupedBackgroundColor];
  [pillView addSubview:separator];
  [separator anchorTop:pillView.topAnchor
               leading:_sessionNameLabel.trailingAnchor
                bottom:pillView.bottomAnchor
              trailing:nil
               padding:kSessionInfoSeparatorPadding
                  size:kSessionInfoSeparatorSize];

  NSString* buttonTitle =
      [NSString stringWithFormat:@" %@",
          l10n_util::GetNSString(IDS_VIVALDI_SYNC_DEVICE_NAME_EDIT)];
  UIButton *editButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [editButton setTitle:buttonTitle forState:UIControlStateNormal];

  UIImageSymbolConfiguration* symbolConfig = [UIImageSymbolConfiguration
      configurationWithPointSize:kSessionInfoEditSymbolPoint
                          weight:UIImageSymbolWeightRegular
                           scale:UIImageSymbolScaleMedium];
  [editButton setPreferredSymbolConfiguration:symbolConfig
                        forImageInState:UIControlStateNormal];
  [editButton setImage:DefaultSymbolWithPointSize(
                                    kPencilSymbol, kSessionInfoEditSymbolPoint)
              forState:UIControlStateNormal];
  editButton.imageView.contentMode = UIViewContentModeScaleAspectFit;
  editButton.titleLabel.font =
      [UIFont preferredFontForTextStyle:sessionTextStyle];
  [editButton setTitleColor:[UIColor colorNamed:kBlueColor]
                   forState:UIControlStateNormal];
  [editButton addTarget:self
                 action:@selector(handleEditTap)
       forControlEvents:UIControlEventTouchUpInside];

  [pillView addSubview:editButton];
  [editButton anchorTop:nil
               leading:separator.trailingAnchor
                bottom:nil
               trailing:pillView.trailingAnchor
                padding:kSessionInfoEditPadding
                   size:kSessionInfoEditSize];
  [editButton centerYInSuperview];

  return pillView;
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

#pragma mark - Actions
- (void)handleEditTap {
  [self.delegate
      didTapSessionEditButtonWithCurrentSession:self.sessionNameLabel.text];
}

@end
