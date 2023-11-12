// Copyright 2022 Vivaldi Technologies AS. All rights reserved.

#import "ios/ui/notes/cells/note_parent_folder_item.h"

#import "base/apple/foundation_util.h"
#import "base/i18n/rtl.h"
#import "ios/chrome/browser/shared/ui/symbols/chrome_icon.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/ui/notes/note_ui_constants.h"
#import "ios/ui/notes/note_utils_ios.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - NoteParentFolderItem

@implementation NoteParentFolderItem

@synthesize title = _title;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.accessibilityIdentifier = @"Change Folder";
    self.cellClass = [NoteParentFolderCell class];
  }
  return self;
}

#pragma mark TableViewItem

- (void)configureCell:(TableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];
  NoteParentFolderCell* cell =
      base::apple::ObjCCastStrict<NoteParentFolderCell>(tableCell);
  cell.parentFolderNameLabel.text = self.title;
}

@end

#pragma mark - NoteParentFolderCell

@interface NoteParentFolderCell ()
// Stack view to display label / value which we'll switch from horizontal to
// vertical based on preferredContentSizeCategory.
@property(nonatomic, strong) UIStackView* stackView;
@end

@interface NoteParentFolderCell ()
@property(nonatomic, readwrite, strong) UILabel* parentFolderNameLabel;
@end

@implementation NoteParentFolderCell
@synthesize parentFolderNameLabel = _parentFolderNameLabel;
@synthesize stackView = _stackView;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (!self)
    return nil;

  self.isAccessibilityElement = YES;
  self.accessibilityTraits |= UIAccessibilityTraitButton;

  // "Folder" decoration label.
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.text = l10n_util::GetNSString(IDS_VIVALDI_NOTE_GROUP_BUTTON);
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  titleLabel.adjustsFontForContentSizeCategory = YES;
  [titleLabel setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                forAxis:UILayoutConstraintAxisHorizontal];
  [titleLabel
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];

  // Parent Folder name label.
  self.parentFolderNameLabel = [[UILabel alloc] init];
  self.parentFolderNameLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  self.parentFolderNameLabel.adjustsFontForContentSizeCategory = YES;
  self.parentFolderNameLabel.textColor =
      [UIColor colorNamed:kTextSecondaryColor];
  self.parentFolderNameLabel.textAlignment = NSTextAlignmentRight;
  [self.parentFolderNameLabel
      setContentHuggingPriority:UILayoutPriorityDefaultLow
                        forAxis:UILayoutConstraintAxisHorizontal];

  // Container StackView.
  self.stackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ titleLabel, self.parentFolderNameLabel ]];
  self.stackView.axis = UILayoutConstraintAxisHorizontal;
  self.stackView.spacing = kNoteCellViewSpacing;
  self.stackView.distribution = UIStackViewDistributionFill;
  self.stackView.alignment = UIStackViewAlignmentCenter;
  self.stackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.contentView addSubview:self.stackView];

  // Set up constraints.
  AddSameConstraintsToSidesWithInsets(
      self.stackView, self.contentView,
      LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kBottom |
          LayoutSides::kTop,
      NSDirectionalEdgeInsetsMake(
          kNoteCellVerticalInset, kNoteCellHorizontalLeadingInset,
          kNoteCellVerticalInset,
          kNoteCellHorizontalAccessoryViewSpacing));

  // Chevron accessory view.
  UIImageView* navigationChevronImage = [[UIImageView alloc]
      initWithImage:[UIImage imageNamed:@"table_view_cell_chevron"]];
  self.accessoryView = navigationChevronImage;
  // TODO(crbug.com/870841): Use default accessory type.
  if (base::i18n::IsRTL())
    self.accessoryView.transform = CGAffineTransformMakeRotation(M_PI);

  [self applyContentSizeCategoryStyles];

  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.parentFolderNameLabel.text = nil;
}

- (NSString*)accessibilityLabel {
  return self.parentFolderNameLabel.text;
}

- (NSString*)accessibilityHint {
  return l10n_util::GetNSString(
      IDS_VIVALDI_NOTE_EDIT_PARENT_FOLDER_BUTTON_HINT);
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (self.traitCollection.preferredContentSizeCategory !=
      previousTraitCollection.preferredContentSizeCategory) {
    [self applyContentSizeCategoryStyles];
  }
}

- (void)applyContentSizeCategoryStyles {
  if (UIContentSizeCategoryIsAccessibilityCategory(
          UIScreen.mainScreen.traitCollection.preferredContentSizeCategory)) {
    self.stackView.axis = UILayoutConstraintAxisVertical;
    self.stackView.alignment = UIStackViewAlignmentLeading;
    self.parentFolderNameLabel.textAlignment = NSTextAlignmentLeft;
  } else {
    self.stackView.axis = UILayoutConstraintAxisHorizontal;
    self.stackView.alignment = UIStackViewAlignmentCenter;
    self.parentFolderNameLabel.textAlignment = NSTextAlignmentRight;
  }
}

@end
