// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ad_tracker_blocker/settings/vivaldi_atb_source_settings_view_controller.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/ad_tracker_blocker/cells/vivaldi_atb_source_setting_item.h"
#import "ios/ui/ad_tracker_blocker/manager/vivaldi_atb_manager.h"
#import "ios/ui/ad_tracker_blocker/settings/vivaldi_atb_add_domain_source_view_controller.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_constants.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_domain_source_mode.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_item.h"
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
  SectionIdentifierSources = kSectionIdentifierEnumZero
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSource = kItemTypeEnumZero
};

}

@interface VivaldiATBSourceSettingsViewController()<VivaldiATBConsumer>
// The source type, e.g. Ads, Trackers
@property(nonatomic,assign) ATBSourceType sourceType;
// The manager for the adblock that provides all methods and properties for
// adblocker.
@property(nonatomic, strong) VivaldiATBManager* adblockManager;
// The Browser in which blocker engine is active.
@property(nonatomic, assign) Browser* browser;
@end


@implementation VivaldiATBSourceSettingsViewController
@synthesize sourceType = _sourceType;
@synthesize adblockManager = _adblockManager;
@synthesize browser = _browser;

#pragma mark - INITIALIZER
- (instancetype)initWithBrowser:(Browser*)browser
                          title:(NSString*)title
                     sourceType:(ATBSourceType)sourceType {
  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    _sourceType = sourceType;
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
  [self getSourceList];
}


#pragma mark - PRIVATE
- (void)setUpTableViewFooter {
  UIView* footerView = [UIView new];
  footerView.frame = CGRectMake(0, 0,
                                self.view.bounds.size.width,
                                tableFooterHeight);
  self.tableView.tableFooterView = footerView;

  UIButton* addSourceButton = [UIButton new];
  addSourceButton.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  addSourceButton.layer.cornerRadius = actionButtonCornerRadius;

  NSString* buttonTitleString = GetNSString(IDS_ADD_NEW_SOURCE);
  [addSourceButton setTitle:buttonTitleString
                   forState:UIControlStateNormal];
  [addSourceButton
     setTitleColor:UIColor.vSystemBlue
     forState:UIControlStateNormal];
  [addSourceButton addTarget:self
                      action:@selector(handleAddSourceButtonTap)
            forControlEvents:UIControlEventTouchUpInside];
  [footerView addSubview:addSourceButton];
  [addSourceButton fillSuperviewToSafeAreaInsetWithPadding:actionButtonPadding];
}

#pragma mark ACTIONS
- (void)handleAddSourceButtonTap {
  NSString* titleString = GetNSString(IDS_ADD_NEW_SOURCE);

  VivaldiATBAddDomainSourceViewController* controller =
  [[VivaldiATBAddDomainSourceViewController alloc]
         initWithBrowser:_browser
                   title:titleString
                  source:_sourceType
             editingMode:ATBAddingModeSource
           editingDomain:nil
     siteSpecificSetting:ATBSettingNone];
  [self.navigationController pushViewController:controller animated:YES];
}

- (void)initializeAdblockManager {
  if (self.adblockManager || !_browser)
    return;
  self.adblockManager = [[VivaldiATBManager alloc] initWithBrowser:_browser];
  self.adblockManager.consumer = self;
}

- (void)getSourceList {
  if (self.adblockManager || !_browser)
    [self initializeAdblockManager];
  [self.adblockManager getBlockerSourcesForSourceType:_sourceType];
}

- (VivaldiATBSourceItem*)getSourceForId:(uint32_t)sourceId {
  if (self.adblockManager || !_browser)
    [self initializeAdblockManager];
  VivaldiATBSourceItem* sourceItem =
      [self.adblockManager getBlockerSourceForSourceId:sourceId
                                            sourceType:_sourceType];
  return sourceItem;
}

