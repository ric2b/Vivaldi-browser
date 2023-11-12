// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_SEARCH_ENGINE_VIVALDI_SEARCH_ENGINE_SETTINGS_VIEW_CONTROLLER_H_
#define IOS_UI_SETTINGS_SEARCH_ENGINE_VIVALDI_SEARCH_ENGINE_SETTINGS_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

class ChromeBrowserState;

// This class is the table view for the Search Engine settings.
@interface VivaldiSearchEngineSettingsViewController
    : SettingsRootTableViewController <SettingsControllerProtocol>

// The designated initializer. `browserState` must not be nil.
- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_UI_SETTINGS_SEARCH_ENGINE_VIVALDI_SEARCH_ENGINE_SETTINGS_VIEW_CONTROLLER_H_
