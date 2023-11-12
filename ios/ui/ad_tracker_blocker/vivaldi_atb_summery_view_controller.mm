// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ad_tracker_blocker/vivaldi_atb_summery_view_controller.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/url_formatter/url_fixer.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/ui/ad_tracker_blocker/cells/vivaldi_atb_setting_item.h"
#import "ios/ui/ad_tracker_blocker/manager/vivaldi_atb_manager.h"
#import "ios/ui/ad_tracker_blocker/settings/vivaldi_atb_settings_view_controller.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_constants.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_source_type.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_summery_details_view_controller.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_summery_header_view.h"
#import "ios/ui/helpers/vivaldi_colors_helper.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/ntp/vivaldi_ntp_constants.h"
#import "ios/ui/ntp/vivaldi_speed_dial_constants.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Namespace
namespace {
// Converts NSString entered by the user to a GURL.
GURL ConvertUserDataToGURL(NSString* urlString) {
  if (urlString) {
    return url_formatter::FixupURL(base::SysNSStringToUTF8(urlString),
                                   std::string());
  } else {
    return GURL();
  }
}

// Padding for title label.
const UIEdgeInsets titleLabelPadding = UIEdgeInsetsMake(0.f, 12.f, 0.f, 0.f);

// Size for the favicon
const CGSize faviconSize = CGSizeMake(22.f, 22.f);
// Padding for the summery view.
const UIEdgeInsets summeryViewPadding = UIEdgeInsetsMake(0, 20.f, 0, 20.f);

// Section identifier for the page
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSiteSettings = kSectionIdentifierEnumZero
};

// Item type for the row
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSiteSetting = kItemTypeEnumZero
};

}

@interface VivaldiATBSummeryViewController()<VivaldiATBConsumer> {
  // Track rule group updates.
  BOOL ruleGroupApplied[2];
  // Track if a rule apply in progress. This is set to 'YES' only when user
  // triggers new settings.
  BOOL ruleApplyInProgress;
}

// TView to show the summery of the blocked ads and trackers.
@property (weak, nonatomic) VivaldiATBSummeryHeaderView* summeryView;
// Button for showing the ad and tracker blocker settings.
@property (weak, nonatomic) UIButton* blockerSettingsButton;
// Favicon view for currently loaded host favicon.
@property (weak, nonatomic) UIImageView* faviconView;
// Title label for the title of the host/domain currently loaded.
@property (weak, nonatomic) UILabel* titleLabel;
// FaviconLoader is a keyed service that uses LargeIconService to retrieve
// favicon images.
@property(nonatomic, assign) FaviconLoader* faviconLoader;
// The Browser in which blocker engine is active.
@property(nonatomic, assign) Browser* browser;
// The user's browser state model used.
@property(nonatomic, assign) ChromeBrowserState* browserState;
// Host of the currently loaded website
@property(nonatomic, strong) NSString* host;
// The manager for the adblock that provides all methods and properties for
// adblocker.
@property(nonatomic, strong) VivaldiATBManager* adblockManager;
// Available options for tracker blocker settings.
@property(nonatomic, strong) NSArray* adblockerSettingOptions;

@end


@implementation VivaldiATBSummeryViewController

@synthesize summeryView = _summeryView;
@synthesize blockerSettingsButton = _blockerSettingsButton;
@synthesize faviconView = _faviconView;
@synthesize titleLabel = _titleLabel;
@synthesize browser = _browser;
@synthesize faviconLoader = _faviconLoader;
@synthesize browserState = _browserState;
@synthesize host = _host;
@synthesize adblockManager = _adblockManager;
@synthesize adblockerSettingOptions = _adblockerSettingOptions;

#pragma mark - INITIALIZER
- (instancetype)initWithBrowser:(Browser*)browser
                           host:(NSString*)host {
  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    _host = host;
    _adblockerSettingOptions = @[];
    _browser = browser;
    _browserState =
        _browser->GetBrowserState()->GetOriginalChromeBrowserState();
    _faviconLoader =
        IOSChromeFaviconLoaderFactory::GetForBrowserState(_browserState);
    ruleGroupApplied[0] = NO;
    ruleGroupApplied[1] = NO;
    ruleApplyInProgress = NO;
  }
  [self setUpCustomNavigationBarTitleView];
  return self;
}

