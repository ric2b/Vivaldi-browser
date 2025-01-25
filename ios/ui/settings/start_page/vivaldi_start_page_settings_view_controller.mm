// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/start_page/vivaldi_start_page_settings_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/features/vivaldi_features.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/ui/settings/start_page/layout_settings/vivaldi_start_page_layout_settings_coordinator.h"
#import "ios/ui/settings/start_page/reopen_with/vivaldi_start_page_reopen_with_coordinator.h"
#import "ios/ui/settings/start_page/vivaldi_start_page_prefs.h"
#import "ios/ui/settings/start_page/vivaldi_start_page_start_item_type.h"
#import "ios/ui/settings/start_page/wallpaper_settings/wallpaper_settings_swift.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

using l10n_util::GetNSString;

namespace {
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierStartPageSettings = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  SettingsItemTypeShowFrequentlyVisited = kItemTypeEnumZero,
  SettingsItemTypeDisplaySpeedDials,
  SettingsItemTypeStartPageLayout,
  SettingsItemTypeWallpaper,
  SettingsItemTypeStartPageOpenWithItem,
  SettingsItemTypeCustomizeStartPage,
};

NSString* const kStartPageShowFrequentlyVisitedSettingsCellId =
    @"kStartPageShowFrequentlyVisitedSettingsCellId";
NSString* const kStartPageShowSpeedDialsSettingsCellId =
    @"kStartPageShowSpeedDialsSettingsCellId";
NSString* const kStartPageLayoutSettingsCellId =
    @"kStartPageLayoutSettingsCellId";
NSString* const kStartPageWallpaperSettingsCellId =
    @"kStartPageWallpaperSettingsCellId";
NSString* const kStartPageOpenWithSettingsCellId =
    @"kStartPageOpenWithSettingsCellId";
NSString* const kStartPageCustomizeStartPageSettingsCellId =
    @"kStartPageCustomizeStartPageSettingsCellId";

}  // namespace

@interface VivaldiStartPageSettingsViewController() {
  // The browser where the settings are being displayed.
  Browser* _browser;
  // Layout setting item reference to modify with observer state.
  TableViewDetailIconItem* _layoutSettingsItem;
  // Current layout settings
  VivaldiStartPageLayoutStyle _startPageLayout;
  // The item related to the switch for the "Frequently Visited Pages" setting.
  TableViewSwitchItem* _displayFrequentlyVisitedPagesItem;
  // The item related to the switch for the "Display Speed Dials" setting.
  TableViewSwitchItem* _displaySpeedDialsItem;
  // Current start page reopen with option.
  VivaldiStartPageStartItemType _startPageStartWithItem;
  // Start page open with setting item reference to modify with observer state.
  TableViewDetailIconItem* _openStartPageWithSettingsItem;
  // The item related to the switch for the "Customize Start Page button"
  // setting.
  TableViewSwitchItem* _customizeStartPageItem;
  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;
  // Layout settings coordinator.
  VivaldiStartPageLayoutSettingsCoordinator* _layoutSettingsCoordinator;
  // Reopen with settings coordinator.
  VivaldiStartPageReopenWithCoordinator* _reopenWithSettingsCoordinator;
}

// Boolean for "Frequently Visited Pages" setting state.
@property(nonatomic, assign) BOOL showFrequentlyVisitedPages;
// Boolean for "Display Speed Dials" setting state.
@property(nonatomic, assign) BOOL displaySpeedDials;
// Boolean for "Customize Start Page button" setting state.
@property(nonatomic, assign) BOOL customizeStartPage;

@end

@implementation VivaldiStartPageSettingsViewController

#pragma mark - Initialization

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _browser = browser;
    [VivaldiStartPagePrefs
        setPrefService:_browser->GetProfile()->GetPrefs()];
  }
  return self;
}


#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  [self loadModel];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];

  if (_settingsAreDismissed)
    return;

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierStartPageSettings];

  if (IsTopSitesEnabled()) {
    [model addItem:[self displayFrequentlyVisitedTableItem]
        toSectionWithIdentifier:SectionIdentifierStartPageSettings];
  }
  [model addItem:[self displaySpeedDialsTableItem]
      toSectionWithIdentifier:SectionIdentifierStartPageSettings];

  [model addItem:[self layoutSettingsItem]
      toSectionWithIdentifier:SectionIdentifierStartPageSettings];
  [model addItem:[self wallpaperSettingsItem]
      toSectionWithIdentifier:SectionIdentifierStartPageSettings];

  [model addItem:[self openStartPageWithSettingsItem]
      toSectionWithIdentifier:SectionIdentifierStartPageSettings];

  [model addItem:[self customizeStartPageTableItem]
      toSectionWithIdentifier:SectionIdentifierStartPageSettings];
}


