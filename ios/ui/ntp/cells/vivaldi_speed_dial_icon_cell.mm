// Copyright 2025 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ntp/cells/vivaldi_speed_dial_icon_cell.h"

#import "Foundation/Foundation.h"

#import "ios/ui/helpers/vivaldi_colors_helper.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/helpers/vivaldi_uiview_style_helper.h"
#import "ios/ui/ntp/vivaldi_ntp_constants.h"
#import "ios/ui/ntp/vivaldi_speed_dial_constants.h"

namespace {
// Padding for title label. In order - Top, Left, Bottom, Right
const UIEdgeInsets titleLabelPadding = UIEdgeInsetsMake(0.f, 4.f, 0.f, 4.f);
const CGFloat titleLabelBottomPadding = 6.f;

// Padding for favicon fallback label
const UIEdgeInsets faviconFallbackLabelPadding =
    UIEdgeInsetsMake(2.f, 2.f, 2.f, 2.f);

// Favicon padding constants
const UIEdgeInsets faviconPaddingNormal = UIEdgeInsetsMake(6, 6, 6, 6);
const UIEdgeInsets faviconPaddingFolder = UIEdgeInsetsMake(12, 12, 12, 12);

// Preview mask constants
const UIEdgeInsets titleLabelMaskViewPadding =
    UIEdgeInsetsMake(0.f, 8.f, 0.f, 8.f);
const CGFloat titleLabelMaskBottomPadding = 4.f;
const CGFloat titleLabelMaskViewHeight = 7.f;

const CGFloat titleLabelMaskBottomPaddingiPad = 4.f;
const CGFloat titleLabelMaskViewHeightiPad = 8.f;
const CGSize faviconMaskSizeiPad = CGSizeMake(26.f, 26.f);
}  // namespace

@interface VivaldiSpeedDialIconCell ()
// The title label for the speed dial URL item.
@property(nonatomic, weak) UILabel* titleLabel;
// The title label mask for preview purpose
@property(nonatomic, weak) UIView* titleLabelMaskView;
// Imageview for the favicon.
@property(nonatomic, weak) UIImageView* faviconView;
// The fallback label when there's no favicon available.
@property(nonatomic, weak) UILabel* fallbackFaviconLabel;
// Bottom constraint for title label
@property(nonatomic, strong) NSLayoutConstraint* titleLabelBottomConstraint;
// Bottom constraint for title label mask for iPhone
@property(nonatomic, strong) NSLayoutConstraint* titleLabelMaskBottomConstraint;
// Bottom constraint for title label mask for iPad
@property(nonatomic, strong)
    NSLayoutConstraint* titleLabelMaskBottomConstraintiPad;
// Height constraint for title label mask
@property(nonatomic, strong) NSLayoutConstraint* titleLabelMaskHeightConstraint;
// Constraint for favicon width
@property(nonatomic, strong) NSLayoutConstraint* faviconWidthConstraint;
// Constraint for favicon height
@property(nonatomic, strong) NSLayoutConstraint* faviconHeightConstraint;

@property(nonatomic, strong) NSLayoutConstraint* faviconTopConstraint;
@property(nonatomic, strong) NSLayoutConstraint* faviconLeadingConstraint;
@property(nonatomic, strong) NSLayoutConstraint* faviconBottomConstraint;
@property(nonatomic, strong) NSLayoutConstraint* faviconTrailingConstraint;

@end

@implementation VivaldiSpeedDialIconCell

@synthesize titleLabel = _titleLabel;
@synthesize titleLabelMaskView = _titleLabelMaskView;
@synthesize faviconView = _faviconView;
@synthesize fallbackFaviconLabel = _fallbackFaviconLabel;
@synthesize titleLabelBottomConstraint = _titleLabelBottomConstraint;
@synthesize titleLabelMaskBottomConstraint = _titleLabelMaskBottomConstraint;
@synthesize titleLabelMaskBottomConstraintiPad =
    _titleLabelMaskBottomConstraintiPad;
