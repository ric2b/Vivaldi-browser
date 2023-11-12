// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_SETTINGS_ROOT_TABLE_VIEW_CONTROLLER_VIVALDI_H_
#define IOS_UI_SETTINGS_SETTINGS_ROOT_TABLE_VIEW_CONTROLLER_VIVALDI_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

@interface SettingsRootTableViewController(Vivaldi)

- (void)showErrorCellWithMessage:(NSString*)message
                         section:(NSInteger)section
                        itemType:(NSInteger)itemType
                       textColor:(UIColor*)color;
- (void)showErrorCellWithMessage:(NSString*)message
                         section:(NSInteger)section
                        itemType:(NSInteger)itemType;
- (void)removeErrorCell:(NSInteger)section
               itemType:(NSInteger)itemType;
- (void)reloadSection:(NSInteger)section;

@end

#endif  // IOS_UI_SETTINGS_SETTINGS_ROOT_TABLE_VIEW_CONTROLLER_VIVALDI_H_
