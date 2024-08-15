// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ad_tracker_blocker/settings/vivaldi_atb_add_domain_source_view_controller.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/ad_tracker_blocker/cells/vivaldi_atb_setting_item.h"
#import "ios/ui/ad_tracker_blocker/manager/vivaldi_atb_manager.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_constants.h"
#import "ios/ui/custom_views/vivaldi_text_field_view.h"
#import "ios/ui/helpers/vivaldi_colors_helper.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
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
  SectionIdentifierSettings = kSectionIdentifierEnumZero
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeGlobalSetting = kItemTypeEnumZero
};

}

// Namespace
namespace {
// Tableview header height
const CGFloat tableViewHeaderHeight = 90.f;
// Padding for the text view container when adding domain.
const UIEdgeInsets textViewContainerPaddingDomain =
  UIEdgeInsetsMake(12.f, 20.f, 22.f, 20.f);
// Padding for the text view container when adding source.
const UIEdgeInsets textViewContainerPaddingSource =
  UIEdgeInsetsMake(12.f, 20.f, 20.f, 20.f);
// Padding for the text view.
const UIEdgeInsets textViewPadding = UIEdgeInsetsMake(8.f, 0.f, 8.f, 0.f);
// Spacing between two buttons.
const CGFloat stackSpacing = 8.f;

UIButton* ActionButton() {
  UIButton* button = [UIButton new];
  button.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  button.layer.cornerRadius = actionButtonCornerRadius;
  [button setTitleColor:UIColor.vSystemBlue
               forState:UIControlStateNormal];
  return button;
}

}

@interface VivaldiATBAddDomainSourceViewController()<VivaldiATBConsumer>
// Textview for domain or source url.
@property(nonatomic,weak) VivaldiTextFieldView* textFieldView;
// Button for adding/editing domain or source
@property(nonatomic,weak) UIButton* addEditButton;
// Button for deleting domain
@property(nonatomic,weak) UIButton* deleteButton;
// The source type, e.g. Ads, Trackers
@property(nonatomic,assign) ATBSourceType source;
// The manager for the adblock that provides all methods and properties for
// adblocker.
@property(nonatomic, strong) VivaldiATBManager* adblockManager;
// The Browser in which blocker engine is active.
@property(nonatomic, assign) Browser* browser;
// The editing mode for view controller, e.g. adding domain or source.
@property(nonatomic,assign) ATBDomainSourceEditingMode editingMode;
// The domain currently editing. Optional, available in editing mode.
@property(nonatomic, assign) NSString* editingDomain;
// User preferred setting for the editing domain.
@property(nonatomic, assign) ATBSettingType siteSpecificSetting;
@end

@implementation VivaldiATBAddDomainSourceViewController
@synthesize textFieldView = _textFieldView;
@synthesize addEditButton = _addEditButton;
@synthesize deleteButton = _deleteButton;
@synthesize source = _source;
@synthesize editingMode = _editingMode;
@synthesize adblockManager = _adblockManager;
@synthesize browser = _browser;
@synthesize siteSpecificSetting = _siteSpecificSetting;

#pragma mark - INITIALIZER
- (instancetype)initWithBrowser:(Browser*)browser
                          title:(NSString*)title
                         source:(ATBSourceType)source
                    editingMode:(ATBDomainSourceEditingMode)editingMode
                  editingDomain:(NSString*)editingDomain
            siteSpecificSetting:(ATBSettingType)siteSpecificSetting {
  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    _browser = browser;
    _source = source;
    _editingMode = editingMode;
    _siteSpecificSetting = siteSpecificSetting;
    _editingDomain = editingDomain;
    self.title = title;
    self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
    [self setUpTableViewHeader];
    [self setUpTableViewFooter];
    [self setActionButtonTitle];
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
  if (self.editingMode != ATBAddingModeSource)
    [self loadATBOptions];
}


#pragma mark - PRIVATE
- (void)setUpTableViewHeader {
  UIView* headerView = [UIView new];
  headerView.frame = CGRectMake(0, 0,
                                self.view.bounds.size.width,
                                tableViewHeaderHeight);
  headerView.backgroundColor = UIColor.clearColor;

  UIView* textViewContainer = [UIView new];
  textViewContainer.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  textViewContainer.layer.cornerRadius = actionButtonCornerRadius;
  [headerView addSubview:textViewContainer];

  VivaldiTextFieldView* textFieldView =
    [[VivaldiTextFieldView alloc] initWithPlaceholder:@""];
  _textFieldView = textFieldView;
  [textFieldView setURLMode];
  [textViewContainer addSubview:textFieldView];
  [textFieldView fillSuperviewWithPadding:textViewPadding];
  [textFieldView setAutoCorrectDisabled:true];
  [textFieldView setFocus];

  switch (self.editingMode) {

    case ATBAddingModeDomain:
    case ATBEditingModeDomain: {
      [textFieldView setURLValidationType:URLTypeDomain];
      [textViewContainer
       fillSuperviewToSafeAreaInsetWithPadding:textViewContainerPaddingDomain];

      NSString* placeHolderStringDomain =
        GetNSString(IDS_AUTOFILL_DOMAIN_HINT);
      [textFieldView setPlaceholder:placeHolderStringDomain];
      [textFieldView setText:_editingDomain];
      break;
    }

    case ATBAddingModeSource: {
      textFieldView.validateScheme = YES;
      [textFieldView setURLValidationType:URLTypeGeneric];
      [textViewContainer
       fillSuperviewToSafeAreaInsetWithPadding:textViewContainerPaddingSource];

      NSString* placeHolderStringSource =
        GetNSString(IDS_AUTOFILL_IMPORT_URL_HINT);
      [textFieldView setPlaceholder:placeHolderStringSource];
      break;
    }

    default:
      break;
  }

  self.tableView.tableHeaderView = headerView;
}

