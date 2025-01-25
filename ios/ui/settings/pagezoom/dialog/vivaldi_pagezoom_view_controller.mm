// Copyright 2024-25 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/pagezoom/dialog/vivaldi_pagezoom_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/ui/settings/pagezoom/dialog/uiwindow_pagezoom.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

using l10n_util::GetNSString;

namespace {
// Layout constants
const CGFloat kContentViewHeight = 200.0;
const CGFloat kCornerRadius = 18.0;
const CGFloat kShadowRadius = 1.0;
const CGFloat kShadowOpacity = 0.2;
const CGFloat kShadowOpacityDark = 0.4;
const CGFloat kIconSize = 20.0;
const CGFloat kTopPadding = 20.0;
const CGFloat kIconTitleSpacing = 10.0;
const CGFloat kHorizontalPadding = 20.0;
const CGFloat kVerticalSpacing = 2.0;
const CGFloat kButtonSpacing = 20.0;
const CGFloat kSwitchTopSpacing = 30.0;
const CGFloat kSwitchLabelSpacing = 10.0;
const CGFloat kMaxDialogWidth = 400.0;
const CGSize kShadowOffset = {0, 1};

// Icons
NSString *fallbackIcon = @"vivaldi_ntp_fallback_favicon";
}  // namespace

@interface VivaldiPageZoomViewController ()

@property (nonatomic, strong) UIImageView *iconImageView;
@property (nonatomic, strong) UILabel *titleLabel;
@property (nonatomic, strong) UILabel *zoomLabel;
@property (nonatomic, strong) UIButton *resetButton;
@property (nonatomic, strong) UIButton *minusButton;
@property (nonatomic, strong) UIButton *plusButton;
@property (nonatomic, strong) UIButton *doneButton;
@property (nonatomic, strong) UISwitch *globalSwitch;
@property (nonatomic, strong) UILabel *globalSettingTitleLabel;
@property (nonatomic, strong) UILabel *globalSettingSubtitleLabel;
@property (nonatomic, strong) UIView *contentView;
@end

@implementation VivaldiPageZoomViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  // Set background color and configure view appearance
  self.view.backgroundColor = [UIColor systemBackgroundColor];
  self.view.userInteractionEnabled = YES;

  // Configure rounded corners only at bottom
  self.view.layer.cornerRadius = kCornerRadius;
  self.view.layer.maskedCorners =
    kCALayerMinXMaxYCorner | kCALayerMaxXMaxYCorner;

  // Add shadow
  self.view.layer.shadowColor = [UIColor blackColor].CGColor;
  self.view.layer.shadowOffset = kShadowOffset;
  self.view.layer.shadowRadius = kShadowRadius;
  self.view.layer.shadowOpacity = kShadowOpacity;

  // Important: Set masksToBounds to NO on the main layer to allow shadow to show
  self.view.layer.masksToBounds = NO;

  // Add a content view inside main view to handle the corner masking
  self.contentView = [[UIView alloc] init];
  self.contentView.backgroundColor = [UIColor systemBackgroundColor];
  self.contentView.layer.cornerRadius = kCornerRadius;
  self.contentView.layer.maskedCorners =
    kCALayerMinXMaxYCorner | kCALayerMaxXMaxYCorner;
  self.contentView.layer.masksToBounds = YES;
  self.contentView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:self.contentView];

  // Pin content view to main view edges respecting safe area
  [NSLayoutConstraint activateConstraints:@[
    [self.contentView.topAnchor
      constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
    [self.contentView.leadingAnchor
      constraintEqualToAnchor:self.view.leadingAnchor],
    [self.contentView.trailingAnchor
      constraintEqualToAnchor:self.view.trailingAnchor],
    [self.contentView.bottomAnchor
      constraintEqualToAnchor:self.view.bottomAnchor],
    // Set a fixed height for the content
    [self.contentView.heightAnchor
      constraintEqualToConstant:kContentViewHeight]
  ]];

  [self setupUI];
  [self.zoomHandler refreshState];
}

