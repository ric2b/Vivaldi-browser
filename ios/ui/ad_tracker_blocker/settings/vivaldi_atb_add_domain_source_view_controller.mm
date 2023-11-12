// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ad_tracker_blocker/settings/vivaldi_atb_add_domain_source_view_controller.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/ad_tracker_blocker/cells/vivaldi_atb_setting_item.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_constants.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_mediator.h"
#import "ios/ui/custom_views/vivaldi_text_field_view.h"
#import "ios/ui/helpers/vivaldi_colors_helper.h"
#import "ios/ui/helpers/vivaldi_colors_helper.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/mobile_common/grit/vivaldi_mobile_common_native_strings.h"

using l10n_util::GetNSString;

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Namespace
namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierGlobalSettings
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeGlobalSetting
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
}

@interface VivaldiATBAddDomainSourceViewController()<VivaldiATBConsumer>
// Textview for domain or source url.
@property(nonatomic,weak) VivaldiTextFieldView* textFieldView;
// Button for adding domain or source
@property(nonatomic,weak) UIButton* actionButton;
// An array to hold the ad and tracker blocker setting options.
@property(nonatomic,strong) NSMutableArray* settingOptions;
// The source type, e.g. Ads, Trackers
@property(nonatomic,assign) ATBSourceType source;
// The editing mode for view controller, e.g. adding domain or source.
@property(nonatomic,assign) ATBDomainSourceEditingMode mode;
// The mediator that provides data for this view controller.
@property(nonatomic, strong) VivaldiATBMediator* mediator;
@end

@implementation VivaldiATBAddDomainSourceViewController
@synthesize textFieldView = _textFieldView;
@synthesize actionButton = _actionButton;
@synthesize settingOptions = _settingOptions;
@synthesize source = _source;
@synthesize mode = _mode;
@synthesize mediator = _mediator;

#pragma mark - INITIALIZER
- (instancetype)initWithTitle:(NSString*)title
                       source:(ATBSourceType)source
                  editingMode:(ATBDomainSourceEditingMode)mode {
  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    _source = source;
    _mode = mode;
    self.title = title;
    self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
    [self setUpTableViewHeader];
    [self setUpTableViewFooter];
    [self setActionButtonTitle];
  }
  return self;
}

#pragma mark - VIEW CONTROLLER LIFECYCLE
- (void)viewDidLoad {
  [super viewDidLoad];
  [super loadModel];

  if (self.mode == ATBEditingModeDomain)
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
  [textViewContainer addSubview:textFieldView];
  [textFieldView fillSuperviewWithPadding:textViewPadding];

  switch (self.mode) {

    case ATBEditingModeDomain: {
      [textViewContainer
       fillSuperviewToSafeAreaInsetWithPadding:textViewContainerPaddingDomain];

      NSString* placeHolderStringDomain =
        GetNSString(IDS_AUTOFILL_DOMAIN_HINT);
      [textFieldView setPlaceholder:placeHolderStringDomain];
      break;
    }

    case ATBEditingModeSource: {
      [textViewContainer
       fillSuperviewToSafeAreaInsetWithPadding:textViewContainerPaddingSource];

      NSString* placeHolderStringSource =
        GetNSString(IDS_AUTOFILL_IMPORT_URL_HINT);
      [textFieldView setPlaceholder:placeHolderStringSource];
      [textFieldView setURLMode];
      break;
    }

    default:
      break;
  }

  self.tableView.tableHeaderView = headerView;
}

- (void)setUpTableViewFooter {
  UIView* footerView = [UIView new];
  footerView.frame = CGRectMake(0, 0,
                                self.view.bounds.size.width,
                                tableFooterHeight);
  self.tableView.tableFooterView = footerView;

  UIButton* actionButton = [UIButton new];
  _actionButton = actionButton;
  actionButton.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  actionButton.layer.cornerRadius = actionButtonCornerRadius;
  [actionButton setTitleColor:UIColor.vSystemBlue
                     forState:UIControlStateNormal];
  [actionButton addTarget:self
                   action:@selector(handleActionButtonTap)
                  forControlEvents:UIControlEventTouchUpInside];
  [footerView addSubview:actionButton];
  [actionButton fillSuperviewToSafeAreaInsetWithPadding:actionButtonPadding];
}

- (void)setActionButtonTitle {

  switch (self.mode) {
    case ATBEditingModeDomain: {
      NSString* domainButtonTitleString =
        GetNSString(IDS_ADD_NEW_DOMAIN);
      [self.actionButton setTitle:domainButtonTitleString
                         forState:UIControlStateNormal];
      break;
    }

    case ATBEditingModeSource: {
      NSString* importButtonTitleString =
        GetNSString(IDS_AUTOFILL_IMPORT_URL);
      [self.actionButton setTitle:importButtonTitleString
                         forState:UIControlStateNormal];
      break;
    }

    default:
      break;
  }
}

/// Create and start mediator to compute and  populate the ad and tracker
/// blocker setting options.
- (void)loadATBOptions {
  self.mediator = [[VivaldiATBMediator alloc] init];
  self.mediator.consumer = self;
  [self.mediator startMediating];
}

-(void)reloadGlobalSettingModelWithOption:(NSArray*)options {

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

#pragma mark ACTIONS
- (void)handleActionButtonTap {
  // TODO: - @prio@vivaldi.com - Implement this when backend is available.
}

#pragma mark - AD AND TRACKER BLOCKER CONSUMER

- (void)refreshSettingOptions:(NSArray*)items {
  if (items.count > 0)
    [self reloadGlobalSettingModelWithOption:items];
}


@end
