// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ad_tracker_blocker/settings/vivaldi_atb_settings_view_controller.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/ad_tracker_blocker/cells/vivaldi_atb_setting_item.h"
#import "ios/ui/ad_tracker_blocker/manager/vivaldi_atb_manager.h"
#import "ios/ui/ad_tracker_blocker/settings/vivaldi_atb_per_site_settings_view_controller.h"
#import "ios/ui/ad_tracker_blocker/settings/vivaldi_atb_source_settings_view_controller.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_constants.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

using l10n_util::GetNSString;

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Namespace
namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierGlobalSettings = kSectionIdentifierEnumZero,
  SectionIdentifierExceptions,
  SectionIdentifierSources,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeGlobalSetting = kItemTypeEnumZero,
  ItemTypeExceptionSetting,
  ItemTypeSourceSetting,
  ItemTypeHeader
};

}

@interface VivaldiATBSettingsViewController()<VivaldiATBConsumer>
// The Browser in which blocker engine is active.
@property(nonatomic, assign) Browser* browser;
// The manager for the adblock that provides all methods and properties for
// adblocker.
@property(nonatomic, strong) VivaldiATBManager* adblockManager;
@end

@implementation VivaldiATBSettingsViewController
@synthesize adblockManager = _adblockManager;
@synthesize browser = _browser;

#pragma mark - INITIALIZER
- (instancetype)initWithBrowser:(Browser*)browser
                          title:(NSString*)title {
  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    _browser = browser;
    self.title = title;
    self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
  }
  return self;
}

- (void)dealloc {
  if (!self.adblockManager)
    return;
  self.adblockManager.consumer = nil;
  [self.adblockManager disconnect];
}

#pragma mark - VIEW CONTROLLER LIFECYCLE
- (void)viewDidLoad {
  [super viewDidLoad];
  [super loadModel];
  [self loadATBOptions];
}

#pragma mark - PRIVATE

/// Create and start mediator to compute and  populate the ad and tracker
/// blocker setting options.
- (void)loadATBOptions {
  if (!_browser)
    return;
  self.adblockManager = [[VivaldiATBManager alloc] initWithBrowser:_browser];
  self.adblockManager.consumer = self;
  [self.adblockManager getSettingOptions];
}


-(void)reloadGlobalSettingModelWithOption:(NSArray*)options {

  TableViewModel* model = self.tableViewModel;

  // Delete any existing section.
  if ([model
          hasSectionForSectionIdentifier:SectionIdentifierGlobalSettings])
    [model
        removeSectionWithIdentifier:SectionIdentifierGlobalSettings];

  // Creates Section for the setting options
  [model
      addSectionWithIdentifier:SectionIdentifierGlobalSettings];

  // Set up section header
  [model setHeader:[self sectionHeaderWith:SectionIdentifierGlobalSettings]
                  forSectionWithIdentifier:SectionIdentifierGlobalSettings];

  for (id option in options) {
    VivaldiATBSettingItem* settingItem = [[VivaldiATBSettingItem alloc]
        initWithType:ItemTypeGlobalSetting];
    settingItem.item = option;
    settingItem.globalDefaultOption =
        [self.adblockManager globalBlockingSetting];
    settingItem.userPreferredOption =
        [self.adblockManager globalBlockingSetting];
    settingItem.showDefaultMarker = NO;

    // Show selection check
    if (settingItem.item.type == [self.adblockManager globalBlockingSetting]) {
      settingItem.accessoryType = UITableViewCellAccessoryCheckmark;
    } else {
      settingItem.accessoryType = UITableViewCellAccessoryNone;
    }

    [model addItem:settingItem
         toSectionWithIdentifier:SectionIdentifierGlobalSettings];
  }

  [self.tableView reloadData];
}

-(void)reloadExceptionAndSourceSettingsModel {
  TableViewModel* model = self.tableViewModel;

  // Delete any existing section.
  if ([model
          hasSectionForSectionIdentifier:SectionIdentifierExceptions])
    [model
        removeSectionWithIdentifier:SectionIdentifierExceptions];
  if ([model
          hasSectionForSectionIdentifier:SectionIdentifierSources])
    [model
        removeSectionWithIdentifier:SectionIdentifierSources];

  // Create sections for exceptions and source settings
  [model
      addSectionWithIdentifier:SectionIdentifierExceptions];
  [model
      addSectionWithIdentifier:SectionIdentifierSources];

  // Set up section header
  [model setHeader:[self sectionHeaderWith:SectionIdentifierExceptions]
                  forSectionWithIdentifier:SectionIdentifierExceptions];
  [model setHeader:[self sectionHeaderWith:SectionIdentifierSources]
                  forSectionWithIdentifier:SectionIdentifierSources];

  // Add expection setting option
  TableViewTextItem* exceptionItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeExceptionSetting];
  exceptionItem.text =
    GetNSString(IDS_BLOCK_PREF_MANAGE_BLOCKING_LEVEL_PER_SITE);
  [model addItem:exceptionItem
      toSectionWithIdentifier:SectionIdentifierExceptions];

  // Add source setting option
  TableViewTextItem* manageTrackerSourcesItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeSourceSetting];
  manageTrackerSourcesItem.useCustomSeparator = YES;
  manageTrackerSourcesItem.text =
    GetNSString(IDS_BLOCK_PREF_MANAGE_TRACKER_BLOCKING_SOURCES);

  TableViewTextItem* manageAdsSourcesItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeSourceSetting];
  manageAdsSourcesItem.text =
    GetNSString(IDS_BLOCK_PREF_MANAGE_AD_BLOCKING_SOURCES);

  [model addItem:manageTrackerSourcesItem
      toSectionWithIdentifier:SectionIdentifierSources];
  [model addItem:manageAdsSourcesItem
      toSectionWithIdentifier:SectionIdentifierSources];

  [self.tableView reloadData];
}