- (void)dealloc {
  [self shutdown];
}

#pragma mark - VIEW CONTROLLER LIFECYCLE
- (void)viewDidLoad {
  [super viewDidLoad];
  [super loadModel];
  [self setUpTableViewComponents];
  [self loadATBOptions];
}

-(void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  // Remove shadows and background from nav bar.
  [self setUpNavigationBarStyle];
  // We would like to initiate the title and favicon update when view is
  // about to be on the lifecycle.
  [self populateTitleAndFavicon];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [self updateHeaderViewHeight];
}

#pragma mark - PRIVATE
#pragma mark - SET UP UI COMPONENTS
- (void)setUpTableViewComponents {
  self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
  [self setUpTableViewHeader];
  [self setUpTableViewFooter];
}

// For Vivaldi we will hide the 1px underline below the navigation bar.
- (void)setUpNavigationBarStyle {
  UINavigationBarAppearance* appearance =
      [[UINavigationBarAppearance alloc] init];

  [appearance setBackgroundColor:
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor]];
  [appearance setShadowColor:UIColor.clearColor];
  [appearance setShadowImage:nil];

  // Apply the custom appearance to only this view controller's navigation bar
  self.navigationController.navigationBar.scrollEdgeAppearance = appearance;
  self.navigationController.navigationBar.standardAppearance = appearance;
  self.navigationController.navigationBar.compactAppearance = appearance;

  // Add Done button
  UIBarButtonItem* doneItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(handleDoneButtonTap)];
  self.navigationItem.rightBarButtonItem = doneItem;
}

- (void)setUpCustomNavigationBarTitleView {

  // The container for the favicon and title
  UIView* titleView = [UIView new];

  // Favicon view
  UIImageView* faviconView = [UIImageView new];
  _faviconView = faviconView;
  faviconView.contentMode = UIViewContentModeScaleAspectFit;
  faviconView.backgroundColor = UIColor.clearColor;

  [titleView addSubview:faviconView];
  [faviconView anchorTop:nil
                 leading:titleView.leadingAnchor
                  bottom:nil
                trailing:nil
                    size:faviconSize];
  [faviconView centerYInSuperview];

  // Website title label
  UILabel* titleLabel = [UILabel new];
  _titleLabel = titleLabel;
  titleLabel.textColor = UIColor.labelColor;
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  titleLabel.numberOfLines = 1;
  titleLabel.textAlignment = NSTextAlignmentLeft;

  [titleView addSubview:titleLabel];
  [titleLabel anchorTop:nil
                leading:faviconView.trailingAnchor
                 bottom:nil
               trailing:titleView.trailingAnchor
                padding:titleLabelPadding];
  [titleLabel centerYInSuperview];

  UIBarButtonItem* customTitleView =
    [[UIBarButtonItem alloc] initWithCustomView:titleView];
  self.navigationItem.leftBarButtonItem = customTitleView;
}

- (void)setUpTableViewHeader {
  UIView* tableHeaderView = [UIView new];
  tableHeaderView.backgroundColor = UIColor.clearColor;
  self.tableView.tableHeaderView = tableHeaderView;

  VivaldiATBSummeryHeaderView* summeryView =
    [VivaldiATBSummeryHeaderView new];
  _summeryView = summeryView;
  [tableHeaderView addSubview:summeryView];
  [summeryView
    fillSuperviewToSafeAreaInsetWithPadding:summeryViewPadding];
}

