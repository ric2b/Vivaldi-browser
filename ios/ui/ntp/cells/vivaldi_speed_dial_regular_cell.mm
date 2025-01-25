// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ntp/cells/vivaldi_speed_dial_regular_cell.h"

#import "ios/ui/helpers/vivaldi_colors_helper.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/helpers/vivaldi_uiview_style_helper.h"
#import "ios/ui/ntp/vivaldi_ntp_constants.h"
#import "ios/ui/ntp/vivaldi_speed_dial_constants.h"
#import "ios/ui/thumbnail/vivaldi_thumbnail_service.h"

namespace {

// Padding for thumbnail. In order - Top, Left, Bottom, Right
const UIEdgeInsets thumbnailPadding =
    UIEdgeInsetsMake(4.f, 4.f, 0.f, 4.f);
// Padding for favicon. In order - Top, Left, Bottom, Right
const UIEdgeInsets faviconPadding =
    UIEdgeInsetsMake(8.f, 8.f, 8.f, 0.0);
// Padding for favicon fallback label
const UIEdgeInsets faviconFallbackLabelPadding =
    UIEdgeInsetsMake(2.f, 2.f, 2.f, 2.f);
// Padding for title label. In order - Top, Left, Bottom, Right
const UIEdgeInsets titleLabelPadding =
    UIEdgeInsetsMake(0.f, 8.f, 0.f, 8.f);
const UIEdgeInsets titleLabelMaskPadding =
    UIEdgeInsetsMake(2.f, 10.f, 2.f, 16.f);

const UIEdgeInsets activityIndicatorPadding =
    UIEdgeInsetsMake(8.f, 8.f, 0.f, 0.f);
const CGSize activityIndicatorSize = CGSizeMake(24.f, 24.f);
const CGFloat activityIndicatorRadius = 4.f;
const CGFloat activityIndicatorBGOpacity = 0.8;

const CGFloat thumbnailCropYOffset = 14.f;
}

@interface VivaldiSpeedDialRegularCell()
// The title label for the speed dial URL item.
@property(nonatomic,weak) UILabel* titleLabel;
// The title label mask for preview purpose
@property(nonatomic,weak) UIView* titleLabelMaskView;
// The fallback label when there's no thumbnail available for the speed dial
// URL item.
@property(nonatomic,weak) UILabel* fallbackTitleLabel;
// Imageview for the thumbnail.
@property(nonatomic,weak) UIImageView* thumbView;
// Imageview for the favicon.
@property(nonatomic,weak) UIImageView* faviconView;
// The fallback label when there's no favicon available.
@property(nonatomic,weak) UILabel* fallbackFaviconLabel;
// Activity indicator visible when thumbnail is updating.
@property(nonatomic,weak) UIActivityIndicatorView* activityIndicator;
// Property to hold the resource path
@property(nonatomic,strong) NSString* resourcePath;
@end

@implementation VivaldiSpeedDialRegularCell

@synthesize titleLabel = _titleLabel;
@synthesize titleLabelMaskView = _titleLabelMaskView;
@synthesize fallbackTitleLabel = _fallbackTitleLabel;
@synthesize thumbView = _thumbView;
@synthesize faviconView = _faviconView;
@synthesize fallbackFaviconLabel = _fallbackFaviconLabel;
@synthesize activityIndicator = _activityIndicator;
@synthesize resourcePath = _resourcePath;

#pragma mark - INITIALIZER
- (id)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _resourcePath = [NSString stringWithFormat:@"%@%@",
                        [[NSBundle mainBundle] resourcePath],
                        @"/res"];
    [self setUpUI];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.titleLabel.text = nil;
  self.fallbackTitleLabel.text = nil;
  self.faviconView.image = nil;
  self.faviconView.backgroundColor = UIColor.clearColor;
  self.thumbView.image = nil;
  self.titleLabelMaskView.hidden = YES;
  self.thumbView.backgroundColor = UIColor.clearColor;
  self.fallbackTitleLabel.hidden = YES;
  self.fallbackFaviconLabel.hidden = YES;
  self.fallbackFaviconLabel.text = nil;
}