@synthesize titleLabelMaskHeightConstraint = _titleLabelMaskHeightConstraint;
@synthesize faviconWidthConstraint = _faviconWidthConstraint;
@synthesize faviconHeightConstraint = _faviconHeightConstraint;
@synthesize faviconTopConstraint = _faviconTopConstraint;
@synthesize faviconLeadingConstraint = _faviconLeadingConstraint;
@synthesize faviconBottomConstraint = _faviconBottomConstraint;
@synthesize faviconTrailingConstraint = _faviconTrailingConstraint;

#pragma mark - INITIALIZER
- (id)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self setUpUI];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.titleLabel.text = nil;
  self.faviconView.image = nil;
  self.titleLabelMaskView.hidden = YES;
  self.faviconView.backgroundColor = UIColor.clearColor;
  self.fallbackFaviconLabel.hidden = YES;
  self.fallbackFaviconLabel.text = nil;
}

#pragma mark - SET UP UI COMPONENTS
- (void)setUpUI {
  self.layer.cornerRadius = vSpeedDialItemCornerRadius;
  self.clipsToBounds = true;
  self.layer.borderColor = UIColor.clearColor.CGColor;

  // Container view to hold the labels and image views.
  UIView* container = [UIView new];
  container.backgroundColor = UIColor.clearColor;
  container.layer.cornerRadius = vSpeedDialItemCornerRadius;
  container.clipsToBounds = true;
  [self addSubview:container];
  [container fillSuperview];

  // Favicon view container
  UIView* faviconContainerView = [UIView new];
  faviconContainerView.backgroundColor = UIColor.clearColor;
  [container addSubview:faviconContainerView];
  [faviconContainerView anchorTop:container.topAnchor
                          leading:container.leadingAnchor
                           bottom:nil
                         trailing:container.trailingAnchor];

  UIView* faviconBackgroundView = [UIView new];
  faviconBackgroundView.layer.cornerRadius =
      vSpeedDialIconLayoutFaviconCornerRadius;
  faviconBackgroundView.clipsToBounds = true;
  [faviconContainerView addSubview:faviconBackgroundView];
  [faviconBackgroundView centerInSuperview];
  self.faviconWidthConstraint = [faviconBackgroundView.widthAnchor
      constraintEqualToConstant:vSpeedDialItemFaviconSizeIconLayout.width];
  self.faviconHeightConstraint = [faviconBackgroundView.heightAnchor
      constraintEqualToConstant:vSpeedDialItemFaviconSizeIconLayout.height];
  [self.faviconWidthConstraint setActive:YES];
  [self.faviconHeightConstraint setActive:YES];
  [faviconBackgroundView
      addShadowWithBackground:[UIColor
                                  colorNamed:vNTPSpeedDialCellBackgroundColor]
                       offset:vSpeedDialItemShadowOffset
                  shadowColor:[UIColor colorNamed:vSpeedDialItemShadowColor]
                       radius:vSpeedDialItemShadowRadius
                      opacity:vSpeedDialItemShadowOpacity];
  faviconBackgroundView.backgroundColor =
      [UIColor colorNamed:vNTPSpeedDialCellBackgroundColor];

  // Favicon view
  UIImageView* faviconView = [[UIImageView alloc] initWithImage:nil];
  faviconView.translatesAutoresizingMaskIntoConstraints = NO;
  _faviconView = faviconView;
  _faviconView.contentMode = UIViewContentModeScaleAspectFit;
  _faviconView.backgroundColor = UIColor.clearColor;
  _faviconView.layer.cornerRadius = vSpeedDialFaviconCornerRadius;
  _faviconView.clipsToBounds = true;
  [faviconBackgroundView addSubview:_faviconView];

  self.faviconTopConstraint = [_faviconView.topAnchor
      constraintEqualToAnchor:faviconBackgroundView.topAnchor
                     constant:faviconPaddingNormal.top];
  self.faviconLeadingConstraint = [_faviconView.leadingAnchor
      constraintEqualToAnchor:faviconBackgroundView.leadingAnchor
                     constant:faviconPaddingNormal.left];
  self.faviconBottomConstraint = [_faviconView.bottomAnchor
      constraintEqualToAnchor:faviconBackgroundView.bottomAnchor
                     constant:-faviconPaddingNormal.bottom];
  self.faviconTrailingConstraint = [_faviconView.trailingAnchor
      constraintEqualToAnchor:faviconBackgroundView.trailingAnchor
                     constant:-faviconPaddingNormal.right];

  [NSLayoutConstraint activateConstraints:@[
    self.faviconTopConstraint, self.faviconLeadingConstraint,
    self.faviconBottomConstraint, self.faviconTrailingConstraint
  ]];

  // Fallback favicon label
  UILabel* fallbackFaviconLabel = [[UILabel alloc] init];
  _fallbackFaviconLabel = fallbackFaviconLabel;
  fallbackFaviconLabel.textColor =
      [UIColor colorNamed:vNTPSpeedDialDomainTextColor];
  fallbackFaviconLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  fallbackFaviconLabel.numberOfLines = 0;
  fallbackFaviconLabel.textAlignment = NSTextAlignmentCenter;

  [faviconContainerView addSubview:fallbackFaviconLabel];
  [fallbackFaviconLabel matchToView:_faviconView
                            padding:faviconFallbackLabelPadding];
  fallbackFaviconLabel.hidden = YES;

  // Website title label
  UILabel* titleLabel = [[UILabel alloc] init];
  _titleLabel = titleLabel;
  titleLabel.textColor = [UIColor colorNamed:vNTPSpeedDialDomainTextColor];
  titleLabel.numberOfLines = 1;
  titleLabel.textAlignment = NSTextAlignmentCenter;

  [container addSubview:_titleLabel];
  [_titleLabel anchorTop:faviconContainerView.bottomAnchor
                 leading:container.leadingAnchor
                  bottom:nil
                trailing:container.trailingAnchor
                 padding:titleLabelPadding];
  self.titleLabelBottomConstraint = [titleLabel.bottomAnchor
      constraintEqualToAnchor:container.bottomAnchor
                     constant:-titleLabelBottomPadding];

  // Title label mask, used for preview only
  UIView* titleLabelMaskView = [UIView new];
  _titleLabelMaskView = titleLabelMaskView;
  titleLabelMaskView.backgroundColor = UIColor.clearColor;
  titleLabelMaskView.layer.cornerRadius = vSpeedDialFaviconCornerRadius;
  titleLabelMaskView.clipsToBounds = true;

  [container addSubview:titleLabelMaskView];
  [titleLabelMaskView anchorTop:faviconContainerView.bottomAnchor
                        leading:container.leadingAnchor
                         bottom:nil
                       trailing:container.trailingAnchor
                        padding:titleLabelMaskViewPadding];
  titleLabelMaskView.hidden = YES;

  // Height constraint for preview mask.
  self.titleLabelMaskHeightConstraint = [titleLabelMaskView.heightAnchor
      constraintEqualToConstant:titleLabelMaskViewHeight];
  [self.titleLabelMaskHeightConstraint setActive:YES];

  // Bottom constraints for iPhone and iPad preview mask.
  self.titleLabelMaskBottomConstraint = [titleLabelMaskView.bottomAnchor
      constraintEqualToAnchor:container.bottomAnchor
                     constant:-titleLabelMaskBottomPadding];

  self.titleLabelMaskBottomConstraintiPad = [titleLabelMaskView.bottomAnchor
      constraintEqualToAnchor:container.bottomAnchor
                     constant:-titleLabelMaskBottomPaddingiPad];
}