// Add support for orientation changes
- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:(
         id<UIViewControllerTransitionCoordinator>
       )coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

  // Update shadow path for new size
  [coordinator animateAlongsideTransition:
    ^(id<UIViewControllerTransitionCoordinatorContext> context) {
    // Update shadow path if needed
    [self updateShadowPathForSize:size];
  } completion:nil];
}

// Helper method to update shadow path
- (void)updateShadowPathForSize:(CGSize)size {
  CGRect shadowRect =
    CGRectMake(0, 0, MIN(size.width, kMaxDialogWidth), size.height);
  self.view.layer.shadowPath =
    [UIBezierPath bezierPathWithRect:shadowRect].CGPath;
}

- (void)setupUI {
  // Icon
  self.iconImageView =
    [[UIImageView alloc] initWithImage: [UIImage imageNamed:fallbackIcon]];
  self.iconImageView.contentMode = UIViewContentModeScaleAspectFit;
  self.iconImageView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.contentView addSubview:self.iconImageView];

  // Title Label
  self.titleLabel = [[UILabel alloc] init];
  self.titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  self.titleLabel.textColor = [UIColor labelColor];
  [self.contentView addSubview:self.titleLabel];

  // Reset Button
  NSString *resetTitle = GetNSString(IDS_IOS_PAGEZOOM_SETTING_RESET_TITLE);
  self.resetButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [self.resetButton setTitle:resetTitle forState:UIControlStateNormal];
  self.resetButton.titleLabel.font =
    [UIFont preferredFontForTextStyle:UIFontTextStyleCallout];
  self.resetButton.titleLabel.adjustsFontForContentSizeCategory = YES;
  self.resetButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.resetButton addTarget:self.zoomHandler
                       action:@selector(resetZoom)
             forControlEvents:UIControlEventTouchUpInside];
  [self.contentView addSubview:self.resetButton];

  // Zoom Label
  self.zoomLabel = [[UILabel alloc] init];
  self.zoomLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
  self.zoomLabel.textColor = [UIColor labelColor];
  self.zoomLabel.textAlignment = NSTextAlignmentCenter;
  self.zoomLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [self.contentView addSubview:self.zoomLabel];

  // Minus Button
  self.minusButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [self.minusButton setTitle:@"−" forState:UIControlStateNormal];
  self.minusButton.titleLabel.font =
    [UIFont preferredFontForTextStyle:UIFontTextStyleTitle1];
  self.minusButton.titleLabel.adjustsFontForContentSizeCategory = YES;
  self.minusButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.minusButton addTarget:self.zoomHandler
                       action:@selector(zoomOut)
             forControlEvents:UIControlEventTouchUpInside];
  [self.contentView addSubview:self.minusButton];

  // Plus Button
  self.plusButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [self.plusButton setTitle:@"+" forState:UIControlStateNormal];
  self.plusButton.titleLabel.font =
    [UIFont preferredFontForTextStyle:UIFontTextStyleTitle1];
  self.plusButton.titleLabel.adjustsFontForContentSizeCategory = YES;
  self.plusButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.plusButton addTarget:self.zoomHandler
                      action:@selector(zoomIn)
            forControlEvents:UIControlEventTouchUpInside];
  [self.contentView addSubview:self.plusButton];

  // Done Button
  NSString *doneTitle = GetNSString(IDS_IOS_PAGEZOOM_SETTING_DONE_TITLE);
  self.doneButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [self.doneButton setTitle:doneTitle forState:UIControlStateNormal];
  self.doneButton.titleLabel.font =
    [UIFont preferredFontForTextStyle:UIFontTextStyleCallout];
  self.doneButton.titleLabel.adjustsFontForContentSizeCategory = YES;
  self.doneButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.doneButton addTarget:self
                      action:@selector(doneTapped)
            forControlEvents:UIControlEventTouchUpInside];
  [self.contentView addSubview:self.doneButton];

  // Global Setting Title
  NSString *globalSettingTitle = GetNSString(IDS_IOS_PAGEZOOM_SETTING_GLOBAL_TITLE);
  self.globalSettingTitleLabel = [[UILabel alloc] init];
  self.globalSettingTitleLabel.text = globalSettingTitle;
  self.globalSettingTitleLabel.font =
    [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  self.globalSettingTitleLabel.textColor = [UIColor labelColor];
  self.globalSettingTitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [self.contentView addSubview:self.globalSettingTitleLabel];

  // Global Setting Subtitle
  NSString *globalSettingSubtitle =
    GetNSString(IDS_IOS_PAGEZOOM_SETTING_GLOBAL_SUBTITLE);
  self.globalSettingSubtitleLabel = [[UILabel alloc] init];
  self.globalSettingSubtitleLabel.text = globalSettingSubtitle;
  self.globalSettingSubtitleLabel.font =
    [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  self.globalSettingSubtitleLabel.textColor = [UIColor secondaryLabelColor];
  self.globalSettingSubtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  self.globalSettingSubtitleLabel.numberOfLines = 0;
  self.globalSettingSubtitleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  [self.contentView addSubview:self.globalSettingSubtitleLabel];

  // Global Switch
  self.globalSwitch = [[UISwitch alloc] init];
  self.globalSwitch.translatesAutoresizingMaskIntoConstraints = NO;
  [self.globalSwitch addTarget:self.zoomHandler
                        action:@selector(updateGlobalZoomSwitch:)
              forControlEvents:UIControlEventValueChanged];
  [self.contentView addSubview:self.globalSwitch];

  // Layout Constraints
  [self setupConstraints];
}

- (void)setupConstraints {
  // Icon and Title
  [NSLayoutConstraint activateConstraints:@[
    [self.iconImageView.topAnchor
      constraintEqualToAnchor:self.contentView.safeAreaLayoutGuide.topAnchor
                     constant:kTopPadding],
    [self.iconImageView.trailingAnchor
      constraintEqualToAnchor:self.titleLabel.leadingAnchor
                     constant:-kIconTitleSpacing],
    [self.iconImageView.widthAnchor constraintEqualToConstant:kIconSize],
    [self.iconImageView.heightAnchor constraintEqualToConstant:kIconSize],

    [self.titleLabel.centerYAnchor
      constraintEqualToAnchor:self.iconImageView.centerYAnchor],
    [self.titleLabel.centerXAnchor
      constraintEqualToAnchor:self.contentView.centerXAnchor]
  ]];

  // Reset Button
  [NSLayoutConstraint activateConstraints:@[
    [self.resetButton.topAnchor
      constraintEqualToAnchor:self.iconImageView.bottomAnchor
                    constant:kTopPadding],
    [self.resetButton.leadingAnchor
      constraintEqualToAnchor:self.contentView.safeAreaLayoutGuide.leadingAnchor
                     constant:kHorizontalPadding]
  ]];

  // Zoom Label
  [NSLayoutConstraint activateConstraints:@[
    [self.zoomLabel.centerYAnchor
      constraintEqualToAnchor:self.resetButton.centerYAnchor],
    [self.zoomLabel.centerXAnchor
      constraintEqualToAnchor:self.contentView.centerXAnchor]
  ]];

  // Minus and Plus Buttons
  [NSLayoutConstraint activateConstraints:@[
    [self.minusButton.centerYAnchor
      constraintEqualToAnchor:self.zoomLabel.centerYAnchor],
    [self.minusButton.trailingAnchor
      constraintEqualToAnchor:self.zoomLabel.leadingAnchor
                     constant:-kButtonSpacing],

    [self.plusButton.centerYAnchor
      constraintEqualToAnchor:self.zoomLabel.centerYAnchor],
    [self.plusButton.leadingAnchor
      constraintEqualToAnchor:self.zoomLabel.trailingAnchor
                     constant:kButtonSpacing]
  ]];

  // Done Button
  [NSLayoutConstraint activateConstraints:@[
    [self.doneButton.centerYAnchor
      constraintEqualToAnchor:self.resetButton.centerYAnchor],
    [self.doneButton.trailingAnchor
      constraintEqualToAnchor:
        self.contentView.safeAreaLayoutGuide.trailingAnchor
      constant:-kHorizontalPadding]
  ]];

  // Global Switch and Labels
  [NSLayoutConstraint activateConstraints:@[
    [self.globalSwitch.topAnchor
      constraintEqualToAnchor:self.zoomLabel.bottomAnchor
                     constant:kSwitchTopSpacing],
    [self.globalSwitch.leadingAnchor
      constraintEqualToAnchor:self.contentView.safeAreaLayoutGuide.leadingAnchor
                     constant:kHorizontalPadding],

    [self.globalSettingTitleLabel.leadingAnchor
      constraintEqualToAnchor:self.globalSwitch.trailingAnchor
                     constant:kSwitchLabelSpacing],
    [self.globalSettingTitleLabel.topAnchor
      constraintEqualToAnchor:self.globalSwitch.topAnchor
                     constant:-kSwitchLabelSpacing],
    [self.globalSettingTitleLabel.trailingAnchor
      constraintEqualToAnchor:self.contentView.trailingAnchor
                     constant:-kHorizontalPadding],

    [self.globalSettingSubtitleLabel.topAnchor
      constraintEqualToAnchor:self.globalSettingTitleLabel.bottomAnchor
                     constant:kVerticalSpacing],
    [self.globalSettingSubtitleLabel.leadingAnchor
      constraintEqualToAnchor:self.globalSettingTitleLabel.leadingAnchor],
    [self.globalSettingSubtitleLabel.trailingAnchor
      constraintEqualToAnchor:self.contentView.trailingAnchor
                     constant:-kHorizontalPadding]
  ]];
}

#pragma mark - Actions

- (void)doneTapped {
  UIWindow *keyWindow = nil;
  for (UIScene *scene in [UIApplication sharedApplication].connectedScenes) {
    if (scene.activationState == UISceneActivationStateForegroundActive &&
        [scene isKindOfClass:[UIWindowScene class]]) {
      keyWindow = ((UIWindowScene *)scene).windows.firstObject;
      break;
    }
  }
  [keyWindow hidePageZoomViewController];
}

#pragma mark - VivaldiPageZoomDialogConsumer

- (void)setZoomInEnabled:(BOOL)enabled {
  self.plusButton.enabled = enabled;
}

- (void)setZoomOutEnabled:(BOOL)enabled {
  self.minusButton.enabled = enabled;
}

- (void)setResetZoomEnabled:(BOOL)enabled {
  self.resetButton.enabled = enabled;
}

- (void)setCurrentZoomLevel:(int)zoomLevel {
  self.zoomLabel.text = [NSString stringWithFormat:@"%d%%", zoomLevel];
}

- (void)setCurrentHostURL:(NSString*)host {
  NSString *fullText = l10n_util::GetNSStringF(
      IDS_IOS_PAGEZOOM_SETTING_HOST_TITLE,
      base::SysNSStringToUTF16(host));
  NSMutableAttributedString *attributedString =
      [[NSMutableAttributedString alloc] initWithString:fullText];

  // Get the body font descriptor and create a bold version
  UIFontDescriptor *bodyDescriptor =
    [UIFontDescriptor preferredFontDescriptorWithTextStyle:UIFontTextStyleBody];
  UIFont *boldFont =
    [UIFont systemFontOfSize:bodyDescriptor.pointSize weight:UIFontWeightBold];

  // Apply bold font to the domain part
  NSRange hostRange = [fullText rangeOfString:host];
  [attributedString addAttribute:NSFontAttributeName
                           value:boldFont range:hostRange];

  self.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  self.titleLabel.adjustsFontForContentSizeCategory = YES;
  self.titleLabel.attributedText = attributedString;
}

- (void)setCurrentHostFavicon:(UIImage*)image {
  self.iconImageView.image = image;
}

-  (void)setGlobalPageZoom:(BOOL)enabled {
  self.globalSwitch.on = enabled;
}

// Add support for trait collection changes
- (void)traitCollectionDidChange:(UITraitCollection *)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  if (self.traitCollection.userInterfaceStyle !=
        previousTraitCollection.userInterfaceStyle) {
    // Layout Constraints
    [self.view setNeedsUpdateConstraints];
    [self.view updateConstraintsIfNeeded];

    // Update shadow color based on interface style
    self.view.layer.shadowColor = [UIColor blackColor].CGColor;
    self.view.layer.shadowOpacity =
      self.traitCollection.userInterfaceStyle ==
        UIUserInterfaceStyleDark ? kShadowOpacityDark : kShadowOpacity;
  }
}

@end
