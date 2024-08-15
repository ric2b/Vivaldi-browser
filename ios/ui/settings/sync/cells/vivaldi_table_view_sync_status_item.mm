// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/sync/cells/vivaldi_table_view_sync_status_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kStackMargin = 13.0;
const CGFloat kStackViewSpacing = 13.0;
const CGFloat kStatusLabelHorizontalPadding = 15.0;
const CGFloat kStatusLabelVerticalPadding = 4.0;

const int kSyncStatusTextColor = 0x3B3B3B;

const UIFontTextStyle fontTextStyle = UIFontTextStyleSubheadline;
}

#pragma mark - VivaldiTableViewSyncStatusItem

@implementation VivaldiTableViewSyncStatusItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [VivaldiTableViewSyncStatusCell class];
  }
  return self;
}

- (void)configureCell:(TableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];
  VivaldiTableViewSyncStatusCell* cell =
      base::apple::ObjCCastStrict<VivaldiTableViewSyncStatusCell>(tableCell);
  if ([self.accessibilityIdentifier length]) {
    cell.accessibilityIdentifier = self.accessibilityIdentifier;
  }

  [cell setSelectionStyle:UITableViewCellSelectionStyleNone];
  if (self.lastSyncDateString) {
    cell.lastSyncDateLabel.text = self.lastSyncDateString;
  } else {
    cell.lastSyncDateLabel.hidden = YES;
  }

  cell.syncActiveLabel.text = self.statusText;
  [cell updateLabelCornerRadius];

  cell.syncStatusView.backgroundColor = self.statusBackgroundColor;

  if (styler.cellBackgroundColor) {
    cell.backgroundColor = styler.cellBackgroundColor;
  } else {
    cell.backgroundColor = styler.tableViewBackgroundColor;
  }
}

@end

#pragma mark - VivaldiTableViewSyncStatusCell

@implementation VivaldiTableViewSyncStatusCell


- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    _lastSyncDateLabel = [[UILabel alloc] init];
    _lastSyncDateLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _lastSyncDateLabel.font = [UIFont preferredFontForTextStyle:fontTextStyle];
    _lastSyncDateLabel.textAlignment = NSTextAlignmentCenter;
    _lastSyncDateLabel.numberOfLines = 0;

    _syncActiveLabel = [[UILabel alloc] init];
    // Because the background of the label is colored we want
    // the same color for the text regardless of light/dark mode
    _syncActiveLabel.textColor = UIColorFromRGB(kSyncStatusTextColor);
    _syncActiveLabel.font =
        [UIFont preferredFontForTextStyle:fontTextStyle];
    _syncActiveLabel.textAlignment = NSTextAlignmentCenter;
    _syncActiveLabel.numberOfLines = 0;
    _syncActiveLabel.backgroundColor = [UIColor clearColor];

    _syncStatusView = [[UIView alloc] init];
    _syncStatusView.layer.masksToBounds = YES;
    [_syncStatusView addSubview:_syncActiveLabel];

    UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
      _lastSyncDateLabel, _syncStatusView
    ]];
    stackView.axis = UILayoutConstraintAxisVertical;
    stackView.alignment = UIStackViewAlignmentCenter;
    stackView.spacing = kStackViewSpacing;
    [self.contentView addSubview:stackView];

    // Layout
    if (!_lastSyncDateLabel.hidden) {
      [_lastSyncDateLabel anchorTop:nil
            leading:stackView.leadingAnchor
              bottom:nil
            trailing:stackView.trailingAnchor];
    }

    [_syncActiveLabel fillSuperviewWithPadding:UIEdgeInsetsMake(
                kStatusLabelVerticalPadding,
                kStatusLabelHorizontalPadding,
                kStatusLabelVerticalPadding,
                kStatusLabelHorizontalPadding)];

    [stackView fillSuperviewWithPadding:UIEdgeInsetsMake(
              kStackMargin,kStackMargin,
              kStackMargin,kStackMargin)];
  }

  return self;
}

- (void)updateLabelCornerRadius {
  [_syncStatusView layoutIfNeeded];
  CGFloat height = CGRectGetHeight(_syncStatusView.bounds);
  _syncStatusView.layer.cornerRadius = height/2.0;
}

- (void)prepareForReuse {
  [super prepareForReuse];

  self.lastSyncDateLabel.hidden = NO;
  self.syncActiveLabel.hidden = NO;
}

@end
