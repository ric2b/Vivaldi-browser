// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ntp/cells/vivaldi_speed_dial_folder_regular_cell.h"

#import "ios/chrome/grit/ios_strings.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/helpers/vivaldi_uiview_style_helper.h"
#import "ios/ui/ntp/vivaldi_ntp_constants.h"
#import "ios/ui/ntp/vivaldi_speed_dial_constants.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

namespace {
// Padding for title label. In order - Top, Left, Bottom, Right
const UIEdgeInsets titleLabelPadding =
    UIEdgeInsetsMake(0.f, 8.f, 8.f, 8.f);
}

@interface VivaldiSpeedDialFolderRegularCell()
// Title label for the folder item
@property(nonatomic,weak) UILabel* titleLabel;
// Icon for the folder item
@property(nonatomic,weak) UIImageView* iconView;
// Constraint for folder icon width
@property(nonatomic,strong) NSLayoutConstraint* iconWidthConstraint;
// Constraint for folder icon height
@property(nonatomic,strong) NSLayoutConstraint* iconHeightConstraint;
@end

@implementation VivaldiSpeedDialFolderRegularCell

@synthesize titleLabel = _titleLabel;
@synthesize iconView = _iconView;
@synthesize iconWidthConstraint = _iconWidthConstraint;
@synthesize iconHeightConstraint = _iconHeightConstraint;

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
  iconView.contentMode = UIViewContentModeScaleAspectFit;
  iconView.backgroundColor = UIColor.clearColor;
  [iconContainerView addSubview: iconView];
  [iconView centerInSuperview];

  self.iconWidthConstraint =
    [iconView.widthAnchor
     constraintEqualToConstant:vSpeedDialFolderIconSizeMedium.width];
  self.iconHeightConstraint =
    [iconView.heightAnchor
     constraintEqualToConstant:vSpeedDialFolderIconSizeMedium.height];
  [self.iconWidthConstraint setActive:YES];
  [self.iconHeightConstraint setActive:YES];

  // Folder name label
  UILabel* label = [[UILabel alloc] init];
  _titleLabel = label;
  label.textColor = [UIColor colorNamed:vNTPSpeedDialDomainTextColor];
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
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
                   addNew:(bool)addNew
              layoutStyle:(VivaldiStartPageLayoutStyle)style {

  switch (style) {
    case VivaldiStartPageLayoutStyleSmall:
      _titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
      self.iconHeightConstraint.constant = vSpeedDialFolderIconSizeSmall.height;
      self.iconWidthConstraint.constant = vSpeedDialFolderIconSizeSmall.width;
      break;
    case VivaldiStartPageLayoutStyleMedium:
      _titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
      self.iconHeightConstraint.constant = vSpeedDialFolderIconSizeMedium.height;
      self.iconWidthConstraint.constant = vSpeedDialFolderIconSizeMedium.width;
      break;
    case VivaldiStartPageLayoutStyleLarge:
      _titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
      self.iconHeightConstraint.constant =
          vSpeedDialFolderIconSizeRegular.height;
      self.iconWidthConstraint.constant = vSpeedDialFolderIconSizeRegular.width;
      break;
    default:
      break;
  }

  if (addNew) {
    self.iconView.image = [UIImage imageNamed: vNTPAddNewSpeedDialIcon];
    NSString* newSpeedDialTitle =
      (style == VivaldiStartPageLayoutStyleSmall) ?
      l10n_util::GetNSString(IDS_IOS_NEW_SPEED_DIAL_SMALL) :
      l10n_util::GetNSString(IDS_IOS_NEW_SPEED_DIAL);
    self.titleLabel.text = newSpeedDialTitle;
  } else {
    self.iconView.image = [UIImage imageNamed:vNTPSpeedDialFolderIcon];
    self.titleLabel.text = item.title;
  }
}

@end
