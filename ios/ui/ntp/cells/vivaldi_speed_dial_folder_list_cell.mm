// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ntp/cells/vivaldi_speed_dial_folder_list_cell.h"

#import "ios/chrome/grit/ios_strings.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/helpers/vivaldi_uiview_style_helper.h"
#import "ios/ui/ntp/vivaldi_ntp_constants.h"
#import "ios/ui/ntp/vivaldi_speed_dial_constants.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

namespace {
// Padding for title label. In order - Top, Left, Bottom, Right
// Padding for labels. In order - Top, Left, Bottom, Right
const UIEdgeInsets faviconPadding =
    UIEdgeInsetsMake(0.f, 14.f, 0.f, 0.f);
const UIEdgeInsets titleLabelPadding =
    UIEdgeInsetsMake(0.f, 12.f, 0.f, 8.f);
}

@interface VivaldiSpeedDialFolderListCell()
// Title label for the folder item
@property(nonatomic,weak) UILabel* titleLabel;
// Icon for the folder item
@property(nonatomic,weak) UIImageView* iconView;
@end

@implementation VivaldiSpeedDialFolderListCell

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
  self.iconView.image = nil;
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

  // Icon view
  UIImageView* iconView = [[UIImageView alloc] initWithImage:nil];
  _iconView = iconView;
  iconView.contentMode = UIViewContentModeScaleAspectFit;
  iconView.backgroundColor = UIColor.clearColor;
  [container addSubview: iconView];
  [iconView anchorTop: nil
              leading: container.leadingAnchor
               bottom: nil
             trailing: nil
              padding: faviconPadding
                 size: vSpeedDialFolderIconSizeList];
  [iconView centerYInSuperview];

  // Folder name label
  UILabel* titleLabel = [UILabel new];
  _titleLabel = titleLabel;
  titleLabel.textColor = [UIColor colorNamed:vNTPSpeedDialDomainTextColor];
  titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  titleLabel.numberOfLines = 1;
  titleLabel.textAlignment = NSTextAlignmentLeft;

  [container addSubview: titleLabel];
  [titleLabel anchorTop: nil
                leading: iconView.trailingAnchor
                 bottom: nil
               trailing: container.trailingAnchor
                padding: titleLabelPadding];
  [titleLabel centerYInSuperview];
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