- (void)setUpTableViewFooter {
  UIView* footerView = [UIView new];
  footerView.backgroundColor = UIColor.clearColor;
  footerView.frame = CGRectMake(0, 0,
                                self.view.bounds.size.width,
                                tableFooterHeight);
  self.tableView.tableFooterView = footerView;

  // Settings button
  UIButton* blockerSettingsButton = [UIButton new];
  blockerSettingsButton.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  blockerSettingsButton.layer.cornerRadius = actionButtonCornerRadius;
  NSString* settingsButtonTitleString =
    l10n_util::GetNSString(IDS_VIVALDI_IOS_MANAGE_BLOCKER_SETTINGS);
  [blockerSettingsButton setTitle:settingsButtonTitleString
                         forState:UIControlStateNormal];
  [blockerSettingsButton
     setTitleColor:UIColor.vSystemBlue
     forState:UIControlStateNormal];
  [blockerSettingsButton addTarget:self
                            action:@selector(handleBlockerSettingsButtonTap)
                  forControlEvents:UIControlEventTouchUpInside];

  _blockerSettingsButton = blockerSettingsButton;
  [footerView addSubview:blockerSettingsButton];
  [blockerSettingsButton
    fillSuperviewToSafeAreaInsetWithPadding:actionButtonPadding];
}

- (void)updateHeaderViewHeight {
  UIView *headerView = self.tableView.tableHeaderView;
  if (headerView) {
    CGSize newSize =
        [headerView systemLayoutSizeFittingSize:CGSizeMake(
              self.view.bounds.size.width, 0)];

    if (newSize.height != headerView.frame.size.height) {
      CGRect newFrame = headerView.frame;
      newFrame.size.height = newSize.height;
      headerView.frame = newFrame;
      self.tableView.tableHeaderView = headerView;
    }
  }
}

#pragma mark ACTIONS
- (void)handleDoneButtonTap {
  [self shutdown];
  [self dismissViewControllerAnimated:YES completion:nil];
}

- (void)handleBlockerSettingsButtonTap {
  NSString* settingsTitleString =
    l10n_util::GetNSString(IDS_IOS_PREFS_VIVALDI_AD_AND_TRACKER_BLOCKER);
  VivaldiATBSettingsViewController* settingsController =
    [[VivaldiATBSettingsViewController alloc]
       initWithBrowser:_browser
                 title:settingsTitleString];
  [self.navigationController
    pushViewController:settingsController animated:YES];
}

- (void)loadATBOptions {
  self.adblockManager = [[VivaldiATBManager alloc] initWithBrowser:_browser];
  self.adblockManager.consumer = self;
  [self.adblockManager getSettingOptions];

  if ([self.adblockManager isApplyingExceptionRules]) {
    ruleApplyInProgress = YES;
    [_summeryView setRulesGroupApplying:YES];
  }
}

- (void)updateTableViewHeader {
  if (!self.adblockManager)
    return;

  [_summeryView setStatusFromSetting:
      [self.adblockManager blockingSettingForDomain:_host]];
}

-(void)reloadModelWithOptions:(NSArray*)options {

  TableViewModel* model = self.tableViewModel;

  // Delete any existing section.
  if ([model
          hasSectionForSectionIdentifier:SectionIdentifierSiteSettings])
    [model
        removeSectionWithIdentifier:SectionIdentifierSiteSettings];

  // Creates Section for the setting options
  [model
      addSectionWithIdentifier:SectionIdentifierSiteSettings];

  for (id option in options) {
    VivaldiATBSettingItem* tableViewItem = [[VivaldiATBSettingItem alloc]
        initWithType:ItemTypeSiteSetting];
    tableViewItem.item = option;
    tableViewItem.globalDefaultOption =
        [self.adblockManager globalBlockingSetting];
    tableViewItem.userPreferredOption =
        [self.adblockManager blockingSettingForDomain:_host];
    tableViewItem.showDefaultMarker = YES;

    // Show selection check
    if (tableViewItem.item.type ==
          [self.adblockManager blockingSettingForDomain:_host]) {
      tableViewItem.accessoryType = UITableViewCellAccessoryCheckmark;
    } else {
      tableViewItem.accessoryType = UITableViewCellAccessoryNone;
    }

    [model addItem:tableViewItem
         toSectionWithIdentifier:SectionIdentifierSiteSettings];
  }

  [self.tableView reloadData];
}

