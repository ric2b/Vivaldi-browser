// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ad_tracker_blocker/settings/vivaldi_atb_per_site_settings_view_controller.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/ad_tracker_blocker/cells/vivaldi_atb_site_setting_item.h"
#import "ios/ui/ad_tracker_blocker/manager/vivaldi_atb_manager.h"
#import "ios/ui/ad_tracker_blocker/settings/vivaldi_atb_add_domain_source_view_controller.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_constants.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_domain_source_mode.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_item.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_source_type.h"
#import "ios/ui/helpers/vivaldi_colors_helper.h"
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
  SectionIdentifierSites = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSite = kItemTypeEnumZero,
};

}

@interface VivaldiATBPerSiteSettingsViewController()<VivaldiATBConsumer>
// The Browser in which blocker engine is active.
@property(nonatomic, assign) Browser* browser;
// The manager for the adblock that provides all methods and properties for
// adblocker.
@property(nonatomic, strong) VivaldiATBManager* adblockManager;
@end

@implementation VivaldiATBPerSiteSettingsViewController
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
    [self setUpTableViewFooter];
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
  [self initializeAdblockManager];
  [self getExemptedList];
}

#pragma mark - PRIVATE

- (void)initializeAdblockManager {
  if (self.adblockManager || !_browser)
    return;
  self.adblockManager = [[VivaldiATBManager alloc] initWithBrowser:_browser];
  self.adblockManager.consumer = self;
}

- (void)getExemptedList {
  if (self.adblockManager || !_browser)
    [self initializeAdblockManager];
  [self.adblockManager getAllExceptionsList];
}

- (void)setUpTableViewFooter {
  UIView* footerView = [UIView new];
  footerView.frame = CGRectMake(0, 0,
                                self.view.bounds.size.width,
                                tableFooterHeight);
  self.tableView.tableFooterView = footerView;

  UIButton* addDomainButton = [UIButton new];
  addDomainButton.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  addDomainButton.layer.cornerRadius = actionButtonCornerRadius;

  NSString* buttonTitleString = GetNSString(IDS_ADD_NEW_DOMAIN);
  [addDomainButton setTitle:buttonTitleString
                   forState:UIControlStateNormal];
  [addDomainButton
     setTitleColor:UIColor.vSystemBlue
     forState:UIControlStateNormal];
  [addDomainButton addTarget:self
                            action:@selector(handleAddDomainButtonTap)
                  forControlEvents:UIControlEventTouchUpInside];
  [footerView addSubview:addDomainButton];
  [addDomainButton fillSuperviewToSafeAreaInsetWithPadding:actionButtonPadding];
}

#pragma mark ACTIONS
- (void)handleAddDomainButtonTap {
  [self navigateToSourceDomainEditingWithDomain:nil
                                    siteSetting:ATBSettingNoBlocking
                                      isEditing:NO];
}

- (void)navigateToSourceDomainEditingWithDomain:(NSString*)domain
                                    siteSetting:(ATBSettingType)siteSetting
                                      isEditing:(BOOL)isEditing {
  NSString* titleString =
      isEditing ? GetNSString(IDS_EDIT_DOMAIN) :
      GetNSString(IDS_ADD_NEW_DOMAIN);

  VivaldiATBAddDomainSourceViewController* controller =
    [[VivaldiATBAddDomainSourceViewController alloc]
         initWithBrowser:_browser
                   title:titleString
                  source:ATBSourceNone
             editingMode:isEditing ? ATBEditingModeDomain : ATBAddingModeDomain
           editingDomain:domain
     siteSpecificSetting:isEditing ? siteSetting :
                            [self.adblockManager globalBlockingSetting]];
  [self.navigationController pushViewController:controller animated:YES];
}

- (void)reloadModelWithExceptions:(NSArray*)exceptions {
  TableViewModel* model = self.tableViewModel;

  // Delete any existing section.
  if ([model
          hasSectionForSectionIdentifier:SectionIdentifierSites])
    [model
        removeSectionWithIdentifier:SectionIdentifierSites];

  if (exceptions.count < 1)
    return;

  // Creates section for sites list
  [model
      addSectionWithIdentifier:SectionIdentifierSites];

  for (id item in exceptions) {
    VivaldiATBItem* exceptionItem = static_cast<VivaldiATBItem*>(item);
    VivaldiATBSiteSettingItem* tableViewItem =
        [[VivaldiATBSiteSettingItem alloc] initWithType:ItemTypeSite];
    tableViewItem.item = exceptionItem;
    tableViewItem.title = exceptionItem.title;
    tableViewItem.detailText = exceptionItem.subtitle;

    switch (exceptionItem.type) {
      case ATBSettingNoBlocking:
        tableViewItem.image = [UIImage imageNamed:vATBShieldNone];
        break;
      case ATBSettingBlockTrackers:
        tableViewItem.image = [UIImage imageNamed:vATBShieldTrackers];
        break;
      case ATBSettingBlockTrackersAndAds:
        tableViewItem.image = [UIImage imageNamed:vATBShieldTrackesAndAds];
        break;
      default: break;
    }

    tableViewItem.useCustomSeparator = YES;

    [model addItem:tableViewItem
         toSectionWithIdentifier:SectionIdentifierSites];
  }
}

#pragma mark - UITABLEVIEW DELEGATE

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  TableViewModel* model = self.tableViewModel;

  switch ([model itemTypeForIndexPath:indexPath]) {
    case ItemTypeSite: {
      TableViewItem* selectedItem = [model itemAtIndexPath:indexPath];
      VivaldiATBSiteSettingItem* selectedSetting =
          base::apple::ObjCCastStrict<VivaldiATBSiteSettingItem>(selectedItem);
      [self navigateToSourceDomainEditingWithDomain:selectedSetting.item.title
                                        siteSetting:selectedSetting.item.type
                                          isEditing:YES];
    }
      break;
    default:
      break;
  }
}

#pragma mark: - VivaldiATBConsumer
- (void)didRefreshExceptionsList:(NSArray*)exceptions {
  [self reloadModelWithExceptions:exceptions];
  [self.tableView reloadData];
}

@end
