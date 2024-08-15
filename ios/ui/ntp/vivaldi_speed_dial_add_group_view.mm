// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ntp/vivaldi_speed_dial_add_group_view.h"

#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/ui/bookmarks_editor/vivaldi_bookmarks_constants.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/helpers/vivaldi_uiview_style_helper.h"
#import "ios/ui/ntp/vivaldi_ntp_constants.h"
#import "ios/ui/ntp/vivaldi_speed_dial_constants.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

using l10n_util::GetNSString;

namespace {

const CGFloat cornerRadius = 12;
const CGFloat buttonContentVPadding = 12;
const CGFloat buttonContentHPadding = 20;
const CGFloat buttonTopPadding = 24;

const CGFloat commonPadding = 20;
const CGFloat descriptionTopPadding = 8;

const CGFloat imageViewWidth = 300;
const CGFloat imageViewHeight = 84;
const CGFloat imageViewTopPadding = 12;
const CGFloat imageViewHPadding = 20;

}  // namespace

@interface VivaldiSpeedDialAddGroupView ()

// UI
@property (nonatomic, weak) UIImageView *addGroupIllustrationView;
@property (nonatomic, weak) UILabel *titleLabel;
@property (nonatomic, weak) UILabel *descriptionLabel;
@property (nonatomic, weak) UIButton *addButton;

// Dynamic Constaints
// Add group illustration is visible on all device and layout state except
// Compact Height && Regular Width combo.
@property (nonatomic, strong) NSArray<NSLayoutConstraint*>
    *imageViewVisibleConstraints;
@property (nonatomic, strong) NSArray<NSLayoutConstraint*>
    *imageViewHiddenConstraints;

@end

@implementation VivaldiSpeedDialAddGroupView

- (instancetype)init {
  self = [super init];
  if (self) {
    [self setupViews];
  }
  return self;
}

#pragma mark - Public
- (void)refreshLayoutWithVerticalSizeClass:
    (UIUserInterfaceSizeClass)verticalSizeClass {
  if ([self shouldHideImageViewWithVerticalSizeClass:verticalSizeClass]) {
    [self deactivateImageViewConstraints];
  } else {
    [self activateImageViewConstraints];
  }
  [self layoutIfNeeded];
}

#pragma mark - Private

- (void)setupViews {
  self.layer.cornerRadius = cornerRadius;
  self.clipsToBounds = YES;

  // Add drop shadow
  [self addShadowWithBackground:
                        [UIColor colorNamed:vNTPSpeedDialCellBackgroundColor]
                         offset:vSpeedDialItemShadowOffset
                    shadowColor:[UIColor colorNamed:vSpeedDialItemShadowColor]
                         radius:vSpeedDialItemShadowRadius
                        opacity:vSpeedDialItemShadowOpacity
  ];
  self.backgroundColor = [UIColor colorNamed:vNTPSpeedDialCellBackgroundColor];

  // Add group illustration
  UIImageView* imageView =
      [[UIImageView alloc] initWithImage:
            [UIImage imageNamed:vBookmarkAddGroupIllustration]];
  self.addGroupIllustrationView = imageView;
  imageView.contentMode = UIViewContentModeScaleAspectFit;
  imageView.backgroundColor = UIColor.clearColor;
  [self addSubview:imageView];

  // Title Label
  UILabel* titleLabel = [[UILabel alloc] init];
  self.titleLabel = titleLabel;
  titleLabel.text = GetNSString(IDS_IOS_START_PAGE_PROMO_CREATE_NEW_GROUP_TITLE);
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle3];
  titleLabel.font = [UIFont boldSystemFontOfSize:titleLabel.font.pointSize];
  titleLabel.numberOfLines = 0;
  titleLabel.textAlignment = NSTextAlignmentCenter;
  [self addSubview:titleLabel];

  // Description Label
  UILabel* descriptionLabel = [[UILabel alloc] init];
  self.descriptionLabel = descriptionLabel;
  descriptionLabel.text =
      GetNSString(IDS_IOS_START_PAGE_PROMO_CREATE_NEW_GROUP_DESCRIPTION);
  descriptionLabel.textColor = UIColor.secondaryLabelColor;
  descriptionLabel.adjustsFontForContentSizeCategory = YES;
  descriptionLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  descriptionLabel.numberOfLines = 0;
  descriptionLabel.textAlignment = NSTextAlignmentCenter;
  [self addSubview:descriptionLabel];

  // Add Button
  UIButton* addButton = [UIButton buttonWithType:UIButtonTypeSystem];
  self.addButton = addButton;
  [addButton setTitle:GetNSString(IDS_IOS_START_PAGE_PROMO_ADD_NEW_GROUP_TITLE)
             forState:UIControlStateNormal];
  addButton.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  addButton.titleLabel.font =
      [UIFont boldSystemFontOfSize:addButton.titleLabel.font.pointSize];
  [addButton addTarget:self
                action:@selector(addButtonTapped:)
      forControlEvents:UIControlEventTouchUpInside];

  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration tintedButtonConfiguration];
  buttonConfiguration.contentInsets =
      NSDirectionalEdgeInsetsMake(buttonContentVPadding, buttonContentHPadding,
                                  buttonContentVPadding, buttonContentHPadding);
  buttonConfiguration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  addButton.configuration = buttonConfiguration;

  addButton.pointerInteractionEnabled = YES;
  addButton.pointerStyleProvider =
      CreateOpaqueOrTransparentButtonPointerStyleProvider();

  [self addSubview:addButton];

  // Set up the constraints
  [self setUpConstraints];
}

