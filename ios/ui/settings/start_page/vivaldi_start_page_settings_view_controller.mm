// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/start_page/vivaldi_start_page_settings_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/ui/settings/start_page/layout_settings/vivaldi_start_page_layout_settings_view_controller.h"
#import "ios/ui/settings/start_page/vivaldi_start_page_prefs.h"
#import "ios/ui/settings/start_page/wallpaper_settings/wallpaper_settings_swift.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

using l10n_util::GetNSString;

namespace {
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierStartPageSettings = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  SettingsItemTypeStartPageLayout = kItemTypeEnumZero,
  SettingsItemTypeWallpaper,
};

NSString* const kStartPageLayoutSettingsCellId =
    @"kStartPageLayoutSettingsCellId";
NSString* const kStartPageWallpaperSettingsCellId =
    @"kStartPageWallpaperSettingsCellId";

}  // namespace

@interface VivaldiStartPageSettingsViewController() {
  // The browser where the settings are being displayed.
  Browser* _browser;
  // Layout setting item reference to modify with observer state.
  TableViewDetailIconItem* _layoutSettingsItem;
  // Current layout settings
  VivaldiStartPageLayoutStyle _startPageLayout;
  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;
}

@end

@implementation VivaldiStartPageSettingsViewController

#pragma mark - Initialization

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _browser = browser;
    [VivaldiStartPagePrefs
        setPrefService:_browser->GetBrowserState()->GetPrefs()];
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

  [model addItem:[self layoutSettingsItem]
      toSectionWithIdentifier:SectionIdentifierStartPageSettings];
  [model addItem:[self wallpaperSettingsItem]
      toSectionWithIdentifier:SectionIdentifierStartPageSettings];
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
    default:
      break;
  }
}

#pragma mark - VivaldiStartPageSettingsConsumer
- (void)setPreferenceForStartPageLayout:
    (VivaldiStartPageLayoutStyle)layoutStyle {
  _startPageLayout = layoutStyle;
  if (!_layoutSettingsItem)
    return;
  _layoutSettingsItem.detailText =
      [self titleForCurrentLayout:layoutStyle];
  [self reconfigureCellsForItems:@[_layoutSettingsItem]];
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
  _settingsAreDismissed = YES;
}

#pragma mark - Private Methods
- (TableViewDetailIconItem*)layoutSettingsItem {
  _layoutSettingsItem =
      [self detailItemWithType:SettingsItemTypeStartPageLayout
                             text:l10n_util::GetNSString(
                                IDS_IOS_VIVALDI_START_PAGE_LAYOUT_TITLE)
                       detailText:@""
          accessibilityIdentifier:kStartPageLayoutSettingsCellId];
  [self setPreferenceForStartPageLayout:_startPageLayout];
  return _layoutSettingsItem;
}

- (TableViewDetailIconItem*)wallpaperSettingsItem {
  TableViewDetailIconItem* wallpaperSettingsItem =
      [self detailItemWithType:SettingsItemTypeWallpaper
                          text:l10n_util::GetNSString(
                                IDS_IOS_VIVALDI_START_PAGE_WALLPAPER_TITLE)
                    detailText:@""
       accessibilityIdentifier:kStartPageWallpaperSettingsCellId];

  return wallpaperSettingsItem;
}

- (void)showStartPageLayoutSettings {
  VivaldiStartPageLayoutSettingsViewController* controller =
      [[VivaldiStartPageLayoutSettingsViewController alloc]
          initWithTitle:
              l10n_util::GetNSString(IDS_IOS_VIVALDI_START_PAGE_LAYOUT_TITLE)
                browser:_browser];
  controller.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;
  [self.navigationController pushViewController:controller animated:YES];
}

- (void)showWallpaperSettings {
  UIViewController *controller =
      [VivaldiWallpaperSettingsViewProvider makeViewController];
  controller.title =
      l10n_util::GetNSString(IDS_IOS_VIVALDI_START_PAGE_WALLPAPER_TITLE);
  controller.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;
  [self.navigationController pushViewController:controller animated:YES];
}

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
