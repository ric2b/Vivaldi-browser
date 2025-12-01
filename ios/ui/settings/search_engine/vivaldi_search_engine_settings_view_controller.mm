// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/search_engine/vivaldi_search_engine_settings_view_controller.h"

#import "base/apple/foundation_util.h"
#import "browser/features/vivaldi_features.h"
#import "ios/chrome/browser/settings/ui_bundled/search_engine_table_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/ui/settings/search_engine/editor/vivaldi_search_engine_editor_coordinator.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

namespace {
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSearchEngineList = kSectionIdentifierEnumZero,
  SectionIdentifierCustomSearchEngine,
};

typedef NS_ENUM(NSInteger, ItemType) {
  SettingsItemTypeRegularSearchEngine = kItemTypeEnumZero,
  SettingsItemTypePrivateSearchEngine,
  SettingsItemTypeSearchEngineNickname,

  SettingsItemTypeAddCustomSearchEngine,
};

NSString* const kRegularTabsSearchEngineCellId =
    @"kRegularTabsSearchEngineCellId";
NSString* const kPrivateTabsSearchEngineCellId =
    @"kPrivateTabsSearchEngineCellId";

}  // namespace

@interface VivaldiSearchEngineSettingsViewController () <
    VivaldiSearchEngineEditorCoordinatorDelegate> {
  Browser* _browser;     // weak
  ProfileIOS* _profile;  // weak

  TableViewDetailIconItem* _regularSearchEngineItem;
  TableViewDetailIconItem* _privateSearchEngineItem;
  TableViewSwitchItem* _enableNicknameToggleItem;
  TableViewDetailIconItem* _addCustomSearchEngineItem;

  NSString* _regularTabsSearchEngine;
  NSString* _privateTabsSearchEngine;
  BOOL _nicknameEnabled;

  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;

  // Coordinator for the editor
  VivaldiSearchEngineEditorCoordinator* _editorCoordinator;
}

@end

@implementation VivaldiSearchEngineSettingsViewController

#pragma mark - Initialization

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _browser = browser;
    _profile = browser->GetProfile();
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  [self loadModel];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.presentationDelegate
        searchEngineSettingsViewControllerWasRemoved:self];
  }
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
  [model addItem:[self searchEngineNicknameToggleItem]
      toSectionWithIdentifier:SectionIdentifierSearchEngineList];

  if (vivaldi_features::IsAddCustomSearchEngineEnabled()) {
    [model addSectionWithIdentifier:SectionIdentifierCustomSearchEngine];
    [model addItem:[self addCustomSearchEngineItem]
        toSectionWithIdentifier:SectionIdentifierCustomSearchEngine];
  }
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  SearchEngineTableViewController* controller;

  switch (itemType) {
    case SettingsItemTypeRegularSearchEngine:
      controller =
          [[SearchEngineTableViewController alloc] initWithProfile:_profile
                                                           browser:_browser
                                                         isPrivate:NO];
      break;
    case SettingsItemTypePrivateSearchEngine:
      controller =
          [[SearchEngineTableViewController alloc] initWithProfile:_profile
                                                           browser:_browser
                                                         isPrivate:YES];
      break;
    case SettingsItemTypeAddCustomSearchEngine:
      [self showAddSearchEngine];
      break;
    default:
      break;
  }
  [self.navigationController pushViewController:controller animated:YES];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  switch (itemType) {
    case SettingsItemTypeSearchEngineNickname: {
      TableViewSwitchCell* switchCell =
          base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView
                 addTarget:self
                    action:@selector(searchEngineNicknameToggleChanged:)
          forControlEvents:UIControlEventValueChanged];
      break;
    }
    default:
      break;
  }
  return cell;
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

  _profile = nullptr;

  _nicknameEnabled = YES;
  _settingsAreDismissed = YES;

  [_editorCoordinator stop];
  _editorCoordinator = nil;
}

#pragma mark VivaldiSearchEngineSettingsConsumer