#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  if (itemType == SettingsItemTypeShowFrequentlyVisited) {
    TableViewSwitchCell* switchCell =
        base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
    [switchCell.switchView
        addTarget:self
            action:@selector(displayFrequentlyVisitedSwitchToggled:)
                forControlEvents:UIControlEventValueChanged];
  }

  if (itemType == SettingsItemTypeDisplaySpeedDials) {
    TableViewSwitchCell* switchCell =
        base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(displaySpeedDialsSwitchToggled:)
                    forControlEvents:UIControlEventValueChanged];
  }

  if (itemType == SettingsItemTypeCustomizeStartPage) {
    TableViewSwitchCell* switchCell =
        base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
    [switchCell.switchView addTarget:self
          action:@selector(showCustomizeStartPageSwitchToggled:)
                forControlEvents:UIControlEventValueChanged];
  }

  return cell;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {

  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  switch (itemType) {
    case SettingsItemTypeStartPageLayout:
      [self showStartPageLayoutSettings];
      break;
    case SettingsItemTypeWallpaper:
      [self showWallpaperSettings];
      break;
    case SettingsItemTypeStartPageOpenWithItem:
      [self showOpenWithSettings];
      break;
    default:
      break;
  }
}

#pragma mark - VivaldiStartPageSettingsConsumer

- (void)setPreferenceShowFrequentlyVisitedPages:(BOOL)showFrequentlyVisited {
  if (!IsTopSitesEnabled()) {
    return;
  }

  self.showFrequentlyVisitedPages = showFrequentlyVisited;
  if (!_displayFrequentlyVisitedPagesItem)
    return;
  _displayFrequentlyVisitedPagesItem.on = showFrequentlyVisited;
  [self reconfigureCellsForItems:@[_displayFrequentlyVisitedPagesItem]];
}

- (void)setPreferenceShowSpeedDials:(BOOL)showSpeedDials {
  self.displaySpeedDials = showSpeedDials;
  if (!_displaySpeedDialsItem)
    return;
  _displaySpeedDialsItem.on = showSpeedDials;
  [self reconfigureCellsForItems:@[_displaySpeedDialsItem]];
}

- (void)setPreferenceShowCustomizeStartPageButton:(BOOL)showCustomizeButton {
  self.customizeStartPage = showCustomizeButton;
  if (!_customizeStartPageItem)
    return;
  _customizeStartPageItem.on = showCustomizeButton;
  [self reconfigureCellsForItems:@[_customizeStartPageItem]];
}

- (void)setPreferenceSpeedDialLayout:
    (VivaldiStartPageLayoutStyle)layoutStyle {
  _startPageLayout = layoutStyle;
  if (!_layoutSettingsItem)
    return;
  _layoutSettingsItem.detailText =
      [self titleForCurrentLayout:layoutStyle];
  [self reconfigureCellsForItems:@[_layoutSettingsItem]];
}

- (void)setPreferenceSpeedDialColumn:(VivaldiStartPageLayoutColumn)column {
  // No op.
}

- (void)setPreferenceStartPageReopenWithItem:
    (VivaldiStartPageStartItemType)item {
  _startPageStartWithItem = item;
  if (!_startPageStartWithItem || !_openStartPageWithSettingsItem)
    return;
  _openStartPageWithSettingsItem.detailText =
      [self titleForReopenStartPageWithItem:item];
  [self reconfigureCellsForItems:@[_openStartPageWithSettingsItem]];
}

#pragma mark SettingsControllerProtocol

- (void)reportDismissalUserAction {
  // No op.
}

- (void)reportBackUserAction {
  // No op.
}