/// Returns the header footer item for each section.
- (TableViewHeaderFooterItem*)sectionHeaderWith:(NSInteger)section {
  TableViewTextHeaderFooterItem* header =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
  header.text = [[self headerForSection:section] capitalizedString];
  return header;
}

/// Returns the header title for each section.
- (NSString*)headerForSection:(NSInteger)section {
  switch (section) {
    case SectionIdentifierGlobalSettings:
      return GetNSString(IDS_BLOCK_PREF_CATEGORY_TRACKING_PROTECTION_LEVEL);
    case SectionIdentifierExceptions:
      return GetNSString(IDS_BLOCK_PREF_CATEGORY_EXCEPTIONS);
    case SectionIdentifierSources:
      return GetNSString(IDS_BLOCK_PREF_CATEGORY_SOURCES);
    default:
      return @"";
  }
}

- (void)navigateToSiteSettingViewController {
  NSString* pageTitleString =
    GetNSString(IDS_BLOCK_PREF_MANAGE_BLOCKING_LEVEL_PER_SITE);

  VivaldiATBPerSiteSettingsViewController* controller =
    [[VivaldiATBPerSiteSettingsViewController alloc]
        initWithBrowser:_browser title:pageTitleString];
  [self.navigationController pushViewController:controller animated:YES];
}

- (void)navigateToSourceSettingViewControllerWithIndex:(NSInteger)index {
  NSString* pageTitleString = (index == 0) ?
    GetNSString(IDS_BLOCK_PREF_MANAGE_TRACKER_BLOCKING_SOURCES) :
    GetNSString(IDS_BLOCK_PREF_MANAGE_AD_BLOCKING_SOURCES);

  VivaldiATBSourceSettingsViewController* controller =
    [[VivaldiATBSourceSettingsViewController alloc]
        initWithBrowser:_browser
                  title:pageTitleString
             sourceType:index == 0 ? ATBSourceTrackers : ATBSourceAds];
  [self.navigationController pushViewController:controller animated:YES];
}

#pragma mark - UITABLEVIEW DELEGATE

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  TableViewModel* model = self.tableViewModel;

  switch ([model itemTypeForIndexPath:indexPath]) {
    case ItemTypeGlobalSetting: {
      TableViewItem* selectedItem = [model itemAtIndexPath:indexPath];

      // Do nothing if the tapped option was already the default.
      VivaldiATBSettingItem* selectedSettingItem =
          base::apple::ObjCCastStrict<VivaldiATBSettingItem>(selectedItem);
      if (selectedSettingItem.accessoryType ==
          UITableViewCellAccessoryCheckmark) {
        [tableView deselectRowAtIndexPath:indexPath animated:YES];
        return;
      }

      // Iterate through the options and remove the checkmark from any that
      // have it.
      if ([model
           hasSectionForSectionIdentifier:SectionIdentifierGlobalSettings]) {
        for (TableViewItem* item in
             [model
              itemsInSectionWithIdentifier:SectionIdentifierGlobalSettings]) {
          VivaldiATBSettingItem* settingItem =
              base::apple::ObjCCastStrict<VivaldiATBSettingItem>(item);
          if (settingItem.accessoryType == UITableViewCellAccessoryCheckmark) {
            settingItem.accessoryType = UITableViewCellAccessoryNone;
            UITableViewCell* cell =
                [tableView cellForRowAtIndexPath:[model indexPathForItem:item]];
            cell.accessoryType = UITableViewCellAccessoryNone;
          }
        }
      }

      VivaldiATBSettingItem* newSelectedCell =
          base::apple::ObjCCastStrict<VivaldiATBSettingItem>
              ([model itemAtIndexPath:indexPath]);
      newSelectedCell.accessoryType = UITableViewCellAccessoryCheckmark;
      UITableViewCell* cell = [tableView cellForRowAtIndexPath:indexPath];
      cell.accessoryType = UITableViewCellAccessoryCheckmark;

      NSInteger type = newSelectedCell.item.type;
      ATBSettingType settingType = static_cast<ATBSettingType>(type);

      if (!self.adblockManager)
        return;
      [self.adblockManager setExceptionFromBlockingType:settingType];
      break;
    }
    case ItemTypeExceptionSetting:
      [self navigateToSiteSettingViewController];
      break;
    case ItemTypeSourceSetting:
      [self navigateToSourceSettingViewControllerWithIndex:indexPath.row];
      break;
    default:
      break;
  }
}

#pragma mark: - VivaldiATBConsumer

- (void)didRefreshSettingOptions:(NSArray*)options {
  if (options.count > 0)
    [self reloadGlobalSettingModelWithOption:options];
  [self reloadExceptionAndSourceSettingsModel];
}

@end
