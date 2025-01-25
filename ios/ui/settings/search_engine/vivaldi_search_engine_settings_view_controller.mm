// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/search_engine/vivaldi_search_engine_settings_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/search_engine_table_view_controller.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

namespace {
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSearchEngineList = kSectionIdentifierEnumZero,
  SectionIdentifierSearchSuggestions,
};

typedef NS_ENUM(NSInteger, ItemType) {
  SettingsItemTypeRegularSearchEngine = kItemTypeEnumZero,
  SettingsItemTypePrivateSearchEngine,
  SettingsItemTypeSearchEngineNickname,
  SettingsItemTypeSearchSuggestions,
  SettingsItemTypeSearchSuggestionsFooter,
};

NSString* const kRegularTabsSearchEngineCellId =
    @"kRegularTabsSearchEngineCellId";
NSString* const kPrivateTabsSearchEngineCellId =
    @"kPrivateTabsSearchEngineCellId";

}  // namespace

@interface VivaldiSearchEngineSettingsViewController() {
  ProfileIOS* _profile;  // weak

  TableViewDetailIconItem* _regularSearchEngineItem;
  TableViewDetailIconItem* _privateSearchEngineItem;
  TableViewSwitchItem* _enableSearchSuggestionsToggleItem;
  TableViewSwitchItem* _enableNicknameToggleItem;

  NSString* _regularTabsSearchEngine;
  NSString* _privateTabsSearchEngine;
  BOOL _nicknameEnabled;
  BOOL _searchSuggestionsEnabled;

  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;
}

@end

@implementation VivaldiSearchEngineSettingsViewController

#pragma mark - Initialization

- (instancetype)initWithProfile:(ProfileIOS*)profile {
  DCHECK(profile);

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _profile = profile;
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
  [model addSectionWithIdentifier:SectionIdentifierSearchSuggestions];

  [model addItem:[self regularSearchEngineDetailItem]
      toSectionWithIdentifier:SectionIdentifierSearchEngineList];
  [model addItem:[self privateSearchEngineDetailItem]
      toSectionWithIdentifier:SectionIdentifierSearchEngineList];
  [model addItem:[self searchEngineNicknameToggleItem]
      toSectionWithIdentifier:SectionIdentifierSearchEngineList];

  [model addItem:[self searchSuggestionsToggleItem]
      toSectionWithIdentifier:SectionIdentifierSearchSuggestions];
  TableViewLinkHeaderFooterItem* footer =
      [[TableViewLinkHeaderFooterItem alloc]
          initWithType:SettingsItemTypeSearchSuggestionsFooter];
  footer.forceIndents = YES;
  footer.text =
      l10n_util::GetNSString(
          IDS_VIVALDI_SEARCH_ENGINE_ENABLE_SEARCH_SUGGESTION_DESCRIPTION);
  [model setFooter:footer
      forSectionWithIdentifier:SectionIdentifierSearchSuggestions];
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
              initWithProfile:_profile isPrivate:NO];
      break;
    case SettingsItemTypePrivateSearchEngine:
      controller =
          [[SearchEngineTableViewController alloc]
              initWithProfile:_profile isPrivate:YES];
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
    case SettingsItemTypeSearchSuggestions: {
      TableViewSwitchCell* switchCell =
          base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView
          addTarget:self
              action:@selector(searchSuggestionToggleChanged:)
                  forControlEvents:UIControlEventValueChanged];
      break;
    }
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

  _searchSuggestionsEnabled = NO;
  _nicknameEnabled = YES;
  _settingsAreDismissed = YES;
}

#pragma mark VivaldiSearchEngineSettingsConsumer

- (void)setSearchEngineForRegularTabs:(NSString*)searchEngine {
  _regularTabsSearchEngine = searchEngine;
  if (!_regularSearchEngineItem)
    return;
  _regularSearchEngineItem.detailText = searchEngine;
  [self reconfigureCellsForItems:@[ _regularSearchEngineItem]];
}

