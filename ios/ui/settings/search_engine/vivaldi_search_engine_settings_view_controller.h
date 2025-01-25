// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_SEARCH_ENGINE_VIVALDI_SEARCH_ENGINE_SETTINGS_VIEW_CONTROLLER_H_
#define IOS_UI_SETTINGS_SEARCH_ENGINE_VIVALDI_SEARCH_ENGINE_SETTINGS_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"
#import "ios/ui/settings/search_engine/vivaldi_search_engine_settings_consumer.h"

class ProfileIOS;

@protocol VivaldiSearchEngineSettingsViewControllerDelegate
- (void)searchSuggestionsEnabled:(BOOL)enabled;
- (void)searchEngineNicknameEnabled:(BOOL)enabled;
@end

// This class is the table view for the Search Engine settings.
@interface VivaldiSearchEngineSettingsViewController
    : SettingsRootTableViewController <SettingsControllerProtocol,
                                       VivaldiSearchEngineSettingsConsumer>

// The designated initializer. `profile` must not be nil.
- (instancetype)initWithProfile:(ProfileIOS*)profile
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@property(nonatomic, weak)
    id<VivaldiSearchEngineSettingsViewControllerDelegate> delegate;

@end

#endif  // IOS_UI_SETTINGS_SEARCH_ENGINE_VIVALDI_SEARCH_ENGINE_SETTINGS_VIEW_CONTROLLER_H_
