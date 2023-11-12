// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ad_tracker_blocker/vivaldi_atb_summery_details_view_controller.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"

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

@implementation VivaldiATBSummeryDetailsViewController

#pragma mark - INITIALIZER
- (instancetype)initWithTitle:(NSString*)title {
  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    self.title = title;
    self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
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
  for (int i = 1; i <= 10; i++)
  {
    TableViewTextItem* dummyItem =
        [[TableViewTextItem alloc] initWithType:ItemTypeSite];
    dummyItem.text = @"https://ads.vg.no/fresk.js";
    dummyItem.useCustomSeparator = YES;

    [model addItem:dummyItem
         toSectionWithIdentifier:SectionIdentifierSites];
  }

  [self.tableView reloadData];
}

@end
