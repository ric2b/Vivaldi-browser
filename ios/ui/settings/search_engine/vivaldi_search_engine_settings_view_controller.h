// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_SEARCH_ENGINE_VIVALDI_SEARCH_ENGINE_SETTINGS_VIEW_CONTROLLER_H_
#define IOS_UI_SETTINGS_SEARCH_ENGINE_VIVALDI_SEARCH_ENGINE_SETTINGS_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/settings/ui_bundled/settings_controller_protocol.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller.h"
#import "ios/ui/settings/search_engine/vivaldi_search_engine_settings_consumer.h"

class ProfileIOS;

@class VivaldiSearchEngineSettingsViewController;

@protocol VivaldiSearchEngineSettingsViewControllerDelegate
- (void)searchEngineNicknameEnabled:(BOOL)enabled;
@end

// Delegate for presentation events related to
// VivaldiSearchEngineSettingsViewController.
@protocol VivaldiSearchEngineSettingsViewControllerPresentationDelegate

// Called when the view controller is removed from its parent.
- (void)searchEngineSettingsViewControllerWasRemoved:
      (VivaldiSearchEngineSettingsViewController*)controller;

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

@property(nonatomic, weak)
    id<VivaldiSearchEngineSettingsViewControllerPresentationDelegate>
        presentationDelegate;

@end

#endif  // IOS_UI_SETTINGS_SEARCH_ENGINE_VIVALDI_SEARCH_ENGINE_SETTINGS_VIEW_CONTROLLER_H_
