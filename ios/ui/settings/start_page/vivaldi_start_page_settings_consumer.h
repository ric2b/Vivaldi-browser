// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_START_PAGE_VIVALDI_START_PAGE_SETTINGS_CONSUMER_H_
#define IOS_UI_SETTINGS_START_PAGE_VIVALDI_START_PAGE_SETTINGS_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "ios/ui/settings/start_page/layout_settings/vivaldi_start_page_layout_style.h"

// A protocol implemented by consumers to handle start page settings state.
@protocol VivaldiStartPageSettingsConsumer

// Updates the state with the layout settings item with pref value.
- (void)setPreferenceForStartPageLayout:(VivaldiStartPageLayoutStyle)layoutStyle;

@end

#endif  // IOS_UI_SETTINGS_START_PAGE_VIVALDI_START_PAGE_SETTINGS_CONSUMER_H_
