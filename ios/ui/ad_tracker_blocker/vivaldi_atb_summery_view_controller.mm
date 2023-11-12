// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ad_tracker_blocker/vivaldi_atb_summery_view_controller.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/url_formatter/url_fixer.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/ui/ntp/vivaldi_ntp_constants.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/ui/ad_tracker_blocker/cells/vivaldi_atb_setting_item.h"
#import "ios/ui/ad_tracker_blocker/settings/vivaldi_atb_settings_view_controller.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_constants.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_mediator.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_source_type.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_summery_details_view_controller.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_summery_header_view.h"
#import "ios/ui/helpers/vivaldi_colors_helper.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/helpers/vivaldi_uiviewcontroller_helper.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"
#import "vivaldi/mobile_common/grit/vivaldi_mobile_common_native_strings.h"

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
const CGFloat tableHeaderHeight = 110.f;
// Padding for the summery view.
const UIEdgeInsets summeryViewPadding = UIEdgeInsetsMake(0, 20.f, 0, 20.f);

// Section identifier for the page
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierGlobalSettings
};

// Item type for the row
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeGlobalSetting
};

}

@interface VivaldiATBSummeryViewController()<VivaldiATBConsumer,
                                            VivaldiATBSummeryHeaderViewDelegate>

// TView to show the summery of the blocked ads and trackers.
@property (weak, nonatomic) VivaldiATBSummeryHeaderView* summeryView;
// Button for showing the ad and tracker blocker settings.
@property (weak, nonatomic) UIButton* privacySettingsButton;
// Favicon view for currently loaded host favicon.
@property (weak, nonatomic) UIImageView* faviconView;
// Title label for the title of the host/domain currently loaded.
@property (weak, nonatomic) UILabel* titleLabel;
// FaviconLoader is a keyed service that uses LargeIconService to retrieve
// favicon images.
@property(nonatomic, assign) FaviconLoader* faviconLoader;
// The Browser in which bookmarks are presented
@property(nonatomic, assign) Browser* browser;
// The user's browser state model used.
@property(nonatomic, assign) ChromeBrowserState* browserState;
// Host of the currently loaded website
@property(nonatomic, strong) NSString* host;
// The mediator that provides data for this view controller.
@property(nonatomic, strong) VivaldiATBMediator* mediator;

@end


@implementation VivaldiATBSummeryViewController

@synthesize summeryView = _summeryView;
@synthesize privacySettingsButton = _privacySettingsButton;
@synthesize faviconView = _faviconView;
@synthesize titleLabel = _titleLabel;
@synthesize browser = _browser;
@synthesize faviconLoader = _faviconLoader;
@synthesize browserState = _browserState;
@synthesize host = _host;
@synthesize mediator = _mediator;

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
  self.mediator.consumer = nil;
  [self.mediator disconnect];
}

#pragma mark - VIEW CONTROLLER LIFECYCLE
- (void)viewDidLoad {
  [super viewDidLoad];
  [super loadModel];
  [self setUpTableViewComponents];
  [self loadATBOptions];

  // TODO: - @prio@vivaldi.com
  // Replace with actual value when backend is implemented.
  [self.summeryView setValueWithBlockedTrackers:121
                                            ads:223];
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
  summeryView.delegate = self;
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
  UIButton* privacySettingsButton = [UIButton new];
  privacySettingsButton.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  privacySettingsButton.layer.cornerRadius = actionButtonCornerRadius;
  NSString* privacyButtonTitleString =
    l10n_util::GetNSString(IDS_MANAGE_DEFAULT_SETTINGS);
  [privacySettingsButton setTitle:privacyButtonTitleString
                         forState:UIControlStateNormal];
  [privacySettingsButton
     setTitleColor:UIColor.vSystemBlue
     forState:UIControlStateNormal];
  [privacySettingsButton addTarget:self
                            action:@selector(handlePrivacySettingsButtonTap)
                  forControlEvents:UIControlEventTouchUpInside];

  _privacySettingsButton = privacySettingsButton;
  [footerView addSubview:privacySettingsButton];
  [privacySettingsButton
    fillSuperviewToSafeAreaInsetWithPadding:actionButtonPadding];
}

#pragma mark ACTIONS
- (void)handleDoneButtonTap {
  [self dismissViewControllerAnimated:YES completion:nil];
}

- (void)handlePrivacySettingsButtonTap {
  NSString* settingsTitleString =
    l10n_util::GetNSString(IDS_PREFS_ADS_TRACKING);
  VivaldiATBSettingsViewController* settingsController =
    [[VivaldiATBSettingsViewController alloc]
      initWithTitle:settingsTitleString];
  [self.navigationController
    pushViewController:settingsController animated:YES];
}

/// Create and start mediator to compute and  populate the ad and tracker
/// blocker setting options and blocked trackers and ads count for the host.
- (void)loadATBOptions {
  self.mediator = [[VivaldiATBMediator alloc] init];
  self.mediator.consumer = self;
  [self.mediator startMediating];
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

    if (!attributes) {
      return;
    }

    if (attributes.faviconImage) {
      self.faviconView.image = attributes.faviconImage;
    }
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

#pragma mark - AD AND TRACKER BLOCKER CONSUMER

- (void)refreshSettingOptions:(NSArray*)items {
  if (items.count == 0)
    return;
  [self reloadModelWithOptions:items];
}

#pragma mark - SUMMERY HEADER VIEW DELEGATE

- (void)didTapAds {
  [self navigateToDetailsViewControllerWithSource:ATBSourceAds];
}

- (void)didTapTrackers {
  [self navigateToDetailsViewControllerWithSource:ATBSourceTrackers];
}

#pragma mark - UITABLEVIEW DELEGATE

- (void)tableView:(UITableView*)tableView
didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [self dismissViewControllerAnimated:YES completion:nil];
}

@end
