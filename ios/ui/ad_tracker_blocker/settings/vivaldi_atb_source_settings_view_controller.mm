// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ad_tracker_blocker/settings/vivaldi_atb_source_settings_view_controller.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/ad_tracker_blocker/settings/vivaldi_atb_add_domain_source_view_controller.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_constants.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_domain_source_mode.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_source_type.h"
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
  SectionIdentifierSources = kSectionIdentifierEnumZero
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSource = kItemTypeEnumZero
};

}


@implementation VivaldiATBSourceSettingsViewController

#pragma mark - INITIALIZER
- (instancetype)initWithTitle:(NSString*)title {
  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    self.title = title;
    self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
    [self setUpTableViewFooter];
  }
  return self;
}

#pragma mark - VIEW CONTROLLER LIFECYCLE
- (void)viewDidLoad {
  [super viewDidLoad];
  [super loadModel];
  [self reloadModel];
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
      initWithTitle:titleString
             source:ATBSourceNone
        editingMode:ATBEditingModeSource];
  [self.navigationController pushViewController:controller animated:YES];
}


-(void)reloadModel {
  TableViewModel* model = self.tableViewModel;

  // Delete any existing section.
  if ([model
          hasSectionForSectionIdentifier:SectionIdentifierSources])
    [model
        removeSectionWithIdentifier:SectionIdentifierSources];

  // Creates section for sources list
  [model
      addSectionWithIdentifier:SectionIdentifierSources];

  // TODO: @prio@vivaldi.com - REPLACE WITH ACTUAL DATA WHEN BACKEND AVAILABLE.
  // This is a dummy data generated for UI development.
  for (int i = 1; i <= 5; i++)
  {
    TableViewSwitchItem* dummyItem =
        [[TableViewSwitchItem alloc] initWithType:ItemTypeSource];
    dummyItem.text = @"DuckDuckGo Tracker Rader";
    dummyItem.detailText = @"Updated: 21 Nov 2022 14:10";
    dummyItem.useCustomSeparator = YES;

    [model addItem:dummyItem
         toSectionWithIdentifier:SectionIdentifierSources];
  }

  [self.tableView reloadData];
}

@end
