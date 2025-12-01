// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_START_PAGE_VIVALDI_START_PAGE_SETTINGS_VIEW_CONTROLLER_H_
#define IOS_UI_SETTINGS_START_PAGE_VIVALDI_START_PAGE_SETTINGS_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/settings/ui_bundled/settings_controller_protocol.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller.h"
#import "ios/ui/settings/start_page/vivaldi_start_page_settings_consumer.h"

class Browser;
@class VivaldiStartPageSettingsViewController;

// Delegate for presentation events related to
// VivaldiStartPageSettingsViewController.
@protocol VivaldiStartPageSettingsViewControllerPresentationDelegate

// Called when the view controller is removed from its parent.
- (void)startPageSettingsViewControllerWasRemoved:
    (VivaldiStartPageSettingsViewController*)controller;

@end


// This class is the table view for the Start Page settings.
@interface VivaldiStartPageSettingsViewController
    : SettingsRootTableViewController <SettingsControllerProtocol,
                                      VivaldiStartPageSettingsConsumer>

// The consumer of the start page settings mediator.
@property(nonatomic, weak) id<VivaldiStartPageSettingsConsumer> consumer;

@property(nonatomic, weak)
    id<VivaldiStartPageSettingsViewControllerPresentationDelegate>
        presentationDelegate;

// Initializes a new VivaldiStartPageSettingsViewController. `browser` must not
// be nil.
- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_UI_SETTINGS_START_PAGE_VIVALDI_START_PAGE_SETTINGS_VIEW_CONTROLLER_H_
