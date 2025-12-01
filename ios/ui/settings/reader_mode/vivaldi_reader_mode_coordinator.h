// Copyright 2025 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_READER_MODE_VIVALDI_READER_MODE_COORDINATOR_H_
#define IOS_UI_SETTINGS_READER_MODE_VIVALDI_READER_MODE_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

class Browser;

// This class is the coordinator for the reader mode setting.
@interface VivaldiReaderModeCoordinator : ChromeCoordinator

// Optional popover anchor when presented on iPad.
// If set, the coordinator will anchor the popover to this view/rect instead of
// trying to infer an anchor from the navigation items.
@property(nonatomic, weak) UIView* popoverSourceView;
@property(nonatomic, assign) CGRect popoverSourceRect;

// Called when the coordinator stops (e.g., UI dismissed). Useful for cleanup
// by the creator. Executed on main thread.
@property(nonatomic, copy) void (^didStopHandler)(void);

@end

#endif  // IOS_UI_SETTINGS_READER_MODE_VIVALDI_READER_MODE_COORDINATOR_H_
