// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/search_engine/vivaldi_search_engine_settings_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/search_engines/template_url_service_observer.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/util.h"
#import "ios/chrome/browser/search_engines/search_engine_observer_bridge.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/search_engine_table_view_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

namespace {
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSearchEngineList = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  SettingsItemTypeRegularSearchEngine = kItemTypeEnumZero,
  SettingsItemTypePrivateSearchEngine,
};

NSString* const kRegularTabsSearchEngineCellId =
    @"kRegularTabsSearchEngineCellId";
NSString* const kPrivateTabsSearchEngineCellId =
    @"kPrivateTabsSearchEngineCellId";

}  // namespace

@interface VivaldiSearchEngineSettingsViewController() <SearchEngineObserving> {
  ChromeBrowserState* _browserState;  // weak
  TemplateURLService* _templateURLService;  // weak
  // Bridge for TemplateURLServiceObserver.
  std::unique_ptr<SearchEngineObserverBridge> _observer;

  TableViewDetailIconItem* _regularSearchEngineItem;
  TableViewDetailIconItem* _privateSearchEngineItem;

  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;
}

@end

@implementation VivaldiSearchEngineSettingsViewController

#pragma mark - Initialization

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState {
  DCHECK(browserState);

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _browserState = browserState;
    _templateURLService =
        ios::TemplateURLServiceFactory::GetForBrowserState(browserState);
    _observer =
        std::make_unique<SearchEngineObserverBridge>(self, _templateURLService);
    _templateURLService->Load();
    [self setTitle:l10n_util::GetNSString(IDS_IOS_SEARCH_ENGINE_SETTING_TITLE)];
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
  [model addSectionWithIdentifier:SectionIdentifierSearchEngineList];

  [model addItem:[self regularSearchEngineDetailItem]
      toSectionWithIdentifier:SectionIdentifierSearchEngineList];
  [model addItem:[self privateSearchEngineDetailItem]
      toSectionWithIdentifier:SectionIdentifierSearchEngineList];
}


#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {

  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  SearchEngineTableViewController* controller;

  switch (itemType) {
    case SettingsItemTypeRegularSearchEngine:
      controller =
          [[SearchEngineTableViewController alloc]
              initWithBrowserState:_browserState isPrivate:NO];
      break;
    case SettingsItemTypePrivateSearchEngine:
      controller =
          [[SearchEngineTableViewController alloc]
              initWithBrowserState:_browserState isPrivate:YES];
      break;
    default:
      break;
  }
  [self.navigationController pushViewController:controller animated:YES];
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  _regularSearchEngineItem.detailText =
      [self defaultSearchEngineNameForType:
                        TemplateURLService::kDefaultSearchMain];

  _privateSearchEngineItem.detailText =
      [self defaultSearchEngineNameForType:
                        TemplateURLService::kDefaultSearchPrivate];

  [self reconfigureCellsForItems:@[ _regularSearchEngineItem,
                                    _privateSearchEngineItem ]];
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

  // Remove observer bridges.
  _observer.reset();

  _browserState = nullptr;
  _templateURLService = nullptr;

  _settingsAreDismissed = YES;
}

#pragma mark - Private Methods
- (TableViewItem*)regularSearchEngineDetailItem {
  NSString* regularDefaultSearchEngineName =
      [self defaultSearchEngineNameForType:
            TemplateURLService::kDefaultSearchMain];

  _regularSearchEngineItem =
      [self detailItemWithType:SettingsItemTypeRegularSearchEngine
                             text:l10n_util::GetNSString(
                        IDS_VIVALDI_SEARCH_ENGINE_SETTINGS_STANDARD_TAB_TITLE)
                       detailText:regularDefaultSearchEngineName
                           symbol:nil
          accessibilityIdentifier:kRegularTabsSearchEngineCellId];

  return _regularSearchEngineItem;
}

- (TableViewItem*)privateSearchEngineDetailItem {
  NSString* privateDefaultSearchEngineName =
      [self defaultSearchEngineNameForType:
            TemplateURLService::kDefaultSearchPrivate];

  _privateSearchEngineItem =
      [self detailItemWithType:SettingsItemTypePrivateSearchEngine
                             text:l10n_util::GetNSString(
                        IDS_VIVALDI_SEARCH_ENGINE_SETTINGS_PRIVATE_TAB_TITLE)
                       detailText:privateDefaultSearchEngineName
                           symbol:nil
          accessibilityIdentifier:kPrivateTabsSearchEngineCellId];

  return _privateSearchEngineItem;
}

- (NSString*)defaultSearchEngineNameForType:
    (TemplateURLService::DefaultSearchType)type {
  DCHECK(!_templateURLService);

  const TemplateURL* const default_provider =
      _templateURLService->GetDefaultSearchProvider(type);

  if (!default_provider) {
    return @"";
  }
  return base::SysUTF16ToNSString(default_provider->short_name());
}

#pragma mark Item Constructors

- (TableViewDetailIconItem*)detailItemWithType:(NSInteger)type
                                          text:(NSString*)text
                                    detailText:(NSString*)detailText
                                        symbol:(UIImage*)symbol
                       accessibilityIdentifier:
                           (NSString*)accessibilityIdentifier {
  TableViewDetailIconItem* detailItem =
      [[TableViewDetailIconItem alloc] initWithType:type];
  detailItem.text = text;
  detailItem.detailText = detailText;
  detailItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  detailItem.accessibilityTraits |= UIAccessibilityTraitButton;
  detailItem.accessibilityIdentifier = accessibilityIdentifier;
  detailItem.iconImage = symbol;
  return detailItem;
}

@end