- (void)setSearchEngineForRegularTabs:(NSString*)searchEngine {
  _regularTabsSearchEngine = searchEngine;
  if (!_regularSearchEngineItem)
    return;
  _regularSearchEngineItem.detailText = searchEngine;
  [self reconfigureCellsForItems:@[ _regularSearchEngineItem ]];
}

- (void)setSearchEngineForPrivateTabs:(NSString*)searchEngine {
  _privateTabsSearchEngine = searchEngine;
  if (!_privateSearchEngineItem)
    return;
  _privateSearchEngineItem.detailText = searchEngine;
  [self reconfigureCellsForItems:@[ _privateSearchEngineItem ]];
}

- (void)setPreferenceForEnableSearchEngineNickname:(BOOL)enable {
  _nicknameEnabled = enable;
  if (!_enableNicknameToggleItem) {
    return;
  }
  _enableNicknameToggleItem.on = _nicknameEnabled;
}

#pragma mark - VivaldiSearchEngineEditorCoordinatorDelegate

- (void)searchEngineEditorShouldDismiss:
    (VivaldiSearchEngineEditorCoordinator*)coordinator {
  _editorCoordinator.delegate = nil;
  [_editorCoordinator stop];
  _editorCoordinator = nil;
}

#pragma mark - Private Methods
- (TableViewItem*)regularSearchEngineDetailItem {
  if (!_regularSearchEngineItem) {
    _regularSearchEngineItem = [self
             detailItemWithType:SettingsItemTypeRegularSearchEngine
                  text:
                      l10n_util::GetNSString(
                          IDS_VIVALDI_SEARCH_ENGINE_SETTINGS_STANDARD_TAB_TITLE)
                     detailText:_regularTabsSearchEngine
                         symbol:nil
        accessibilityIdentifier:kRegularTabsSearchEngineCellId];
  }
  return _regularSearchEngineItem;
}

- (TableViewItem*)privateSearchEngineDetailItem {
  if (!_privateSearchEngineItem) {
    _privateSearchEngineItem = [self
             detailItemWithType:SettingsItemTypePrivateSearchEngine
                  text:
                      l10n_util::GetNSString(
                          IDS_VIVALDI_SEARCH_ENGINE_SETTINGS_PRIVATE_TAB_TITLE)
                     detailText:_privateTabsSearchEngine
                         symbol:nil
        accessibilityIdentifier:kPrivateTabsSearchEngineCellId];
  }
  return _privateSearchEngineItem;
}

- (TableViewSwitchItem*)searchEngineNicknameToggleItem {
  if (!_enableNicknameToggleItem) {
    _enableNicknameToggleItem = [[TableViewSwitchItem alloc]
        initWithType:SettingsItemTypeSearchEngineNickname];
    NSString* title = l10n_util::GetNSString(
        IDS_VIVALDI_SEARCH_ENGINE_SETTINGS_ENABLE_NICKNAME_TITLE);
    _enableNicknameToggleItem.text = title;
    _enableNicknameToggleItem.on = _nicknameEnabled;
    _enableNicknameToggleItem.accessibilityIdentifier = title;
  }
  return _enableNicknameToggleItem;
}

- (TableViewDetailIconItem*)addCustomSearchEngineItem {
  if (!_addCustomSearchEngineItem) {
    NSString* title = l10n_util::GetNSString(
        IDS_VIVALDI_SEARCH_ENGINE_SETTINGS_ADD_CUSTOM_ENGINE_TITLE);
    _addCustomSearchEngineItem =
        [self detailItemWithType:SettingsItemTypeAddCustomSearchEngine
                               text:title
                         detailText:nil
                             symbol:nil
            accessibilityIdentifier:title];
  }
  return _addCustomSearchEngineItem;
}

- (void)searchEngineNicknameToggleChanged:(UISwitch*)switchView {
  [self.delegate searchEngineNicknameEnabled:switchView.isOn];
}

#pragma mark - Private
- (void)showAddSearchEngine {
  _editorCoordinator = [[VivaldiSearchEngineEditorCoordinator alloc]
      initWithBaseNavigationController:self.navigationController
                               browser:_browser
                             isEditing:NO
                           editingItem:nil];
  _editorCoordinator.delegate = self;
  [_editorCoordinator start];
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
