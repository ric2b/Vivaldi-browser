// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/chrome/browser/ui/ntp/cells/vivaldi_speed_dial_folder_item_cell.h"

#import "ios/chrome/browser/ui/ntp/vivaldi_ntp_constants.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/helpers/vivaldi_uiview_style_helper.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {
// Padding for title label. In order - Top, Left, Bottom, Right
const UIEdgeInsets titleLabelPadding =
    UIEdgeInsetsMake(0.f, 12.f, 12.f, 12.f);
}

@interface VivaldiSpeedDialFolderItemCell()
// Title label for the folder item
@property(nonatomic,weak) UILabel* titleLabel;
// Icon for the folder item
@property(nonatomic,weak) UIImageView* iconView;
@end

@implementation VivaldiSpeedDialFolderItemCell

@synthesize titleLabel = _titleLabel;
@synthesize iconView = _iconView;

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

  // Container to hold the title and icon
  UIView *container = [UIView new];
  container.layer.cornerRadius = vSpeedDialItemCornerRadius;
  container.backgroundColor =
    [UIColor colorNamed: vNTPSpeedDialCellBackgroundColor];
  container.clipsToBounds = true;
  [self addSubview:container];
  [container fillSuperview];

  // Icon view container
  UIView* iconContainerView = [UIView new];
  iconContainerView.backgroundColor = UIColor.clearColor;
  [container addSubview: iconContainerView];
  [iconContainerView anchorTop: container.topAnchor
                       leading: container.leadingAnchor
                        bottom: nil
                      trailing: container.trailingAnchor];

  UIImageView* iconView = [[UIImageView alloc] initWithImage:nil];
  _iconView = iconView;
  _iconView.contentMode = UIViewContentModeScaleAspectFit;
  _iconView.backgroundColor = UIColor.clearColor;
  [iconContainerView addSubview: _iconView];
  [_iconView centerInSuperviewWithSize: vSpeedDialFolderIconSize];

  // Folder name label
  UILabel* label = [[UILabel alloc] init];
  _titleLabel = label;
  label.textColor = [UIColor colorNamed:vNTPSpeedDialDomainTextColor];
  label.font = [UIFont systemFontOfSize:vBody1FontSize];
  label.numberOfLines = 1;
  label.textAlignment = NSTextAlignmentCenter;

  [container addSubview:_titleLabel];
  [_titleLabel anchorTop: iconContainerView.bottomAnchor
                 leading: container.leadingAnchor
                  bottom: container.bottomAnchor
                trailing: container.trailingAnchor
                 padding: titleLabelPadding];
}

#pragma mark - SETTERS
- (void)configureCellWith:(VivaldiSpeedDialItem*)item
                   addNew:(bool)addNew {
  if (addNew) {
    self.iconView.image = [UIImage imageNamed: vNTPAddNewSpeedDialIcon];
    NSString* newSpeedDialTitle =
      l10n_util::GetNSString(IDS_IOS_NEW_SPEED_DIAL);
    self.titleLabel.text = newSpeedDialTitle;
  } else {
    self.iconView.image = [UIImage imageNamed:vNTPSpeedDialFolderIcon];
    self.titleLabel.text = item.title;
  }
}

@end