- (void)populateTitleAndFavicon {
  [self loadFavicon];
  self.titleLabel.text = self.host;
}

// Asynchronously loads favicon for given domain. When the favicon is not
// found in cache, use the fall back icon style.
- (void)loadFavicon {

  // Start loading a favicon.
  __weak VivaldiATBSummeryViewController* weakSelf = self;
  GURL url = ConvertUserDataToGURL(self.host);
  GURL blockURL(url);
  auto faviconLoadedBlock = ^(FaviconAttributes* attributes) {
    VivaldiATBSummeryViewController* strongSelf = weakSelf;
    if (!strongSelf)
      return;

    if (!attributes || !attributes.faviconImage) {
      self.faviconView.image = [UIImage imageNamed:vNTPSDFallbackFavicon];
      return;
    }
    self.faviconView.image = attributes.faviconImage;
  };

  self.faviconLoader->FaviconForPageUrlOrHost(
      blockURL, kMinFaviconSizePt, faviconLoadedBlock);
}

- (void)navigateToDetailsViewControllerWithSource:(ATBSourceType)source {
  NSString* titleString;
  switch (source) {
  case ATBSourceAds:
    titleString = l10n_util::GetNSString(IDS_AD_DETAILS);
    break;
  case ATBSourceTrackers:
    titleString = l10n_util::GetNSString(IDS_TRACKER_DETAILS);
    break;
  default:
    break;
  }

  VivaldiATBSummeryDetailsViewController* controller =
    [[VivaldiATBSummeryDetailsViewController alloc]
      initWithTitle:titleString];
  [self.navigationController pushViewController:controller animated:YES];
}

- (VivaldiATBSettingItem*) getSettingItemFromModel:(TableViewModel*)model
                                       atIndexPath:(NSIndexPath*)indexPath {
    TableViewItem* item = [model itemAtIndexPath:indexPath];
    return base::apple::ObjCCastStrict<VivaldiATBSettingItem>(item);
}

- (void)removeCheckmarksFromSettingItemsInSectionWithIdentifier:
    (TableViewModel*)model {

  if ([model hasSectionForSectionIdentifier:SectionIdentifierSiteSettings]) {
    // Loop through the items in section.
    for (TableViewItem* item in
         [model itemsInSectionWithIdentifier:SectionIdentifierSiteSettings]) {
      VivaldiATBSettingItem* settingItem =
          base::apple::ObjCCastStrict<VivaldiATBSettingItem>(item);
      if (settingItem.accessoryType == UITableViewCellAccessoryCheckmark) {
        // Pass user preferred to "None" as we will only use this to deselect
        // item.
        [self updateAccessoryTypeForSettingItem:settingItem
                                          model:model
                                         toType:UITableViewCellAccessoryNone
                            userPreferredOption:ATBSettingNone];
      }
    }
  }
}

- (void)updateAccessoryTypeForSettingItem:(VivaldiATBSettingItem*)settingItem
                                    model:(TableViewModel*)model
                                   toType:(UITableViewCellAccessoryType)type
                      userPreferredOption:(ATBSettingType)userPreffered {

  settingItem.accessoryType = type;
  UITableViewCell* cell =
      [self.tableView cellForRowAtIndexPath:
          [model indexPathForItem:settingItem]];
  cell.accessoryType = type;
  [self configureCell:cell
      withSettingItem:settingItem
  userPreferredOption:userPreffered];
}

- (void)configureCell:(UITableViewCell*)cell
      withSettingItem:(VivaldiATBSettingItem*)settingItem
  userPreferredOption:(ATBSettingType)userPreffered {

  if (!self.adblockManager)
    return;

  VivaldiATBSettingItemCell* settingCell =
      base::apple::ObjCCastStrict<VivaldiATBSettingItemCell>(cell);
  [settingCell configurWithItem:settingItem.item
            userPreferredOption:userPreffered
            globalDefaultOption:[self.adblockManager globalBlockingSetting]
              showDefaultMarker:YES];
}

