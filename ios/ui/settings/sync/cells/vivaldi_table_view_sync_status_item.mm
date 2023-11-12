// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/sync/cells/vivaldi_table_view_sync_status_item.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kStackMargin = 13.0;
const CGFloat kStackViewSpacing = 13.0;
const CGFloat kStatusLabelCornerRadius = 13.0;
const CGFloat kStatusLabelLeadingPadding = 6.0;
const CGFloat kStatusLabelTrailingPadding = 8.0;
const CGFloat kStatusLabelVerticalPadding = 3.0;
const CGFloat kStatusIndicatorPointSize = 10.0;

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
      base::mac::ObjCCastStrict<VivaldiTableViewSyncStatusCell>(tableCell);
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
  cell.syncStatusView.backgroundColor = self.statusBackgroundColor;

  UIImageSymbolConfiguration* configuration =
    [UIImageSymbolConfiguration configurationWithPointSize:
      kStatusIndicatorPointSize];
  cell.statusCircle.image = [[UIImage systemImageNamed:@"circle.fill"
                     withConfiguration:configuration]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  cell.statusCircle.tintColor = self.statusIndicatorColor;

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

    _statusCircle = [[UIImageView alloc] init];

    UIView* syncIndicatorAndLabel = [[UIView alloc] init];
    [syncIndicatorAndLabel addSubview:_statusCircle];
    [syncIndicatorAndLabel addSubview:_syncActiveLabel];
    syncIndicatorAndLabel.backgroundColor = [UIColor clearColor];

    _syncStatusView = [[UIView alloc] init];
    _syncStatusView.layer.cornerRadius = kStatusLabelCornerRadius;
    _syncStatusView.layer.masksToBounds = YES;
    [_syncStatusView addSubview:syncIndicatorAndLabel];

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

    [_statusCircle anchorTop:nil
                      leading:syncIndicatorAndLabel.leadingAnchor
                      bottom:nil
                    trailing:nil];
    [_statusCircle centerYInSuperview];
    [_syncActiveLabel anchorTop:syncIndicatorAndLabel.topAnchor
                      leading:_statusCircle.trailingAnchor
                      bottom:syncIndicatorAndLabel.bottomAnchor
                    trailing:syncIndicatorAndLabel.trailingAnchor];
    [syncIndicatorAndLabel fillSuperviewWithPadding:UIEdgeInsetsMake(
                kStatusLabelVerticalPadding,
                kStatusLabelLeadingPadding,
                kStatusLabelVerticalPadding,
                kStatusLabelTrailingPadding)];

    [stackView fillSuperviewWithPadding:UIEdgeInsetsMake(
              kStackMargin,kStackMargin,
              kStackMargin,kStackMargin)];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];

  self.lastSyncDateLabel.hidden = NO;
  self.syncActiveLabel.hidden = NO;
}

@end
