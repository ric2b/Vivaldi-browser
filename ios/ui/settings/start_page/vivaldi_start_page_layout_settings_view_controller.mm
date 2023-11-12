// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/start_page/vivaldi_start_page_layout_settings_view_controller.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/helpers/vivaldi_colors_helper.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/ntp/vivaldi_speed_dial_constants.h"
#import "ios/ui/ntp/vivaldi_start_page_prefs.h"
#import "ios/ui/settings/start_page/vivaldi_start_page_layout_preview_view.h"
#import "ios/ui/settings/start_page/vivaldi_start_page_layout_style.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

using l10n_util::GetNSString;

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Namespace
namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSettings = kSectionIdentifierEnumZero
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeLayoutLarge = kItemTypeEnumZero,
  ItemTypeLayoutMedium,
  ItemTypeLayoutSmall,
  ItemTypeLayoutList,
};

}

// Namespace
namespace {
// This is a safe height so that we don't scroll the preview view in any
// situtation.
const CGFloat tableViewFooterHeight = 700.f;
const UIEdgeInsets footerViewPadding = UIEdgeInsetsMake(8, 8, 8, 8);
const UIEdgeInsets previewLabelPadding = UIEdgeInsetsMake(0, 12, 0, 12);
const UIEdgeInsets previewViewPadding = UIEdgeInsetsMake(12, 0, 0, 0);
}

@interface VivaldiStartPageLayoutSettingsViewController()
@property(nonatomic, weak) UIView* footerView;
@property(nonatomic, weak) VivaldiStartPageLayoutPreviewView* previewView;
// The Browser in which bookmarks are presented
@property(nonatomic, assign) Browser* browser;
// The user's browser state model used.
@property(nonatomic, assign) ChromeBrowserState* browserState;
@end

@implementation VivaldiStartPageLayoutSettingsViewController
@synthesize footerView = _footerView;
@synthesize previewView = _previewView;

#pragma mark - INITIALIZER
- (instancetype)initWithTitle:(NSString*)title
                      browser:(Browser*)browser {
  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    self.title = title;
    _browser = browser;
    _browserState =
        _browser->GetBrowserState()->GetOriginalChromeBrowserState();

    self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
    [self setUpTableViewFooter];
  }
  return self;
}

#pragma mark - VIEW CONTROLLER LIFECYCLE
- (void)viewDidLoad {
  [super viewDidLoad];
  [super loadModel];
  [self reloadModel];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [self handleDeviceOrientationChange];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
  (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  [self handleDeviceOrientationChange];
}

#pragma mark - PRIVATE

- (void)setUpTableViewFooter {
  UIView* footerView = [UIView new];
  _footerView = footerView;
  footerView.backgroundColor = UIColor.clearColor;
  footerView.frame = CGRectMake(0, 0,
                                self.view.bounds.size.width,
                                tableViewFooterHeight);
  self.tableView.tableFooterView = footerView;

  UIView* footerContentView = [UIView new];
  footerContentView.backgroundColor = UIColor.clearColor;
  [footerView addSubview:footerContentView];
  [footerContentView
    fillSuperviewToSafeAreaInsetWithPadding: footerViewPadding];

  UILabel* previewLabel = [UILabel new];
  previewLabel.text =
    GetNSString(IDS_IOS_VIVALDI_START_PAGE_LAYOUT_SIZE_PREVIEW);
  previewLabel.textAlignment = NSTextAlignmentLeft;
  previewLabel.backgroundColor = UIColor.clearColor;
  previewLabel.adjustsFontForContentSizeCategory = YES;
  previewLabel.font =
    [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];

  [footerContentView addSubview: previewLabel];
  [previewLabel anchorTop: footerContentView.topAnchor
                       leading: footerContentView.leadingAnchor
                        bottom: nil
                      trailing: footerContentView.trailingAnchor
                       padding: previewLabelPadding];

  VivaldiStartPageLayoutPreviewView* previewView =
    [VivaldiStartPageLayoutPreviewView new];
  _previewView = previewView;

  [footerContentView addSubview: previewView];
  [previewView anchorTop: previewLabel.bottomAnchor
                 leading: footerContentView.leadingAnchor
                  bottom: footerContentView.bottomAnchor
                trailing: footerContentView.trailingAnchor
                 padding: previewViewPadding];

  [self updateUI];
}

-(void)reloadModel {

  TableViewModel* model = self.tableViewModel;

  // Delete any existing section.
  if ([model
          hasSectionForSectionIdentifier:SectionIdentifierSettings])
    [model
        removeSectionWithIdentifier:SectionIdentifierSettings];

  // Creates Section for the setting options
  [model
      addSectionWithIdentifier:SectionIdentifierSettings];

  // Add setting options
  TableViewTextItem* largeItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeLayoutLarge];
  largeItem.useCustomSeparator = YES;
  largeItem.text = GetNSString(IDS_IOS_VIVALDI_START_PAGE_LAYOUT_LARGE);

  TableViewTextItem* mediumItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeLayoutMedium];
  mediumItem.useCustomSeparator = YES;
  mediumItem.text = GetNSString(IDS_IOS_VIVALDI_START_PAGE_LAYOUT_MEDIUM);

  TableViewTextItem* smallItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeLayoutSmall];
  smallItem.useCustomSeparator = YES;
  smallItem.text = GetNSString(IDS_IOS_VIVALDI_START_PAGE_LAYOUT_SMALL);

  TableViewTextItem* listItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeLayoutList];
  listItem.useCustomSeparator = YES;
  listItem.text = GetNSString(IDS_IOS_VIVALDI_START_PAGE_LAYOUT_LIST);

  // Show tick on the currently selected one
  switch ([self currentLayoutStyle]) {
    case VivaldiStartPageLayoutStyleLarge:
      largeItem.accessoryType = UITableViewCellAccessoryCheckmark;
      break;
    case VivaldiStartPageLayoutStyleMedium:
      mediumItem.accessoryType = UITableViewCellAccessoryCheckmark;
      break;
    case VivaldiStartPageLayoutStyleSmall:
      smallItem.accessoryType = UITableViewCellAccessoryCheckmark;
      break;
    case VivaldiStartPageLayoutStyleList:
      listItem.accessoryType = UITableViewCellAccessoryCheckmark;
      break;
  }

  // Add items to the model
  [model addItem:largeItem
       toSectionWithIdentifier:SectionIdentifierSettings];
  [model addItem:mediumItem
       toSectionWithIdentifier:SectionIdentifierSettings];
  [model addItem:smallItem
       toSectionWithIdentifier:SectionIdentifierSettings];
  [model addItem:listItem
       toSectionWithIdentifier:SectionIdentifierSettings];

  [self.tableView reloadData];
}

