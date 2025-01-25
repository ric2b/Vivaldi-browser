// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/toolbar/vivaldi_sticky_toolbar_view.h"

#import "UIKit/UIKit.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_constants+vivaldi.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
const CGFloat locationContainerVerticalPadding = 4;
const CGFloat locationContainerHorizontalPadding = 52;
const CGFloat locationImageViewWidth = 14;
const CGFloat locationImageViewYOffset = 0.5;
const CGFloat locationImageViewRightPadding = 2;
}

@interface VivaldiStickyToolbarView()

@property(nonatomic,weak) UIView* locationContainer;
@property(nonatomic,weak) UILabel* locationLabel;
@property(nonatomic,weak) UIImageView* locationIconImageView;
@property(nonatomic,copy) NSString* securityLevelAccessibilityString;

// Boolean to determine if the full address should be shown.
@property(nonatomic, assign) BOOL showFullAddress;

// Constraints to hide the location image view.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* hideLocationImageConstraints;
// Constraints to show the location image view.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* showLocationImageConstraints;

@end

@implementation VivaldiStickyToolbarView

@synthesize locationContainer = _locationContainer;
@synthesize locationLabel = _locationLabel;
@synthesize locationIconImageView = _locationIconImageView;
@synthesize securityLevelAccessibilityString =
  _securityLevelAccessibilityString;

#pragma mark - INITIALIZER
- (instancetype)init {
  if (self = [super initWithFrame:CGRectZero]) {
    self.backgroundColor = UIColor.clearColor;
    [self setUpUI];
  }
  return self;
}

#pragma mark - SET UP UI COMPONENTS
- (void)setUpUI {

  self.backgroundColor = UIColor.clearColor;

  UIView* locationContainer = [UIView new];
  _locationContainer = locationContainer;
  locationContainer.backgroundColor = UIColor.clearColor;
  [self addSubview:locationContainer];
  [locationContainer anchorTop:self.safeTopAnchor
                       leading:nil
                        bottom:self.safeBottomAnchor
                      trailing:nil
                       padding:UIEdgeInsetsMake(
                            0, 0, locationContainerVerticalPadding, 0)];

  [NSLayoutConstraint activateConstraints:@[
    [locationContainer.leadingAnchor
      constraintGreaterThanOrEqualToAnchor:self.safeLeftAnchor
                                  constant:locationContainerHorizontalPadding],
    [locationContainer.trailingAnchor
      constraintLessThanOrEqualToAnchor:self.safeRightAnchor
                               constant:-locationContainerHorizontalPadding]
  ]];
  [locationContainer centerXInSuperview];

  // Setup accessibility.
  locationContainer.isAccessibilityElement = YES;
  locationContainer.accessibilityLabel =
      l10n_util::GetNSString(IDS_ACCNAME_LOCATION);

  UILabel* locationLabel = [UILabel new];
  _locationLabel = locationLabel;
  locationLabel.adjustsFontForContentSizeCategory = YES;
  locationLabel.font =
    [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  locationLabel.backgroundColor = UIColor.clearColor;
  locationLabel.textColor = UIColor.labelColor;

  [locationContainer addSubview:locationLabel];
  [locationLabel anchorTop:locationContainer.topAnchor
                   leading:nil
                    bottom:locationContainer.bottomAnchor
                  trailing:locationContainer.trailingAnchor];

  UIImageView *locationIconImageView = [UIImageView new];
  _locationIconImageView = locationIconImageView;
  locationIconImageView.contentMode = UIViewContentModeScaleAspectFit;
  locationIconImageView.backgroundColor = UIColor.clearColor;
  locationIconImageView.tintColor = UIColor.labelColor;

  [locationContainer addSubview:locationIconImageView];
  [locationIconImageView setWidthWithConstant:locationImageViewWidth];
  [locationIconImageView.centerYAnchor
    constraintEqualToAnchor:locationLabel.centerYAnchor
                   constant:locationImageViewYOffset].active = YES;

  _showLocationImageConstraints = @[
    [locationContainer.leadingAnchor
        constraintEqualToAnchor:locationIconImageView.leadingAnchor],
    [locationIconImageView.trailingAnchor
        constraintEqualToAnchor:locationLabel.leadingAnchor
                       constant:-locationImageViewRightPadding],
  ];

  _hideLocationImageConstraints = @[
    [locationContainer.leadingAnchor
        constraintEqualToAnchor:locationLabel.leadingAnchor],
  ];

  [NSLayoutConstraint activateConstraints:_showLocationImageConstraints];
}

- (void)updateAccessibility {
  if (self.securityLevelAccessibilityString.length > 0) {
    self.locationContainer.accessibilityValue =
        [NSString stringWithFormat:@"%@ %@", self.locationLabel.text,
                                   self.securityLevelAccessibilityString];
  } else {
    self.locationContainer.accessibilityValue =
        [NSString stringWithFormat:@"%@", self.locationLabel.text];
  }
}


#pragma mark - SETTERS
- (void)updateLocationText:(NSString*)text
                    domain:(NSString*)domain
                  showFull:(BOOL)showFull {
  if (self.showFullAddress != showFull)
    self.showFullAddress = showFull;

  self.locationLabel.lineBreakMode = NSLineBreakByTruncatingTail;

  if (showFull && ![text isEqualToString:domain] && [text length] > 0) {
    UIColor *fullTextColor =
        [[UIColor labelColor]
            colorWithAlphaComponent:vLocationBarSteadyViewFullAddressOpacity];
    UIColor *domainColor =
        [[UIColor labelColor]
             colorWithAlphaComponent:vLocationBarSteadyViewDomainOpacity];
    NSAttributedString *attributedString =
        [VivaldiGlobalHelpers attributedStringWithText:text
                                             highlight:domain
                                             textColor:fullTextColor
                                        highlightColor:domainColor];
    self.locationLabel.attributedText = attributedString;
  } else {
    self.locationLabel.text = text;
    self.locationLabel.textColor = [UIColor labelColor];
  }
  [self updateAccessibility];
}

- (void)setLocationImage:(UIImage*)locationImage {
  BOOL hadImage = self.locationIconImageView.image != nil;
  BOOL hasImage = locationImage != nil;
  self.locationIconImageView.image = locationImage;
  if (hadImage == hasImage) {
    return;
  }

  if (hasImage) {
    [NSLayoutConstraint
        deactivateConstraints:self.hideLocationImageConstraints];
    [NSLayoutConstraint activateConstraints:self.showLocationImageConstraints];
  } else {
    [NSLayoutConstraint
        deactivateConstraints:self.showLocationImageConstraints];
    [NSLayoutConstraint activateConstraints:self.hideLocationImageConstraints];
  }
}

- (void)setTintColor:(UIColor*)tintColor {
  self.locationLabel.textColor = tintColor;
  self.locationIconImageView.tintColor = tintColor;
}

- (void)setSecurityLevelAccessibilityString:(NSString*)string {
  if ([_securityLevelAccessibilityString isEqualToString:string]) {
    return;
  }
  _securityLevelAccessibilityString = [string copy];
  [self updateAccessibility];
}

@end