- (void)setUpTableViewFooter {
  UIView* footerView = [UIView new];
  self.tableView.tableFooterView = footerView;

  switch (self.editingMode) {
    case ATBAddingModeDomain:
    case ATBAddingModeSource: {
      footerView.frame = CGRectMake(0, 0,
                                    self.view.bounds.size.width,
                                    tableFooterHeight);
      UIButton* addEditButton = ActionButton();
      _addEditButton = addEditButton;
      [addEditButton addTarget:self
                       action:@selector(handleAddEditButtonTap)
                      forControlEvents:UIControlEventTouchUpInside];
      [footerView addSubview:addEditButton];
      [addEditButton
          fillSuperviewToSafeAreaInsetWithPadding:actionButtonPadding];

      break;
    }

    case ATBEditingModeDomain: {
      footerView.frame = CGRectMake(0, 0,
                                    self.view.bounds.size.width,
                                    tableFooterHeight*2);
      UIButton* addEditButton = ActionButton();
      _addEditButton = addEditButton;
      [addEditButton addTarget:self
                       action:@selector(handleAddEditButtonTap)
                      forControlEvents:UIControlEventTouchUpInside];

      UIButton* deleteButton = ActionButton();
      _deleteButton = deleteButton;
      [deleteButton addTarget:self
                       action:@selector(handleDeleteButtonTap)
                      forControlEvents:UIControlEventTouchUpInside];

      UIStackView* buttonStack =
          [[UIStackView alloc] initWithArrangedSubviews:@[
            addEditButton, deleteButton
          ]];
      buttonStack.distribution = UIStackViewDistributionFillEqually;
      buttonStack.axis = UILayoutConstraintAxisVertical;
      buttonStack.spacing = stackSpacing;

      [footerView addSubview:buttonStack];
      [buttonStack fillSuperviewToSafeAreaInsetWithPadding:actionButtonPadding];

      break;
    }
    default:
      break;
  }
}

- (void)setActionButtonTitle {

  switch (self.editingMode) {
    case ATBAddingModeDomain: {
      NSString* domainButtonTitleString =
        GetNSString(IDS_ADD_NEW_DOMAIN);
      [self.addEditButton setTitle:domainButtonTitleString
                          forState:UIControlStateNormal];
      break;
    }

    case ATBEditingModeDomain: {
      NSString* updateString = GetNSString(IDS_VIVALDI_IOS_UPDATE_DOMAIN);
      [self.addEditButton setTitle:updateString
                          forState:UIControlStateNormal];

      NSString* removeString = GetNSString(IDS_VIVALDI_IOS_REMOVE_DOMAIN);
      [self.deleteButton setTitle:removeString
                          forState:UIControlStateNormal];
      break;
    }

    case ATBAddingModeSource: {
      NSString* importButtonTitleString =
        GetNSString(IDS_AUTOFILL_IMPORT_URL);
      [self.addEditButton setTitle:importButtonTitleString
                          forState:UIControlStateNormal];
      break;
    }

    default:
      break;
  }
}

- (void)initializeAdblockManager {
  if (self.adblockManager || !_browser)
    return;
  self.adblockManager = [[VivaldiATBManager alloc] initWithBrowser:_browser];
  self.adblockManager.consumer = self;
}

/// Create and start mediator to compute and  populate the ad and tracker
/// blocker setting options.
- (void)loadATBOptions {
  if (!self.adblockManager)
    [self initializeAdblockManager];
  [self.adblockManager getSettingOptions];
}

-(void)reloadTableViewWithItems:(NSArray*)items {

  TableViewModel* model = self.tableViewModel;

  // Delete any existing section.
  if ([model
          hasSectionForSectionIdentifier:SectionIdentifierSettings])
    [model
        removeSectionWithIdentifier:SectionIdentifierSettings];

  // Creates Section for the setting options
  [model
      addSectionWithIdentifier:SectionIdentifierSettings];

  for (id item in items) {
    VivaldiATBSettingItem* settingItem = [[VivaldiATBSettingItem alloc]
        initWithType:ItemTypeGlobalSetting];
    settingItem.item = item;
    settingItem.globalDefaultOption =
        [self.adblockManager globalBlockingSetting];
    settingItem.userPreferredOption = self.siteSpecificSetting;
    settingItem.showDefaultMarker = YES;

    // Show selection check
    if (settingItem.item.type == self.siteSpecificSetting) {
      settingItem.accessoryType = UITableViewCellAccessoryCheckmark;
    } else {
      settingItem.accessoryType = UITableViewCellAccessoryNone;
    }

    [model addItem:settingItem
         toSectionWithIdentifier:SectionIdentifierSettings];
  }

  [self.tableView reloadData];
}