- (void)settingsWillBeDismissed {
  DCHECK(!_settingsAreDismissed);
  _displayFrequentlyVisitedPagesItem = nullptr;
  _displaySpeedDialsItem = nullptr;
  _layoutSettingsItem = nullptr;
  _openStartPageWithSettingsItem = nullptr;
  _customizeStartPageItem = nullptr;
  _settingsAreDismissed = YES;

  [_layoutSettingsCoordinator stop];
  _layoutSettingsCoordinator = nil;
}

#pragma mark - Switch Actions

- (void)displayFrequentlyVisitedSwitchToggled:(UISwitch*)sender {
  BOOL newSwitchValue = sender.isOn;
  _displayFrequentlyVisitedPagesItem.on = newSwitchValue;
  self.showFrequentlyVisitedPages = newSwitchValue;
  [self.consumer setPreferenceShowFrequentlyVisitedPages:newSwitchValue];
}

- (void)displaySpeedDialsSwitchToggled:(UISwitch*)sender {
  BOOL newSwitchValue = sender.isOn;
  _displaySpeedDialsItem.on = newSwitchValue;
  self.displaySpeedDials = newSwitchValue;
  [self.consumer setPreferenceShowSpeedDials:newSwitchValue];
}

- (void)showCustomizeStartPageSwitchToggled:(UISwitch*)sender {
  BOOL newSwitchValue = sender.isOn;
  _customizeStartPageItem.on = newSwitchValue;
  self.customizeStartPage = newSwitchValue;
  [self.consumer
      setPreferenceShowCustomizeStartPageButton:newSwitchValue];
}

#pragma mark - Private Methods

- (TableViewSwitchItem*)displayFrequentlyVisitedTableItem {
  if (!_displayFrequentlyVisitedPagesItem) {
    _displayFrequentlyVisitedPagesItem = [[TableViewSwitchItem alloc]
        initWithType:SettingsItemTypeShowFrequentlyVisited];
    _displayFrequentlyVisitedPagesItem.text =
        GetNSString(IDS_IOS_START_PAGE_SETTINGS_TOP_SITES_TITLE);
    _displayFrequentlyVisitedPagesItem.on = self.showFrequentlyVisitedPages;
    _displayFrequentlyVisitedPagesItem.accessibilityIdentifier =
        kStartPageShowFrequentlyVisitedSettingsCellId;
  }
  return _displayFrequentlyVisitedPagesItem;
}

- (TableViewSwitchItem*)displaySpeedDialsTableItem {
  if (!_displaySpeedDialsItem) {
    _displaySpeedDialsItem = [[TableViewSwitchItem alloc]
        initWithType:SettingsItemTypeDisplaySpeedDials];
    _displaySpeedDialsItem.text =
        GetNSString(IDS_IOS_START_PAGE_SETTINGS_SPEED_DIALS_TITLE);
    _displaySpeedDialsItem.on = self.displaySpeedDials;
    _displaySpeedDialsItem.accessibilityIdentifier =
        kStartPageShowSpeedDialsSettingsCellId;
  }
  return _displaySpeedDialsItem;
}

- (TableViewDetailIconItem*)layoutSettingsItem {
  _layoutSettingsItem =
      [self detailItemWithType:SettingsItemTypeStartPageLayout
                             text:GetNSString(
                                IDS_IOS_VIVALDI_START_PAGE_LAYOUT_TITLE)
                       detailText:@""
          accessibilityIdentifier:kStartPageLayoutSettingsCellId];
  [self setPreferenceSpeedDialLayout:_startPageLayout];
  return _layoutSettingsItem;
}

- (TableViewDetailIconItem*)wallpaperSettingsItem {
  TableViewDetailIconItem* wallpaperSettingsItem =
      [self detailItemWithType:SettingsItemTypeWallpaper
                          text:GetNSString(
                                IDS_IOS_VIVALDI_START_PAGE_WALLPAPER_TITLE)
                    detailText:@""
       accessibilityIdentifier:kStartPageWallpaperSettingsCellId];

  return wallpaperSettingsItem;
}

- (TableViewDetailIconItem*)openStartPageWithSettingsItem {
  if (!_openStartPageWithSettingsItem) {
    _openStartPageWithSettingsItem =
        [self detailItemWithType:SettingsItemTypeStartPageOpenWithItem
                               text:GetNSString(
                                  IDS_IOS_START_PAGE_START_PAGE_OPEN_WITH_TITLE)
                         detailText:
                [self titleForReopenStartPageWithItem:_startPageStartWithItem]
            accessibilityIdentifier:kStartPageOpenWithSettingsCellId];
  }
  return _openStartPageWithSettingsItem;
}

