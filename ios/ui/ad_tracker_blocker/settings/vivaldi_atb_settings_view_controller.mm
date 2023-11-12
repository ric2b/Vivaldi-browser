// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ad_tracker_blocker/settings/vivaldi_atb_settings_view_controller.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/ad_tracker_blocker/cells/vivaldi_atb_setting_item.h"
#import "ios/ui/ad_tracker_blocker/settings/vivaldi_atb_per_site_settings_view_controller.h"
#import "ios/ui/ad_tracker_blocker/settings/vivaldi_atb_source_settings_view_controller.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_constants.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_mediator.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/mobile_common/grit/vivaldi_mobile_common_native_strings.h"

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
// The mediator that provides data for this view controller.
@property(nonatomic, strong) VivaldiATBMediator* mediator;
@end

@implementation VivaldiATBSettingsViewController
@synthesize mediator = _mediator;

#pragma mark - INITIALIZER
- (instancetype)initWithTitle:(NSString*)title {
  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    self.title = title;
    self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
  }
  return self;
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
  self.mediator = [[VivaldiATBMediator alloc] init];
  self.mediator.consumer = self;
  [self.mediator startMediating];
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
    VivaldiATBSettingItem* settingOption = [[VivaldiATBSettingItem alloc]
        initWithType:ItemTypeGlobalSetting];
    settingOption.item = option;
    settingOption.globalDefaultOption = ATBSettingBlockTrackers;
    settingOption.userPreferredOption = ATBSettingBlockTrackers;

    [model addItem:settingOption
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
      initWithTitle:pageTitleString];
  [self.navigationController pushViewController:controller animated:YES];
}

- (void)navigateToSourceSettingViewControllerWithIndex:(NSInteger)index {
  NSString* pageTitleString = (index == 0) ?
    GetNSString(IDS_BLOCK_PREF_MANAGE_TRACKER_BLOCKING_SOURCES) :
    GetNSString(IDS_BLOCK_PREF_MANAGE_AD_BLOCKING_SOURCES);

  VivaldiATBSourceSettingsViewController* controller =
    [[VivaldiATBSourceSettingsViewController alloc]
      initWithTitle:pageTitleString];
  [self.navigationController pushViewController:controller animated:YES];
}

#pragma mark - UITABLEVIEW DELEGATE

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewModel* model = self.tableViewModel;

  if ([model itemTypeForIndexPath:indexPath] == ItemTypeExceptionSetting)
    [self navigateToSiteSettingViewController];

  if ([model itemTypeForIndexPath:indexPath] == ItemTypeSourceSetting)
    [self navigateToSourceSettingViewControllerWithIndex:indexPath.row];
}

#pragma mark - AD AND TRACKER BLOCKER CONSUMER

- (void)refreshSettingOptions:(NSArray*)items {
  if (items.count > 0)
    [self reloadGlobalSettingModelWithOption:items];
  [self reloadExceptionAndSourceSettingsModel];
}

@end