#pragma mark - SETTERS

- (void)configureCellWith:(VivaldiSpeedDialItem*)item
                 isTablet:(BOOL)isTablet
                 isFolder:(BOOL)isFolder
                wallpaper:(UIImage*)wallpaper {
  self.titleLabel.text = item.title;

  if (wallpaper) {
    BOOL useDarkText =
        [VivaldiGlobalHelpers shouldUseDarkTextForImage:wallpaper];
    UIColor* textColor =
        useDarkText ? [UIColor colorNamed:vNTPSpeedDialDomainTextColorDark] :
            [UIColor colorNamed:vNTPSpeedDialDomainTextColorLight];
    self.titleLabel.textColor = textColor;
  } else {
    self.titleLabel.textColor =
        [UIColor colorNamed:vNTPSpeedDialDomainTextColor];
  }

  // Set favicon padding based on folder state
  UIEdgeInsets padding = isFolder ? faviconPaddingFolder : faviconPaddingNormal;
  self.faviconTopConstraint.constant = padding.top;
  self.faviconLeadingConstraint.constant = padding.left;
  self.faviconBottomConstraint.constant = -padding.bottom;
  self.faviconTrailingConstraint.constant = -padding.right;

  if (isTablet) {
    self.faviconWidthConstraint.constant =
        vSpeedDialItemFaviconSizeIconLayoutTablet.width;
    self.faviconHeightConstraint.constant =
        vSpeedDialItemFaviconSizeIconLayoutTablet.height;
    self.titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleCaption2];
  } else {
    self.faviconWidthConstraint.constant =
        vSpeedDialItemFaviconSizeIconLayout.width;
    self.faviconHeightConstraint.constant =
        vSpeedDialItemFaviconSizeIconLayout.height;
    self.titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
  }

  _fallbackFaviconLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];

  // Activate title label constraint and deactivate mask.
  [self.titleLabelBottomConstraint setActive:YES];
  [self.titleLabelMaskBottomConstraint setActive:NO];
  [self.titleLabelMaskBottomConstraintiPad setActive:NO];

  if (isFolder) {
    self.fallbackFaviconLabel.hidden = YES;
    self.fallbackFaviconLabel.text = nil;
    self.faviconView.backgroundColor = UIColor.clearColor;
    self.faviconView.image = [UIImage imageNamed:vNTPSpeedDialFolderIcon];
  }
}