- (TableViewSwitchItem*)customizeStartPageTableItem {
  if (!_customizeStartPageItem) {
    _customizeStartPageItem = [[TableViewSwitchItem alloc]
        initWithType:SettingsItemTypeCustomizeStartPage];
    _customizeStartPageItem.text =
        GetNSString(IDS_IOS_SHOW_START_PAGE_SETTINGS_CUSTOMIZE_TITLE);
    _customizeStartPageItem.on = self.customizeStartPage;
    _customizeStartPageItem.accessibilityIdentifier =
        kStartPageCustomizeStartPageSettingsCellId;
  }
  return _customizeStartPageItem;
}

#pragma mark - Navigation Actions

- (void)showStartPageLayoutSettings {
  _layoutSettingsCoordinator =
      [[VivaldiStartPageLayoutSettingsCoordinator alloc]
          initWithBaseNavigationController:self.navigationController
                                   browser:_browser];
  [_layoutSettingsCoordinator start];
}

- (void)showWallpaperSettings {
  UIViewController *controller =
      [VivaldiWallpaperSettingsViewProvider
          makeViewControllerWithHorizontalLayout:NO];
  controller.title =
      l10n_util::GetNSString(IDS_IOS_VIVALDI_START_PAGE_WALLPAPER_TITLE);
  controller.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;
  [self.navigationController pushViewController:controller animated:YES];
}

- (void)showOpenWithSettings {
  _reopenWithSettingsCoordinator =
      [[VivaldiStartPageReopenWithCoordinator alloc]
          initWithBaseNavigationController:self.navigationController
                                   browser:_browser];
  [_reopenWithSettingsCoordinator start];
}

#pragma mark - Helpers

- (NSString*)titleForCurrentLayout:(VivaldiStartPageLayoutStyle)style {
  switch (style) {
    case VivaldiStartPageLayoutStyleLarge:
      return GetNSString(IDS_IOS_VIVALDI_START_PAGE_LAYOUT_LARGE);
    case VivaldiStartPageLayoutStyleMedium:
      return GetNSString(IDS_IOS_VIVALDI_START_PAGE_LAYOUT_MEDIUM);
    case VivaldiStartPageLayoutStyleSmall:
      return GetNSString(IDS_IOS_VIVALDI_START_PAGE_LAYOUT_SMALL);
    case VivaldiStartPageLayoutStyleList:
      return GetNSString(IDS_IOS_VIVALDI_START_PAGE_LAYOUT_LIST);
    default:
      return GetNSString(IDS_IOS_VIVALDI_START_PAGE_LAYOUT_SMALL);
  }
}

- (NSString*)titleForReopenStartPageWithItem:
      (VivaldiStartPageStartItemType)item {
  switch (item) {
    case VivaldiStartPageStartItemTypeFirstGroup:
      return GetNSString(
            IDS_IOS_START_PAGE_START_PAGE_OPEN_WITH_FIRST_GROUP_TITLE);
    case VivaldiStartPageStartItemTypeTopSites:
      return GetNSString(
            IDS_IOS_START_PAGE_START_PAGE_OPEN_WITH_TOP_SITES_TITLE);
    case VivaldiStartPageStartItemTypeLastVisited:
      return GetNSString(
            IDS_IOS_START_PAGE_START_PAGE_OPEN_WITH_LAST_VISITED_GROUP_TITLE);
    default:
      return GetNSString(
            IDS_IOS_START_PAGE_START_PAGE_OPEN_WITH_FIRST_GROUP_TITLE);
  }
}

#pragma mark Item Constructors

- (TableViewDetailIconItem*)detailItemWithType:(NSInteger)type
                                          text:(NSString*)text
                                    detailText:(NSString*)detailText
                       accessibilityIdentifier:
                           (NSString*)accessibilityIdentifier {
  TableViewDetailIconItem* detailItem =
      [[TableViewDetailIconItem alloc] initWithType:type];
  detailItem.text = text;
  detailItem.detailText = detailText;
  detailItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  detailItem.accessibilityTraits |= UIAccessibilityTraitButton;
  detailItem.accessibilityIdentifier = accessibilityIdentifier;
  return detailItem;
}

@end