- (void)setNewSelectionFor:(VivaldiATBSettingItem*)selectedItem
                     model:(TableViewModel*)model
               atIndexPath:(NSIndexPath*)indexPath {

  ATBSettingType siteSpecificSetting =
      [self getSiteSpecificSettingFromType:selectedItem.item.type];

  // Update the new selection checkmark and text style.
  [self updateAccessoryTypeForSettingItem:selectedItem
                                    model:model
                                   toType:UITableViewCellAccessoryCheckmark
                      userPreferredOption:siteSpecificSetting];

  if (siteSpecificSetting ==
      [self.adblockManager blockingSettingForDomain:_host])
    return;

  if (![VivaldiGlobalHelpers isValidDomain:_host])
    return;

  // Set the flag to track if a user triggered rule update in progress or not.
  ruleApplyInProgress = YES;

  // Update the header status
  [_summeryView setRulesGroupApplying:YES];

  // Add new exception.
  [self.adblockManager setExceptionForDomain:_host
                                blockingType:siteSpecificSetting];
}

- (ATBSettingType)getSiteSpecificSettingFromType:(NSInteger)type {
  switch (type) {
    case ATBSettingNoBlocking:
      return ATBSettingNoBlocking;
    case ATBSettingBlockTrackers:
      return ATBSettingBlockTrackers;
    case ATBSettingBlockTrackersAndAds:
      return ATBSettingBlockTrackersAndAds;
    default:
      if (!self.adblockManager)
        NOTREACHED();
      return [self.adblockManager globalBlockingSetting];
  }
}

- (void)shutdown {
  if (!self.adblockManager)
    return;
  self.adblockManager.consumer = nil;
  [self.adblockManager disconnect];
}

#pragma mark - UITABLEVIEW DELEGATE

- (void)tableView:(UITableView*)tableView
  didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewModel* model = self.tableViewModel;

  // Get newly selected item.
  VivaldiATBSettingItem* selectedItem =
      [self getSettingItemFromModel:model atIndexPath:indexPath];

  // Return early if its already selected.
  if (selectedItem.accessoryType == UITableViewCellAccessoryCheckmark) {
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    return;
  }

  // Otherwise remove selection check from the old selected ite,
  [self removeCheckmarksFromSettingItemsInSectionWithIdentifier:model];

  // Set new selected item checkmark, style and notify adblock manager.
  [self setNewSelectionFor:selectedItem model:model atIndexPath:indexPath];

  [self updateHeaderViewHeight];
}

#pragma mark: - VivaldiATBConsumer

- (void)didRefreshSettingOptions:(NSArray*)options {
  if (options.count == 0)
    return;
  _adblockerSettingOptions = options;

  [self reloadModelWithOptions:_adblockerSettingOptions];
  [self updateTableViewHeader];
}

- (void)rulesListDidEndApplying:(RuleGroup)group {

  // If user explicitely did not trigger settings changes we will avoid
  // listening to the changes. This method can be triggered other ways too.
  if (!ruleApplyInProgress)
    return;

  switch (group) {
    case RuleGroup::kTrackingRules:
      ruleGroupApplied[0] = YES;
      break;
    case RuleGroup::kAdBlockingRules:
      ruleGroupApplied[1] = YES;
      break;
  }

  // Check if all groups have been triggered
  if (ruleGroupApplied[0] && ruleGroupApplied[1]) {
    // Dismiss when all rules are applied.
    ruleApplyInProgress = NO;
    [self shutdown];
    [self dismissViewControllerAnimated:YES completion:nil];
  }
}

- (void)didRefreshExceptionsList:(NSArray*)exceptions {
  // If we manually trigger the settings from this page, return early.
  // We don't want to listen to this changes.
  if (ruleApplyInProgress)
    return;

  // More checks.
  if (exceptions.count == 0 ||
      !_host ||
      !_adblockerSettingOptions)
    return;

  for (id item in exceptions) {
    VivaldiATBItem* exceptionItem = static_cast<VivaldiATBItem*>(item);
    if ([exceptionItem.title isEqualToString:_host]) {
      [self didRefreshSettingOptions:_adblockerSettingOptions];
      break;
    }
  }
}

@end