- (void)configureCellWithAttributes:(FaviconAttributes*)attributes
                               item:(VivaldiSpeedDialItem*)item {
  if (attributes.faviconImage) {
    self.fallbackFaviconLabel.hidden = YES;
    self.fallbackFaviconLabel.text = nil;
    self.faviconView.backgroundColor = UIColor.clearColor;
    self.faviconView.image = attributes.faviconImage;
  } else {
    [self showFallbackFavicon:item];
  }
}

- (void)configurePreviewForDevice:(BOOL)isTablet {
  if (isTablet) {
    // Activate title label mask constraint and deactivate actual label.
    [self.titleLabelBottomConstraint setActive:NO];
    [self.titleLabelMaskBottomConstraint setActive:NO];
    [self.titleLabelMaskBottomConstraintiPad setActive:YES];
    self.faviconWidthConstraint.constant = faviconMaskSizeiPad.width;
    self.faviconHeightConstraint.constant = faviconMaskSizeiPad.height;
    self.titleLabelMaskHeightConstraint.constant = titleLabelMaskViewHeightiPad;
  } else {
    // Activate title label mask constraint and deactivate actual label.
    [self.titleLabelBottomConstraint setActive:NO];
    [self.titleLabelMaskBottomConstraint setActive:YES];
    [self.titleLabelMaskBottomConstraintiPad setActive:NO];
    self.faviconWidthConstraint.constant =
        vSpeedDialItemFaviconSizeIconLayout.width;
    self.faviconHeightConstraint.constant =
        vSpeedDialItemFaviconSizeIconLayout.height;
    self.titleLabelMaskHeightConstraint.constant = titleLabelMaskViewHeight;
  }

  self.faviconView.backgroundColor = UIColor.vSystemGray03;
  self.titleLabelMaskView.hidden = NO;
  self.titleLabelMaskView.backgroundColor =
      [UIColor.vSystemGray03 colorWithAlphaComponent:0.4];
}

#pragma mark : PRIVATE

- (void)showFallbackFavicon:(VivaldiSpeedDialItem*)sdItem {
  if (sdItem.isInternalPage) {
    self.faviconView.backgroundColor = UIColor.clearColor;
    self.fallbackFaviconLabel.hidden = YES;
    self.faviconView.image = [UIImage imageNamed:vNTPSDInternalPageFavicon];
  } else {
    self.fallbackFaviconLabel.hidden = NO;
    self.faviconView.image = nil;
    self.faviconView.backgroundColor =
        [UIColor colorNamed:vSearchbarBackgroundColor];
    NSString* firstLetter = [[sdItem.host substringToIndex:1] uppercaseString];
    self.fallbackFaviconLabel.text = firstLetter;
  }
}

@end