- (void)setUpConstraints {
  // Add group illustration.
  [self.addGroupIllustrationView anchorTop:nil
                                   leading:self.leadingAnchor
                                    bottom:nil
                                  trailing:self.trailingAnchor
                                   padding:UIEdgeInsetsMake(
                                        0, imageViewHPadding,
                                        0, imageViewHPadding)];

  // Dynamic constraints
  self.imageViewVisibleConstraints = @[
    [self.addGroupIllustrationView.topAnchor
          constraintEqualToAnchor:self.topAnchor
                         constant:imageViewTopPadding],
    [self.addGroupIllustrationView.widthAnchor
        constraintEqualToConstant:imageViewWidth],
    [self.addGroupIllustrationView.heightAnchor
        constraintEqualToConstant:imageViewHeight],
    [self.titleLabel.topAnchor
        constraintEqualToAnchor:self.addGroupIllustrationView.bottomAnchor
                       constant:commonPadding]
  ];

  self.imageViewHiddenConstraints = @[
    [self.titleLabel.topAnchor constraintEqualToAnchor:self.topAnchor
                                              constant:commonPadding]
  ];

  if ([self shouldHideImageViewWithVerticalSizeClass:
      self.traitCollection.verticalSizeClass]) {
    [self deactivateImageViewConstraints];
  } else {
    [self activateImageViewConstraints];
  }

  // Title Label
  [self.titleLabel anchorTop:nil
                     leading:self.leadingAnchor
                      bottom:nil
                    trailing:self.trailingAnchor
                     padding:UIEdgeInsetsMake(0, commonPadding,
                                              0, commonPadding)];

  // Description Label
  [self.descriptionLabel anchorTop:self.titleLabel.bottomAnchor
                           leading:self.leadingAnchor
                            bottom:nil
                          trailing:self.trailingAnchor
                           padding:UIEdgeInsetsMake(descriptionTopPadding,
                                                    commonPadding, 0,
                                                    commonPadding)];

  // Add Button
  [self.addButton anchorTop:self.descriptionLabel.bottomAnchor
                           leading:nil
                            bottom:self.bottomAnchor
                          trailing:nil
                           padding:UIEdgeInsetsMake(buttonTopPadding, 0,
                                                    commonPadding, 0)];
  [self.addButton centerXInSuperview];
}

#pragma mark - Helpers

- (void)activateImageViewConstraints {
  if (!self.imageViewVisibleConstraints || !self.imageViewHiddenConstraints)
    return;
  [NSLayoutConstraint deactivateConstraints:self.imageViewHiddenConstraints];
  [NSLayoutConstraint activateConstraints:self.imageViewVisibleConstraints];
  self.addGroupIllustrationView.hidden = NO;
}

- (void)deactivateImageViewConstraints {
  if (!self.imageViewVisibleConstraints || !self.imageViewHiddenConstraints)
    return;
  [NSLayoutConstraint deactivateConstraints:self.imageViewVisibleConstraints];
  [NSLayoutConstraint activateConstraints:self.imageViewHiddenConstraints];
  self.addGroupIllustrationView.hidden = YES;
}

- (BOOL)shouldHideImageViewWithVerticalSizeClass:
    (UIUserInterfaceSizeClass)verticalSizeClass {
  return verticalSizeClass ==
              UIUserInterfaceSizeClassCompact;
}

#pragma mark - Action
- (void)addButtonTapped:(UIButton *)sender {
  [self.delegate didTapAddNewGroup];
}

@end