-(void)reloadModelWithSources:(NSArray*)sources {
  TableViewModel* model = self.tableViewModel;

  // Delete any existing section.
  if ([model
          hasSectionForSectionIdentifier:SectionIdentifierSources])
    [model
        removeSectionWithIdentifier:SectionIdentifierSources];

  // Creates section for sources list
  [model
      addSectionWithIdentifier:SectionIdentifierSources];

  for (id item in sources) {
    VivaldiATBSourceItem* source = static_cast<VivaldiATBSourceItem*>(item);
    VivaldiATBSourceSettingItem* tableViewItem =
        [[VivaldiATBSourceSettingItem alloc] initWithType:ItemTypeSource];
    tableViewItem.source = source;
    tableViewItem.text = source.title;
    tableViewItem.detailText = [source subtitle];
    tableViewItem.on = source.is_enabled;
    tableViewItem.useCustomSeparator = YES;

    [model addItem:tableViewItem
         toSectionWithIdentifier:SectionIdentifierSources];
  }

  [self.tableView reloadData];
}

- (void)updateSourceWithId:(uint32_t)sourceId {
  TableViewModel* model = self.tableViewModel;
  NSArray<VivaldiATBSourceSettingItem*>* sources =
      [model itemsInSectionWithIdentifier:SectionIdentifierSources];

  for (id source in sources) {
    VivaldiATBSourceSettingItem* sourceItem =
        static_cast<VivaldiATBSourceSettingItem*>(source);
    if (!sourceItem)
      return;
    if (sourceItem.source.key == sourceId) {
      VivaldiATBSourceItem* updatedSourceItem = [self getSourceForId:sourceId];
      if (!updatedSourceItem)
        return;

      sourceItem.source = updatedSourceItem;
      sourceItem.text = updatedSourceItem.title;
      sourceItem.detailText = [updatedSourceItem subtitle];
      sourceItem.on = updatedSourceItem.is_enabled;

      NSIndexPath* index = [model indexPathForItem:sourceItem];
      if (!index)
        return;
      [self.tableView
          reloadRowsAtIndexPaths:@[ index ]
                withRowAnimation:UITableViewRowAnimationAutomatic];
    }
  }
}

#pragma mark Switch Actions

- (void)sourceStateToggled:(UISwitch*)sender {
  TableViewModel* model = self.tableViewModel;

  NSIndexPath* indexPath = [model indexPathForItemType:ItemTypeSource
                                     sectionIdentifier:SectionIdentifierSources
                                               atIndex:sender.tag];
  DCHECK(indexPath);
  VivaldiATBSourceSettingItem* switchItem =
      base::apple::ObjCCastStrict<VivaldiATBSourceSettingItem>(
          [model itemAtIndexPath:indexPath]);

  BOOL isEnabled = switchItem.source.is_enabled;

  // Enable/Disable action
  if (!self.adblockManager)
    return;
  if (isEnabled)
    [self.adblockManager
        disableRuleSourceForKey:switchItem.source.key
                     sourceType:self.sourceType];
  else
    [self.adblockManager
        enableRuleSourceForKey:switchItem.source.key
                    sourceType:self.sourceType];

  BOOL newSwitchValue = sender.isOn;
  switchItem.on = newSwitchValue;
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {

  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  if ([cell isKindOfClass:[TableViewSwitchCell class]]) {
    TableViewSwitchCell* switchCell =
        base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(sourceStateToggled:)
                    forControlEvents:UIControlEventValueChanged];
    switchCell.switchView.tag = indexPath.row;
  }

  return cell;
}

#pragma mark - CONTEXT MENU
- (UIContextMenuConfiguration*)tableView:(UITableView*)tableView
    contextMenuConfigurationForRowAtIndexPath:(NSIndexPath*)indexPath
                                        point:(CGPoint)point {
  return [self contextMenuForIndexPath:indexPath];
}

