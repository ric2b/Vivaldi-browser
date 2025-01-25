// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_SETTINGS_TABS_VIVALDI_TAB_SETTINGS_HELPER_H_
#define IOS_UI_SETTINGS_TABS_VIVALDI_TAB_SETTINGS_HELPER_H_

#import <Foundation/Foundation.h>

#import "ios/ui/settings/tabs/vivaldi_ntp_type.h"
#import "ios/ui/settings/tabs/vivaldi_tab_setting_prefs.h"


class PrefService;

@interface VivaldiTabSettingsHelper : NSObject

// It wil return homepage URL set by user or startpage
+ (NSString*)getHomePageURLWithPref:(PrefService*)prefService;

// It wil return newtab URL set by user or internal pages
+ (NSString*)getNewTabURLWithPref:(PrefService*)prefService;

@end

#endif  // IOS_UI_SETTINGS_TABS_VIVALDI_TAB_SETTINGS_HELPER_H_
