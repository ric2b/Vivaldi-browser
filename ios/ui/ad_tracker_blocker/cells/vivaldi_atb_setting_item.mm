// Copyright 2022 Vivaldi Technologies AS. All rights reserved.

#import "ios/ui/ad_tracker_blocker/cells/vivaldi_atb_setting_item.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_constants.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/mobile_common/grit/vivaldi_mobile_common_native_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Padding for the stack view.
// In order - Top, Left, Bottom, Right
const UIEdgeInsets stackPadding = UIEdgeInsetsMake(12.f, 0.f, 12.f, 0.f);
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
     globalDefaultOption:self.globalDefaultOption];
}

@end

#pragma mark - VivaldiATBSettingItemCell
@interface VivaldiATBSettingItemCell()
// Title of the settings
@property (weak, nonatomic) UILabel* titleLabel;
// Description of the settings
@property (weak, nonatomic) UILabel* subtitleLabel;
@end

@implementation VivaldiATBSettingItemCell

@synthesize titleLabel = _titleLabel;
@synthesize subtitleLabel = _subtitleLabel;

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

  // Stack view for title and subtitle
  UIStackView* textStackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    titleLabel, subtitleLabel
  ]];
  textStackView.distribution = UIStackViewDistributionFill;
  textStackView.spacing = vStackSpacing;
  textStackView.axis = UILayoutConstraintAxisVertical;

  [containerView addSubview:textStackView];
  [textStackView fillSuperviewWithPadding:stackPadding];
}

#pragma mark - SETTERS

- (void)configurWithItem:(VivaldiATBItem*)item
     userPreferredOption:(ATBSettingOption)userPreferred
     globalDefaultOption:(ATBSettingOption)globalDefault {

  self.subtitleLabel.text = item.subtitle;

  // Render title text. The global default item will have a '(Default)' marker
  // alongside the item title. And the item title will be bold if the item is
  // selected.
  if (item.option == globalDefault) {

    // Get the default marker string from string file
    NSString* defaultMarker =
      l10n_util::GetNSString(IDS_DEFAULT_LEVEL_LABEL);

    // Combine the item title and default marker with an space in between.
    NSString *combinedString =
      [NSString stringWithFormat: @"%@%@%@", item.title, @" ", defaultMarker];

    if (item.option == userPreferred) {
      [self applyBoldStyleToRange:item.title fullString:combinedString];
    } else {
      self.titleLabel.text = combinedString;
    }

  } else {
    if (item.option == userPreferred) {
      [self applyBoldStyleToRange:item.title fullString:item.title];
    } else {
      self.titleLabel.text = item.title;
    }
  }

  // Show selection check
  if (item.option == userPreferred) {
    self.accessoryType = UITableViewCellAccessoryCheckmark;
  } else {
    self.accessoryType = UITableViewCellAccessoryNone;
  }
}

#pragma mark - PRIVATE
- (void)applyBoldStyleToRange:(NSString*)title
                   fullString:(NSString*)fullString {
  // Create attributed string from the full string to make some part bold.
  NSMutableAttributedString *attributedString =
    [[NSMutableAttributedString alloc] initWithString:fullString];

  // Make the title part bold by getting the range of it
  NSRange boldRange = [fullString rangeOfString:title];
  [attributedString
    addAttribute:NSFontAttributeName
           value:[UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]
           range:boldRange];

  [self.titleLabel setAttributedText: attributedString];
}

@end