#pragma mark ACTIONS
- (void)handleAddEditButtonTap {
  NSString* inputString = [_textFieldView getText];
  if (![_textFieldView hasText] ||
      !self.adblockManager)
    return;

  switch (self.editingMode) {
    case ATBAddingModeDomain: {
      if (![VivaldiGlobalHelpers isValidDomain:inputString])
        return;
      [self.adblockManager setExceptionForDomain:inputString
                                    blockingType:_siteSpecificSetting];
      break;
    }

    case ATBEditingModeDomain: {
      if (![VivaldiGlobalHelpers isValidDomain:inputString])
        return;
      if (![inputString isEqualToString:_editingDomain]) {
        // If domain is edited before update button tap then remove all
        // exceptions from the editingDomain first.
        [self.adblockManager removeExceptionForDomain:_editingDomain];
      }

      // Add new exception.
      [self.adblockManager setExceptionForDomain:inputString
                                    blockingType:_siteSpecificSetting];
      break;
    }
    case ATBAddingModeSource: {
      if (![VivaldiGlobalHelpers isValidURL:inputString])
        return;
      [self.adblockManager addRuleSource:inputString
                              sourceType:_source];
      break;
    }
    default:
      break;
  }

  [self.navigationController popViewControllerAnimated:YES];
}

/// Only available in exceptions editing mode.
- (void)handleDeleteButtonTap {
  NSString* domain = [_textFieldView getText];
  if (![_textFieldView hasText] ||
      !self.adblockManager ||
      ![VivaldiGlobalHelpers isValidDomain:domain])
    return;
  [self.adblockManager removeExceptionForDomain:domain];

  [self.navigationController popViewControllerAnimated:YES];
}

#pragma mark - UITABLEVIEW DELEGATE

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  TableViewModel* model = self.tableViewModel;

  switch ([model itemTypeForIndexPath:indexPath]) {
    case ItemTypeGlobalSetting: {
      TableViewItem* selectedItem = [model itemAtIndexPath:indexPath];

      // Do nothing if the tapped option was already the default.
      VivaldiATBSettingItem* selectedSettingItem =
          base::apple::ObjCCastStrict<VivaldiATBSettingItem>(selectedItem);
      if (selectedSettingItem.accessoryType ==
          UITableViewCellAccessoryCheckmark) {
        [tableView deselectRowAtIndexPath:indexPath animated:YES];
        return;
      }

      // Iterate through the options and remove the checkmark from any that
      // have it.
      if ([model
           hasSectionForSectionIdentifier:SectionIdentifierSettings]) {
        for (TableViewItem* item in
             [model
              itemsInSectionWithIdentifier:SectionIdentifierSettings]) {
          VivaldiATBSettingItem* settingItem =
              base::apple::ObjCCastStrict<VivaldiATBSettingItem>(item);
          if (settingItem.accessoryType == UITableViewCellAccessoryCheckmark) {
            settingItem.accessoryType = UITableViewCellAccessoryNone;
            UITableViewCell* cell =
            [tableView cellForRowAtIndexPath:[model indexPathForItem:item]];
            cell.accessoryType = UITableViewCellAccessoryNone;
          }
        }
      }

      VivaldiATBSettingItem* newSelectedCell =
          base::apple::ObjCCastStrict<VivaldiATBSettingItem>
              ([model itemAtIndexPath:indexPath]);
      newSelectedCell.accessoryType = UITableViewCellAccessoryCheckmark;
      UITableViewCell* cell = [tableView cellForRowAtIndexPath:indexPath];
      cell.accessoryType = UITableViewCellAccessoryCheckmark;

      NSInteger type = newSelectedCell.item.type;

      if (!self.adblockManager)
        return;
      switch (type) {
        case ATBSettingNoBlocking:
          self.siteSpecificSetting = ATBSettingNoBlocking;
          break;
        case ATBSettingBlockTrackers:
          self.siteSpecificSetting = ATBSettingBlockTrackers;
          break;
        case ATBSettingBlockTrackersAndAds:
          self.siteSpecificSetting = ATBSettingBlockTrackersAndAds;
          break;
        default: break;
      }
      [self.adblockManager getSettingOptions];
      break;
    }
    default:
      break;
  }
}

#pragma mark: - VivaldiATBConsumer

- (void)didRefreshSettingOptions:(NSArray*)options {
  if (options.count > 0)
    [self reloadTableViewWithItems:options];
}

@end
