// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_PANEL_PANEL_PREFS_H_
#define IOS_PANEL_PANEL_PREFS_H_

#import <UIKit/UIKit.h>

class PrefRegistrySimple;

// Class to store and manage the prefs for the panels.
@interface PanelPrefs: NSObject

/// Registers the local preferences.
+ (void)registerLocalStatePrefs:(PrefRegistrySimple*)registry;

@end

#endif  // IOS_PANEL_PANEL_PREFS_H_
