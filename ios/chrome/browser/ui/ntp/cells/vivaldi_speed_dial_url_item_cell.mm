// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/chrome/browser/ui/ntp/cells/vivaldi_speed_dial_url_item_cell.h"

#include "Foundation/Foundation.h"

#import "ios/chrome/browser/ui/ntp/vivaldi_ntp_constants.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/helpers/vivaldi_uiview_style_helper.h"

namespace {

// Padding for thumbnail. In order - Top, Left, Bottom, Right
const UIEdgeInsets thumbnailPadding =
    UIEdgeInsetsMake(4.f, 4.f, 0.f, 4.f);
// Padding for favicon. In order - Top, Left, Bottom, Right
const UIEdgeInsets faviconPadding =
    UIEdgeInsetsMake(12.f, 12.f, 12.f, 0.0);
// Padding for title label. In order - Top, Left, Bottom, Right
const UIEdgeInsets titleLabelPadding =
    UIEdgeInsetsMake(0.f, 12.f, 0.f, 12.f);
}

@interface VivaldiSpeedDialURLItemCell()
// The title label for the speed dial URL item.
@property(nonatomic,weak) UILabel* titleLabel;
// The fallback label when there's no thumbnail available for the speed dial
// URL item.
@property(nonatomic,weak) UILabel* fallbackTitleLabel;
// Imageview for the thumbnail.
@property(nonatomic,weak) UIImageView* thumbView;
// Imageview for the favicon.
@property(nonatomic,weak) UIImageView* faviconView;
@end

@implementation VivaldiSpeedDialURLItemCell

@synthesize titleLabel = _titleLabel;
@synthesize fallbackTitleLabel = _fallbackTitleLabel;
@synthesize thumbView = _thumbView;
@synthesize faviconView = _faviconView;

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
  self.fallbackTitleLabel.text = nil;
  self.faviconView.image = nil;
  self.thumbView.image = nil;
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
  fallbackTitleLabel.font = [UIFont systemFontOfSize:vHeaderFontSize];
  fallbackTitleLabel.numberOfLines = 0;
  fallbackTitleLabel.textAlignment = NSTextAlignmentCenter;

  [container addSubview:_fallbackTitleLabel];
  [_fallbackTitleLabel matchToView:_thumbView];

  // Favicon view
  UIImageView* faviconView = [[UIImageView alloc] initWithImage:nil];
  _faviconView = faviconView;
  _faviconView.contentMode = UIViewContentModeScaleAspectFit;
  _faviconView.backgroundColor = UIColor.clearColor;
  [container addSubview: _faviconView];
  [_faviconView anchorTop: thumbView.bottomAnchor
                  leading: container.leadingAnchor
                   bottom: container.bottomAnchor
                 trailing: nil
                  padding: faviconPadding
                     size: vSpeedDialItemFaviconSize];

  // Website title label
  UILabel* titleLabel = [[UILabel alloc] init];
  _titleLabel = titleLabel;
  titleLabel.textColor = [UIColor colorNamed:vNTPSpeedDialDomainTextColor];
  titleLabel.font = [UIFont systemFontOfSize:vBody1FontSize];
  titleLabel.numberOfLines = 1;
  titleLabel.textAlignment = NSTextAlignmentLeft;

  [container addSubview:_titleLabel];
  [_titleLabel anchorTop: faviconView.topAnchor
                 leading: faviconView.trailingAnchor
                  bottom: faviconView.bottomAnchor
                trailing: container.trailingAnchor
                 padding: titleLabelPadding];
}


#pragma mark - SETTERS

- (void)configureCellWith:(VivaldiSpeedDialItem*)item {
  self.titleLabel.text = item.title;

  // TODO: @prio@vivaldi.com - Handle the fall back properly
  // This is temporary and should be replaced once we implement the thumbnail
  // fetching and storing.
  self.thumbView.backgroundColor =
    [UIColor colorNamed: vSearchbarBackgroundColor];
  self.fallbackTitleLabel.hidden = false;
  self.fallbackTitleLabel.text = item.title;
}

- (void)configureCellWithAttributes:(FaviconAttributes*)attributes {
  if (!attributes) {
    self.faviconView.backgroundColor = UIColor.grayColor;
    return;
  }

  if (attributes.faviconImage) {
    self.faviconView.image = attributes.faviconImage;
  } else {
    // Do something for fallback
  }
}

@end