#pragma mark - SET UP UI COMPONENTS
- (void)setUpUI {

  self.layer.cornerRadius = vSpeedDialItemCornerRadius;
  self.clipsToBounds = true;
  self.layer.borderColor = UIColor.clearColor.CGColor;
  // Add drop shadow to parent
  [self
    addShadowWithBackground: [UIColor colorNamed:
                              vNTPSpeedDialCellBackgroundColor]
                     offset: vSpeedDialItemShadowOffset
                shadowColor: [UIColor colorNamed:vSpeedDialItemShadowColor]
                     radius: vSpeedDialItemShadowRadius
                    opacity: vSpeedDialItemShadowOpacity
  ];
  self.backgroundColor =
    [UIColor colorNamed: vNTPSpeedDialCellBackgroundColor];

  // Container view to hold the labels and image views.
  UIView *container = [UIView new];
  container.backgroundColor =
    [UIColor colorNamed: vNTPSpeedDialCellBackgroundColor];
  container.layer.cornerRadius = vSpeedDialItemCornerRadius;
  container.clipsToBounds = true;
  [self addSubview:container];
  [container fillSuperview];

  // Thumbnail view
  UIImageView* thumbView = [[UIImageView alloc] initWithImage:nil];
  _thumbView = thumbView;
  _thumbView.contentMode = UIViewContentModeScaleAspectFill;
  _thumbView.backgroundColor = UIColor.clearColor;
  _thumbView.layer.cornerRadius = vSpeedDialFaviconCornerRadius;
  _thumbView.clipsToBounds = true;
  [container addSubview: _thumbView];
  [_thumbView anchorTop: container.topAnchor
                leading: container.leadingAnchor
                 bottom: nil
               trailing: container.trailingAnchor
                padding: thumbnailPadding];

  // Fallback title label
  UILabel* fallbackTitleLabel = [[UILabel alloc] init];
  _fallbackTitleLabel = fallbackTitleLabel;
  fallbackTitleLabel.textColor =
    [UIColor colorNamed:vNTPSpeedDialDomainTextColor];
  fallbackTitleLabel.font =
    [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  fallbackTitleLabel.numberOfLines = 0;
  fallbackTitleLabel.textAlignment = NSTextAlignmentCenter;

  [container addSubview:_fallbackTitleLabel];
  [_fallbackTitleLabel matchToView:_thumbView
                           padding:titleLabelPadding];

  // Loading spinner
  UIActivityIndicatorView* indicator =
      [[UIActivityIndicatorView alloc] initWithFrame:CGRectZero];
  _activityIndicator = indicator;
  indicator.color = UIColor.whiteColor;
  indicator.backgroundColor =
      [[UIColor blackColor] colorWithAlphaComponent:activityIndicatorBGOpacity];
  indicator.hidesWhenStopped = YES;
  indicator.layer.cornerRadius = activityIndicatorRadius;
  indicator.activityIndicatorViewStyle = UIActivityIndicatorViewStyleMedium;

  [container addSubview:indicator];
  [indicator anchorTop:container.topAnchor
               leading:container.leadingAnchor
                bottom:nil
              trailing:nil
               padding:activityIndicatorPadding
                  size:activityIndicatorSize];

  // Favicon view
  UIImageView* faviconView = [[UIImageView alloc] initWithImage:nil];
  _faviconView = faviconView;
  _faviconView.contentMode = UIViewContentModeScaleAspectFit;
  _faviconView.backgroundColor = UIColor.clearColor;
  _faviconView.layer.cornerRadius = vSpeedDialFaviconCornerRadius;
  _faviconView.clipsToBounds = true;
  [container addSubview: _faviconView];
  [_faviconView anchorTop: thumbView.bottomAnchor
                  leading: container.leadingAnchor
                   bottom: container.bottomAnchor
                 trailing: nil
                  padding: faviconPadding
                     size: vSpeedDialItemFaviconSizeRegularLayout];

  // Fallback favicon label
  UILabel* fallbackFaviconLabel = [[UILabel alloc] init];
  _fallbackFaviconLabel = fallbackFaviconLabel;
  fallbackFaviconLabel.textColor =
    [UIColor colorNamed:vNTPSpeedDialDomainTextColor];
  fallbackFaviconLabel.font =
    [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  fallbackFaviconLabel.numberOfLines = 0;
  fallbackFaviconLabel.textAlignment = NSTextAlignmentCenter;

  [container addSubview:fallbackFaviconLabel];
  [fallbackFaviconLabel matchToView:_faviconView
                            padding:faviconFallbackLabelPadding];
  fallbackFaviconLabel.hidden = YES;

  // Website title label
  UILabel* titleLabel = [[UILabel alloc] init];
  _titleLabel = titleLabel;
  titleLabel.textColor = [UIColor colorNamed:vNTPSpeedDialDomainTextColor];
  titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  titleLabel.numberOfLines = 1;
  titleLabel.textAlignment = NSTextAlignmentLeft;

  [container addSubview:_titleLabel];
  [_titleLabel anchorTop: nil
                 leading: faviconView.trailingAnchor
                  bottom: nil
                trailing: container.trailingAnchor
                 padding: titleLabelPadding];
  [titleLabel centerYToView:faviconView];

  // Title label mask, used for preview only
  UIView* titleLabelMaskView = [UIView new];
  _titleLabelMaskView = titleLabelMaskView;
  titleLabelMaskView.backgroundColor = UIColor.clearColor;
  titleLabelMaskView.layer.cornerRadius = vSpeedDialFaviconCornerRadius;
  titleLabelMaskView.clipsToBounds = true;

  [container addSubview:titleLabelMaskView];
  [titleLabelMaskView anchorTop: faviconView.topAnchor
                        leading: faviconView.trailingAnchor
                         bottom: faviconView.bottomAnchor
                       trailing: container.trailingAnchor
                        padding: titleLabelMaskPadding];
  titleLabelMaskView.hidden = YES;
}


#pragma mark - SETTERS

- (void)configureCellWith:(VivaldiSpeedDialItem*)item
              layoutStyle:(VivaldiStartPageLayoutStyle)style {
  self.titleLabel.text = item.title;

  switch (style) {
    case VivaldiStartPageLayoutStyleMedium:
      _titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
      _fallbackFaviconLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
      break;
    case VivaldiStartPageLayoutStyleLarge:
      _titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
      _fallbackFaviconLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
      break;
    default:
      break;
  }

  if (item.isFrequentlyVisited) {
    self.thumbView.backgroundColor =
        [UIColor colorNamed: vSearchbarBackgroundColor];
    self.fallbackTitleLabel.hidden = NO;
    self.fallbackTitleLabel.text = item.host;
  } else {
    // Thumbnail: Initially we will check check whether the speed dial has a
    // thumbnail in bundle. If not available we will see if application support
    // directory has any stored for the item. If both case fails we will go
    // fallback and show the item title instead.
    NSString* bundlePath =
        [_resourcePath stringByAppendingPathComponent:item.thumbnail];
    UIImage *thumbnailBundle =
        [[UIImage alloc] initWithContentsOfFile:bundlePath];

    if (thumbnailBundle) {
      [self.thumbView setImage: thumbnailBundle];
    } else {
      UIImage* thumbnailLocal =
          [[[VivaldiThumbnailService alloc] init] thumbnailForSDItem:item];
      if (thumbnailLocal) {
        UIImage* thumbnailImage =
            [self cropTopAndResizeImage:thumbnailLocal
                                 toSize:self.bounds.size];
        [self.thumbView setImage: thumbnailImage];
      } else {
        self.thumbView.backgroundColor =
            [UIColor colorNamed: vSearchbarBackgroundColor];
        self.fallbackTitleLabel.hidden = NO;

        if ([item.title length] > 0) {
          self.fallbackTitleLabel.text = item.title;
        } else {
          self.fallbackTitleLabel.text = item.host;
        }
      }
    }
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

- (void)setActivityIndicatorLoading:(BOOL)isLoading {
  if (isLoading) {
    [self.activityIndicator startAnimating];
  } else {
    [self.activityIndicator stopAnimating];
  }
}

- (void)configurePreview {
  self.thumbView.backgroundColor = UIColor.vSystemGray03;
  self.faviconView.backgroundColor = UIColor.vSystemGray03;
  self.titleLabelMaskView.hidden = NO;
  self.titleLabelMaskView.backgroundColor =
    [UIColor.vSystemGray03 colorWithAlphaComponent:0.4];
}

#pragma mark: PRIVATE

- (void)showFallbackFavicon:(VivaldiSpeedDialItem*)sdItem {
  if (sdItem.isInternalPage) {
    self.faviconView.backgroundColor = UIColor.clearColor;
    self.fallbackFaviconLabel.hidden = YES;
    self.faviconView.image = [UIImage imageNamed:vNTPSDInternalPageFavicon];
  } else {
    self.fallbackFaviconLabel.hidden = NO;
    self.faviconView.image = nil;
    self.faviconView.backgroundColor =
        [UIColor colorNamed: vSearchbarBackgroundColor];
    NSString *firstLetter = [[sdItem.host substringToIndex:1] uppercaseString];
    self.fallbackFaviconLabel.text = firstLetter;
  }
}

- (UIImage*)cropTopAndResizeImage:(UIImage*)image
                           toSize:(CGSize)newSize {
  // Determine the aspect ratio to scale the height appropriately
  CGFloat aspectWidth = newSize.width / image.size.width;
  CGFloat scaledHeight = image.size.height * aspectWidth;
  CGFloat offsetY = thumbnailCropYOffset;
  // Start drawing in a context
  UIGraphicsBeginImageContextWithOptions(newSize, NO, 0.0);
  // Create a rect for the scaled image, starting from the top
  CGRect drawRect = CGRectMake(0, offsetY, newSize.width, scaledHeight);
  // Draw the image in the new rect, effectively cropping the top part and
  // scaling it
  [image drawInRect:drawRect];
  // Capture the new image
  UIImage *newImage = UIGraphicsGetImageFromCurrentImageContext();
  // End the context
  UIGraphicsEndImageContext();
  return newImage;
}

@end