- (void)updateUI {
  [_previewView reloadLayoutWithStyle:[self currentLayoutStyle]];
}

/// Returns current layout style
- (VivaldiStartPageLayoutStyle)currentLayoutStyle {
  if (!_browserState)
    return VivaldiStartPageLayoutStyleMedium;
  return [VivaldiStartPagePrefs
          getStartPageLayoutStyleWithPrefService:self.browserState->GetPrefs()];
}

/// Sets current layout style
- (void)setCurrentLayoutStyle:(VivaldiStartPageLayoutStyle)style {
  if (!_browserState)
    return;
  [VivaldiStartPagePrefs setStartPageLayoutStyle:style
                                  inPrefServices:self.browserState->GetPrefs()];
}

/// Device orientation change handler
- (void)handleDeviceOrientationChange {
  [self updateUI];
}


#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {

  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  TableViewModel* model = self.tableViewModel;
  TableViewItem* selectedItem = [model itemAtIndexPath:indexPath];

  // Do nothing if the tapped option was already the default.
  TableViewTextItem* selectedTextItem =
      base::apple::ObjCCastStrict<TableViewTextItem>(selectedItem);
  if (selectedTextItem.accessoryType == UITableViewCellAccessoryCheckmark) {
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    return;
  }

  // Iterate through the options and remove the checkmark from any that have it.
  if ([model hasSectionForSectionIdentifier:SectionIdentifierSettings]) {
    for (TableViewItem* item in
         [model itemsInSectionWithIdentifier:SectionIdentifierSettings]) {
      TableViewTextItem* textItem =
          base::apple::ObjCCastStrict<TableViewTextItem>(item);
      if (textItem.accessoryType == UITableViewCellAccessoryCheckmark) {
        textItem.accessoryType = UITableViewCellAccessoryNone;
        UITableViewCell* cell =
            [tableView cellForRowAtIndexPath:[model indexPathForItem:item]];
        cell.accessoryType = UITableViewCellAccessoryNone;
      }
    }
  }

  // Show the checkmark on the new default option.
  TableViewTextItem* newSelectedOption =
      base::apple::ObjCCastStrict<TableViewTextItem>(
          [model itemAtIndexPath:indexPath]);
  newSelectedOption.accessoryType = UITableViewCellAccessoryCheckmark;
  UITableViewCell* cell = [tableView cellForRowAtIndexPath:indexPath];
  cell.accessoryType = UITableViewCellAccessoryCheckmark;

  NSInteger type = [model itemTypeForIndexPath:indexPath];
  [self handleLayoutSelectionWithItemType:type];
}

#pragma mark - PRIVATE
- (void)handleLayoutSelectionWithItemType:(NSInteger)type {

  VivaldiStartPageLayoutStyle selectedLayout;
  switch (type) {
    case ItemTypeLayoutLarge:
      selectedLayout = VivaldiStartPageLayoutStyleLarge;
      break;
    case ItemTypeLayoutMedium:
      selectedLayout = VivaldiStartPageLayoutStyleMedium;
      break;
    case ItemTypeLayoutSmall:
      selectedLayout = VivaldiStartPageLayoutStyleSmall;
      break;
    case ItemTypeLayoutList:
      selectedLayout = VivaldiStartPageLayoutStyleList;
      break;
    default:
      selectedLayout = VivaldiStartPageLayoutStyleMedium;
      break;
  }

  [_previewView reloadLayoutWithStyle:selectedLayout];
  [self setCurrentLayoutStyle:selectedLayout];

  [[NSNotificationCenter defaultCenter]
    postNotificationName:vStartPageLayoutChangeDidChange
                  object:self];
}

@end