- (void)setSearchEngineForPrivateTabs:(NSString*)searchEngine {
  _privateTabsSearchEngine = searchEngine;
  if (!_privateSearchEngineItem)
    return;
  _privateSearchEngineItem.detailText = searchEngine;
  [self reconfigureCellsForItems:@[ _privateSearchEngineItem ]];
}

- (void)setPreferenceForEnableSearchSuggestions:(BOOL)enable {
  _searchSuggestionsEnabled = enable;
  if (!_enableSearchSuggestionsToggleItem) {
    return;
  }
  _enableSearchSuggestionsToggleItem.on = _searchSuggestionsEnabled;
}

- (void)setPreferenceForEnableSearchEngineNickname:(BOOL)enable {
  _nicknameEnabled = enable;
  if (!_enableNicknameToggleItem) {
    return;
  }
  _enableNicknameToggleItem.on = _nicknameEnabled;
}

#pragma mark - Private Methods
- (TableViewItem*)regularSearchEngineDetailItem {
  if (!_regularSearchEngineItem) {
    _regularSearchEngineItem =
        [self detailItemWithType:SettingsItemTypeRegularSearchEngine
                            text:l10n_util::GetNSString(
                       IDS_VIVALDI_SEARCH_ENGINE_SETTINGS_STANDARD_TAB_TITLE)
                      detailText:_regularTabsSearchEngine
                          symbol:nil
         accessibilityIdentifier:kRegularTabsSearchEngineCellId];
  }
  return _regularSearchEngineItem;
}

- (TableViewItem*)privateSearchEngineDetailItem {
  if (!_privateSearchEngineItem) {
    _privateSearchEngineItem =
        [self detailItemWithType:SettingsItemTypePrivateSearchEngine
                            text:l10n_util::GetNSString(
                        IDS_VIVALDI_SEARCH_ENGINE_SETTINGS_PRIVATE_TAB_TITLE)
                      detailText:_privateTabsSearchEngine
                          symbol:nil
         accessibilityIdentifier:kPrivateTabsSearchEngineCellId];

  }
  return _privateSearchEngineItem;
}

- (TableViewSwitchItem*)searchSuggestionsToggleItem {
  if (!_enableSearchSuggestionsToggleItem) {
    _enableSearchSuggestionsToggleItem =
        [[TableViewSwitchItem alloc]
            initWithType:SettingsItemTypeSearchSuggestions];
    NSString* title =
        l10n_util::GetNSString(
            IDS_VIVALDI_SEARCH_ENGINE_ENABLE_SEARCH_SUGGESTION_TITLE);
    _enableSearchSuggestionsToggleItem.text = title;
    _enableSearchSuggestionsToggleItem.on = _searchSuggestionsEnabled;
    _enableSearchSuggestionsToggleItem.accessibilityIdentifier = title;
  }
  return _enableSearchSuggestionsToggleItem;
}

- (TableViewSwitchItem*)searchEngineNicknameToggleItem {
  if (!_enableNicknameToggleItem) {
    _enableNicknameToggleItem =
        [[TableViewSwitchItem alloc]
            initWithType:SettingsItemTypeSearchEngineNickname];
    NSString* title =
        l10n_util::GetNSString(
             IDS_VIVALDI_SEARCH_ENGINE_SETTINGS_ENABLE_NICKNAME_TITLE);
    _enableNicknameToggleItem.text = title;
    _enableNicknameToggleItem.on = _nicknameEnabled;
    _enableNicknameToggleItem.accessibilityIdentifier = title;
  }
  return _enableNicknameToggleItem;
}

- (void)searchSuggestionToggleChanged:(UISwitch*)switchView {
  [self.delegate searchSuggestionsEnabled:switchView.isOn];
}

- (void)searchEngineNicknameToggleChanged:(UISwitch*)switchView {
  [self.delegate searchEngineNicknameEnabled:switchView.isOn];
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
