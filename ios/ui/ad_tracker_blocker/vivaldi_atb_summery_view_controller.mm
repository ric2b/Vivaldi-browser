// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ad_tracker_blocker/vivaldi_atb_summery_view_controller.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/url_formatter/url_fixer.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/ui/ntp/vivaldi_ntp_constants.h"
#import "ios/chrome/browser/ui/ntp/vivaldi_speed_dial_constants.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
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
// Tableview header height
const CGFloat tableHeaderHeight = 80.f;
// Padding for the summery view.
const UIEdgeInsets summeryViewPadding = UIEdgeInsetsMake(0, 20.f, 0, 20.f);

// Section identifier for the page
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierGlobalSettings = kSectionIdentifierEnumZero
};

// Item type for the row
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeGlobalSetting = kItemTypeEnumZero
};

}

@interface VivaldiATBSummeryViewController()<VivaldiATBConsumer>

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

#pragma mark - INITIALIZER
- (instancetype)initWithBrowser:(Browser*)browser
                           host:(NSString*)host {
  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    _host = host;
    _browser = browser;
    _browserState =
        _browser->GetBrowserState()->GetOriginalChromeBrowserState();
    _faviconLoader =
        IOSChromeFaviconLoaderFactory::GetForBrowserState(_browserState);
  }
  [self setUpNavigationBarStyle];
  [self setUpCustomNavigationBarTitleView];
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
  [self setUpTableViewComponents];
  [self loadATBOptions];
}

-(void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  // We would like to initiate the title and favicon update when view is
  // about to be on the lifecycle.
  [self populateTitleAndFavicon];
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
  [[UINavigationBar appearance] setScrollEdgeAppearance:appearance];
  [[UINavigationBar appearance] setStandardAppearance:appearance];
  [[UINavigationBar appearance] setCompactAppearance:appearance];

  // Add Done button.
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
  tableHeaderView.frame = CGRectMake(0, 0,
                                self.view.bounds.size.width,
                                tableHeaderHeight);
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

#pragma mark ACTIONS
- (void)handleDoneButtonTap {
  [self dismissViewControllerAnimated:YES completion:nil];
}

- (void)handleBlockerSettingsButtonTap {
  NSString* settingsTitleString =
    l10n_util::GetNSString(IDS_PREFS_ADS_TRACKING);
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
          hasSectionForSectionIdentifier:SectionIdentifierGlobalSettings])
    [model
        removeSectionWithIdentifier:SectionIdentifierGlobalSettings];

  // Creates Section for the setting options
  [model
      addSectionWithIdentifier:SectionIdentifierGlobalSettings];

  for (id option in options) {
    VivaldiATBSettingItem* tableViewItem = [[VivaldiATBSettingItem alloc]
        initWithType:ItemTypeGlobalSetting];
    tableViewItem.item = option;
    tableViewItem.globalDefaultOption =
        [self.adblockManager globalBlockingSetting];
    tableViewItem.userPreferredOption =
        [self.adblockManager blockingSettingForDomain:_host];
    tableViewItem.showDefaultMarker = YES;

    [model addItem:tableViewItem
         toSectionWithIdentifier:SectionIdentifierGlobalSettings];
  }

  [self.tableView reloadData];
}

- (void)populateTitleAndFavicon {
  [self loadFaviconWithfallbackToGoogleServer:NO];
  self.titleLabel.text = self.host;
}

// Asynchronously loads favicon for given domain. When the favicon is not
// found in cache, try loading it from a Google server if
// `fallbackToGoogleServer` is YES, otherwise, use the fall back icon style.
- (void)loadFaviconWithfallbackToGoogleServer:(BOOL)fallbackToGoogleServer {

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

  CGFloat desiredFaviconSizeInPoints = kDesiredSmallFaviconSizePt;
  CGFloat minFaviconSizeInPoints = kMinFaviconSizePt;

  self.faviconLoader->FaviconForPageUrl(
      blockURL, desiredFaviconSizeInPoints, minFaviconSizeInPoints,
      /*fallback_to_google_server=*/fallbackToGoogleServer, faviconLoadedBlock);
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

#pragma mark - UITABLEVIEW DELEGATE

- (void)tableView:(UITableView*)tableView
  didSelectRowAtIndexPath:(NSIndexPath*)indexPath {

  TableViewModel* model = self.tableViewModel;

  VivaldiATBSettingItem* newSelectedCell =
      base::mac::ObjCCastStrict<VivaldiATBSettingItem>
          ([model itemAtIndexPath:indexPath]);

  NSInteger type = newSelectedCell.item.type;

  ATBSettingType siteSpecificSetting;

  if (!self.adblockManager)
    return;
  switch (type) {
    case ATBSettingNoBlocking:
      siteSpecificSetting = ATBSettingNoBlocking;
      break;
    case ATBSettingBlockTrackers:
      siteSpecificSetting = ATBSettingBlockTrackers;
      break;
    case ATBSettingBlockTrackersAndAds:
      siteSpecificSetting = ATBSettingBlockTrackersAndAds;
      break;
    default:
      siteSpecificSetting = [self.adblockManager globalBlockingSetting];
      break;
  }

  // Do nothing if previous and new selection are same.
  if (siteSpecificSetting ==
      [self.adblockManager blockingSettingForDomain:_host])
    return;

  if (![VivaldiGlobalHelpers isValidDomain:_host])
    return;

  // Remove exceptions first if any.
  [self.adblockManager removeExceptionForDomain:_host];

  // Add new exception.
  [self.adblockManager setExceptionForDomain:_host
                                blockingType:siteSpecificSetting];

  [self dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark: - VivaldiATBConsumer

- (void)didRefreshSettingOptions:(NSArray*)options {
  if (options.count == 0)
    return;
  [self reloadModelWithOptions:options];
  [self updateTableViewHeader];
}

@end
