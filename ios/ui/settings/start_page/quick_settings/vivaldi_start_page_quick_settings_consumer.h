// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_START_PAGE_QUICK_SETTINGS_VIVALDI_START_PAGE_QUICK_SETTINGS_CONSUMER_H_
#define IOS_UI_SETTINGS_START_PAGE_QUICK_SETTINGS_VIVALDI_START_PAGE_QUICK_SETTINGS_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "ios/ui/settings/start_page/layout_settings/vivaldi_start_page_layout_column.h"
#import "ios/ui/settings/start_page/layout_settings/vivaldi_start_page_layout_style.h"

// A protocol implemented by consumers to handle start page settings
// state change.
@protocol VivaldiStartPageQuickSettingsConsumer

// Updates the state with the show speed dials preference value.
- (void)setPreferenceShowSpeedDials:(BOOL)showSpeedDials;
// Updates the state with the show start page customize preference value.
- (void)setPreferenceShowCustomizeStartPageButton:(BOOL)showCustomizeButton;
// Updates the state with the speed dial layout style preference value
- (void)setPreferenceSpeedDialLayout:(VivaldiStartPageLayoutStyle)layout;
// Updates the state with the speed dial maximum number columns preference value
- (void)setPreferenceSpeedDialColumn:(VivaldiStartPageLayoutColumn)column;

@end

#endif  // IOS_UI_SETTINGS_START_PAGE_QUICK_SETTINGS_VIVALDI_START_PAGE_QUICK_SETTINGS_CONSUMER_H_
