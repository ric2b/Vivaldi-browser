// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/sync/settings_root_table_view_controller+vivaldi.h"

#import "ios/ui/table_view/cells/vivaldi_input_error_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SettingsRootTableViewController(Vivaldi)


- (void)showErrorCellWithMessage:(NSString*)message
                         section:(NSInteger)section
                        itemType:(NSInteger)itemType {
  [self showErrorCellWithMessage:message
                         section:section
                        itemType:itemType
                       textColor:nil];
}

- (void)showErrorCellWithMessage:(NSString*)message
                         section:(NSInteger)section
                        itemType:(NSInteger)itemType
                       textColor:(UIColor*)color {
  if ([self hasErrorCell:section itemType:itemType]) {
    [self removeErrorCell:section itemType:itemType];
  }
  // Insert cell at top of section
  [self.tableViewModel insertItem:
      [self errorCellItemWithMessage:message
                            itemType:itemType
                           textColor:color]
          inSectionWithIdentifier:section
                          atIndex:0];
  [self reloadSection:section];
}

- (void)removeErrorCell:(NSInteger)section
               itemType:(NSInteger)itemType {
  if ([self hasErrorCell:section itemType:itemType]) {
    NSIndexPath* path =
        [self.tableViewModel indexPathForItemType:itemType
                  sectionIdentifier:section];
    [self.tableViewModel removeItemWithType:itemType
        fromSectionWithIdentifier:section];
    [self.tableView deleteRowsAtIndexPaths:@[ path ]
                     withRowAnimation:UITableViewRowAnimationAutomatic];
    [self reloadSection:section];
  }
}

- (BOOL)hasErrorCell:(NSInteger)section
            itemType:(NSInteger)itemType {
  return [self.tableViewModel hasItemForItemType:itemType
      sectionIdentifier:section];
}

- (void)reloadSection:(NSInteger)section {
    NSUInteger index =
        [self.tableViewModel sectionForSectionIdentifier:section];
    [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:index]
                withRowAnimation:UITableViewRowAnimationAutomatic];
}

- (TableViewItem*)errorCellItemWithMessage:(NSString*)errorMessage
                                  itemType:(NSInteger)itemType
                                 textColor:(UIColor*)color {
  VivaldiInputErrorItem* item =
      [[VivaldiInputErrorItem alloc] initWithType:itemType];
  item.text = errorMessage;
  if (color) {
    item.textColor = color;
  }
  return item;
}

@end