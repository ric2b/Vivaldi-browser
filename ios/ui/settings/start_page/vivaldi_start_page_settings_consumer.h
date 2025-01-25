// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_START_PAGE_VIVALDI_START_PAGE_SETTINGS_CONSUMER_H_
#define IOS_UI_SETTINGS_START_PAGE_VIVALDI_START_PAGE_SETTINGS_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "ios/ui/settings/start_page/layout_settings/vivaldi_start_page_layout_column.h"
#import "ios/ui/settings/start_page/layout_settings/vivaldi_start_page_layout_style.h"
#import "ios/ui/settings/start_page/vivaldi_start_page_start_item_type.h"

// A protocol implemented by consumers to handle start page settings
// state change.
@protocol VivaldiStartPageSettingsConsumer

// Updates the state with the show frequently visited pages preference value.
- (void)setPreferenceShowFrequentlyVisitedPages:(BOOL)showFrequentlyVisited;
// Updates the state with the show speed dials preference value.
- (void)setPreferenceShowSpeedDials:(BOOL)showSpeedDials;
// Updates the state with the show start page customize preference value.
- (void)setPreferenceShowCustomizeStartPageButton:(BOOL)showCustomizeButton;
// Updates the state with the speed dial layout style preference value
- (void)setPreferenceSpeedDialLayout:(VivaldiStartPageLayoutStyle)layout;
// Updates the state with the speed dial maximum number columns preference value
- (void)setPreferenceSpeedDialColumn:(VivaldiStartPageLayoutColumn)column;
// Updates the state with the start page reopen with item type
- (void)setPreferenceStartPageReopenWithItem:(VivaldiStartPageStartItemType)item;

@end

#endif  // IOS_UI_SETTINGS_START_PAGE_VIVALDI_START_PAGE_SETTINGS_CONSUMER_H_
