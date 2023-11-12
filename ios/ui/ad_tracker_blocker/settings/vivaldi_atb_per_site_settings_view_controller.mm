// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ad_tracker_blocker/settings/vivaldi_atb_per_site_settings_view_controller.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/ad_tracker_blocker/settings/vivaldi_atb_add_domain_source_view_controller.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_constants.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_domain_source_mode.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_source_type.h"
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
  SectionIdentifierSites = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSite = kItemTypeEnumZero,
};

}

@implementation VivaldiATBPerSiteSettingsViewController

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

  UIButton* addDomainButton = [UIButton new];
  addDomainButton.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  addDomainButton.layer.cornerRadius = actionButtonCornerRadius;

  NSString* buttonTitleString = GetNSString(IDS_ADD_NEW_DOMAIN);
  [addDomainButton setTitle:buttonTitleString
                   forState:UIControlStateNormal];
  [addDomainButton
     setTitleColor:UIColor.vSystemBlue
     forState:UIControlStateNormal];
  [addDomainButton addTarget:self
                            action:@selector(handleAddDomainButtonTap)
                  forControlEvents:UIControlEventTouchUpInside];
  [footerView addSubview:addDomainButton];
  [addDomainButton fillSuperviewToSafeAreaInsetWithPadding:actionButtonPadding];
}

#pragma mark ACTIONS
- (void)handleAddDomainButtonTap {
  NSString* titleString = GetNSString(IDS_ADD_NEW_DOMAIN);

  VivaldiATBAddDomainSourceViewController* controller =
    [[VivaldiATBAddDomainSourceViewController alloc]
      initWithTitle:titleString
             source:ATBSourceNone
        editingMode:ATBEditingModeDomain];
  [self.navigationController pushViewController:controller animated:YES];
}

-(void)reloadModel {
  TableViewModel* model = self.tableViewModel;

  // Delete any existing section.
  if ([model
          hasSectionForSectionIdentifier:SectionIdentifierSites])
    [model
        removeSectionWithIdentifier:SectionIdentifierSites];

  // Creates section for sites list
  [model
      addSectionWithIdentifier:SectionIdentifierSites];

  // TODO: @prio@vivaldi.com - REPLACE WITH ACTUAL DATA WHEN BACKEND AVAILABLE.
  // This is a dummy data generated for UI development.
  for (int i = 1; i <= 5; i++)
  {
    TableViewImageItem* dummyItem =
        [[TableViewImageItem alloc] initWithType:ItemTypeSite];
    dummyItem.title = @"sale.aliexpress.com";
    dummyItem.detailText = @"Block Trackers";
    dummyItem.image = [UIImage imageNamed:vATBShield];
    dummyItem.useCustomSeparator = YES;

    [model addItem:dummyItem
         toSectionWithIdentifier:SectionIdentifierSites];
  }

  [self.tableView reloadData];
}

@end