-(UIContextMenuConfiguration*)contextMenuForIndexPath:(NSIndexPath*)indexPath {
  TableViewModel* model = self.tableViewModel;

  TableViewItem* selectedItem = [model itemAtIndexPath:indexPath];
  VivaldiATBSourceSettingItem* selectedSettingItem =
      base::apple::ObjCCastStrict<VivaldiATBSourceSettingItem>(selectedItem);
  BOOL isEnabled = selectedSettingItem.source.is_enabled;
  BOOL isRemoveable = !selectedSettingItem.source.is_default;

  UIContextMenuConfiguration* config =
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:^UIMenu*
   _Nullable(NSArray<UIMenuElement*>* _Nonnull settingActions) {

        NSString* enableActionTitle =
            GetNSString(IDS_VIVALDI_IOS_RULE_SOURCE_ENABLE);
        NSString* disableActionTitle =
            GetNSString(IDS_VIVALDI_IOS_RULE_SOURCE_DISABLE);
        UIAction * stateChangeAction =
          [UIAction actionWithTitle:isEnabled ?
                                    disableActionTitle : enableActionTitle
                              image:nil
                         identifier:nil
                            handler:^(__kindof UIAction* _Nonnull action) {
            if (!self.adblockManager)
              return;
            if (isEnabled)
              [self.adblockManager
                  disableRuleSourceForKey:selectedSettingItem.source.key
                               sourceType:self.sourceType];
            else
              [self.adblockManager
                  enableRuleSourceForKey:selectedSettingItem.source.key
                              sourceType:self.sourceType];
          }];

        NSString* removeActionTitle =
            GetNSString(IDS_VIVALDI_IOS_RULE_SOURCE_REMOVE);
        UIAction * removeAction =
          [UIAction actionWithTitle:removeActionTitle
                              image:nil
                         identifier:nil
                            handler:^(__kindof UIAction* _Nonnull action) {
            if (!self.adblockManager)
              return;
            [self.adblockManager
                removeRuleSourceForKey:selectedSettingItem.source.key
                            sourceType:self.sourceType];
          }];
        removeAction.attributes = UIMenuElementAttributesDestructive;

        NSString* restoreActionTitle =
            GetNSString(IDS_VIVALDI_IOS_RULE_SOURCE_RESTORE_LISTS);
        UIAction * restoreAction =
          [UIAction actionWithTitle:restoreActionTitle
                              image:nil
                         identifier:nil
                            handler:^(__kindof UIAction* _Nonnull action) {
            if (!self.adblockManager)
              return;
            [self.adblockManager restoreRuleSourceForType:self.sourceType];
          }];
        restoreAction.attributes = UIMenuElementAttributesDestructive;

        UIMenu* menu;
        switch (self.sourceType) {
          case ATBSourceTrackers: {
            if (isRemoveable) {
              menu = [UIMenu menuWithTitle:@"" children:@[
                stateChangeAction, removeAction
              ]];
            } else {
              menu = [UIMenu menuWithTitle:@"" children:@[
                stateChangeAction
              ]];
            }
            break;
          }
          case ATBSourceAds: {
            if (isRemoveable) {
              menu = [UIMenu menuWithTitle:@"" children:@[
                stateChangeAction, removeAction, restoreAction
              ]];
            } else {
              menu = [UIMenu menuWithTitle:@"" children:@[
                stateChangeAction, restoreAction
              ]];
            }
            break;
          }
          default:
            break;
        }
    return menu;
  }];

  return config;
}

#pragma mark: - VivaldiATBConsumer

- (void)didRefreshSourcesList:(NSArray*)sources {
  [self reloadModelWithSources:sources];
}

- (void)ruleSourceDidUpdate:(uint32_t)key
                      group:(RuleGroup)group
                fetchResult:(ATBFetchResult)fetchResult {
  [self updateSourceWithId:key];
}

- (void)knownSourceDidEnable:(RuleGroup)group
                         key:(uint32_t)key {
  [self updateSourceWithId:key];
}

- (void)knownSourceDidDisable:(RuleGroup)group
                          key:(uint32_t)key {
  [self updateSourceWithId:key];
}

@end
