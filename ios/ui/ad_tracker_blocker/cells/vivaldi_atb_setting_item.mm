// Copyright 2022 Vivaldi Technologies AS. All rights reserved.

#import "ios/ui/ad_tracker_blocker/cells/vivaldi_atb_setting_item.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_constants.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Padding for the stack view.
// In order - Top, Left, Bottom, Right
const UIEdgeInsets stackPadding = UIEdgeInsetsMake(12.f, 12.f, 12.f, 0.f);
const CGSize imageViewSize = CGSizeMake(32, 32);
const UIEdgeInsets imageViewPadding = UIEdgeInsetsMake(12.f, 0.f, 0.f, 0.f);
} // End Namespace


@implementation VivaldiATBSettingItem

- (instancetype)initWithType:(NSInteger)type {
  if ((self = [super initWithType:type])) {
    self.cellClass = [VivaldiATBSettingItemCell class];
    self.useCustomSeparator = YES;
  }
  return self;
}

- (void)configureCell:(VivaldiATBSettingItemCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  [cell configurWithItem:self.item
     userPreferredOption:self.userPreferredOption
     globalDefaultOption:self.globalDefaultOption
       showDefaultMarker:self.showDefaultMarker];
}

@end

#pragma mark - VivaldiATBSettingItemCell
@interface VivaldiATBSettingItemCell()
// Title of the settings
@property (weak, nonatomic) UILabel* titleLabel;
// Description of the settings
@property (weak, nonatomic) UILabel* subtitleLabel;
// Imageview for the settings icon
@property (weak, nonatomic) UIImageView* imageView;
@end

@implementation VivaldiATBSettingItemCell

@synthesize titleLabel = _titleLabel;
@synthesize subtitleLabel = _subtitleLabel;
@synthesize imageView = _imageView;

#pragma mark - INITIALIZER
- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style
              reuseIdentifier:reuseIdentifier];
  if (self) {
    [self setUpUI];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.titleLabel.text = nil;
  self.subtitleLabel.text = nil;
  self.accessoryType = UITableViewCellAccessoryNone;
}

#pragma mark - SET UP UI COMPONENTS
- (void)setUpUI {
  self.selectionStyle = UITableViewCellSelectionStyleNone;
  self.backgroundColor = [UIColor colorNamed:kBackgroundColor];

  // Container to hold the components
  UIView* containerView = [UIView new];
  containerView.backgroundColor = UIColor.clearColor;

  [self.contentView addSubview:containerView];
  [containerView fillSuperviewWithPadding:commonContainerPadding];

  // Title label
  UILabel* titleLabel = [UILabel new];
  _titleLabel = titleLabel;
  titleLabel.textColor = UIColor.labelColor;
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.font =
    [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  titleLabel.numberOfLines = 0;
  titleLabel.textAlignment = NSTextAlignmentLeft;

  // Subtitle label
  UILabel* subtitleLabel = [UILabel new];
  _subtitleLabel = subtitleLabel;
  subtitleLabel.textColor = UIColor.secondaryLabelColor;
  subtitleLabel.adjustsFontForContentSizeCategory = YES;
  subtitleLabel.font =
    [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  subtitleLabel.numberOfLines = 0;
  subtitleLabel.textAlignment = NSTextAlignmentLeft;

  // Image view
  UIImageView* imageView = [UIImageView new];
  _imageView = imageView;
  imageView.contentMode = UIViewContentModeScaleAspectFit;
  imageView.backgroundColor = UIColor.clearColor;
  [containerView addSubview:imageView];
  [imageView anchorTop:nil
               leading:containerView.leadingAnchor
                bottom:nil
              trailing:nil
               padding:imageViewPadding
                  size:imageViewSize];
  [imageView centerYInSuperview];

  // Stack view for title and subtitle
  UIStackView* textStackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    titleLabel, subtitleLabel
  ]];
  textStackView.distribution = UIStackViewDistributionFill;
  textStackView.spacing = vStackSpacing;
  textStackView.axis = UILayoutConstraintAxisVertical;

  [containerView addSubview:textStackView];
  [textStackView anchorTop:containerView.topAnchor
                   leading:imageView.trailingAnchor
                    bottom:containerView.bottomAnchor
                  trailing:containerView.trailingAnchor
                   padding:stackPadding];
}

#pragma mark - SETTERS

- (void)configurWithItem:(VivaldiATBItem*)item
     userPreferredOption:(ATBSettingType)userPreferred
     globalDefaultOption:(ATBSettingType)globalDefault
       showDefaultMarker:(BOOL)showDefaultMarker {

  self.subtitleLabel.text = item.subtitle;

  // Populate icons
  switch (item.type) {
    case ATBSettingNoBlocking:
      _imageView.image = [UIImage imageNamed:vATBShieldNone];
      break;
    case ATBSettingBlockTrackers:
      _imageView.image = [UIImage imageNamed:vATBShieldTrackers];
      break;
    case ATBSettingBlockTrackersAndAds:
      _imageView.image = [UIImage imageNamed:vATBShieldTrackesAndAds];
      break;
    default: break;
  }

  // Show selection check
  if (item.type == userPreferred) {
    _imageView.image =
        [_imageView.image
          imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  } else {
    _imageView.image =
        [_imageView.image
          imageWithRenderingMode:UIImageRenderingModeAlwaysOriginal];
  }

  // Render title text. The global default item will have a '(Default)' marker
  // alongside the item title. And the item title will be bold if the item is
  // selected.

  // Render title text. Rules: >>
  // 1. User preferred item will always be bold regardless whether its default
  // or not
  // 2. Global settings will have a 'Default' markup alongside the title. That
  // markup will always have regular font style.
  // 3. 'Default' markup will not be visible alongside the title when the
  // the options are rendered for global settings, such as main Ad and Tracker
  // blocker settings page.
  [self applyFontStyleToRange:item.title
            showDefaultMarker:item.type == globalDefault && showDefaultMarker
                  setSelected:item.type == userPreferred];
}

#pragma mark - PRIVATE
/// 'Default' marker will always have regular font style.
/// 'Title' may become bold or regular based on the selection state.
- (void)applyFontStyleToRange:(NSString*)title
            showDefaultMarker:(BOOL)showDefaultMarker
                  setSelected:(BOOL)setSelected {
  // Default marker
  NSString* defaultMarker = l10n_util::GetNSString(IDS_DEFAULT_LEVEL_LABEL);

  // Create full string from title and marker.
  NSString *fullString = title;

  if (showDefaultMarker) {
    fullString =
        [NSString stringWithFormat: @"%@%@%@", title, @" ", defaultMarker];
  }

  // Create attributed string from the full string.
  NSMutableAttributedString *attributedString =
    [[NSMutableAttributedString alloc]
        initWithString:fullString
            attributes:@{
              NSForegroundColorAttributeName: [UIColor colorNamed: kBlueColor],
              NSFontAttributeName:
                  [UIFont preferredFontForTextStyle:UIFontTextStyleBody]
            }];

  if (setSelected) {
    [self.titleLabel setAttributedText: attributedString];
  } else {
    self.titleLabel.text = fullString;
    self.titleLabel.textColor = UIColor.labelColor;
  }
}

@end
