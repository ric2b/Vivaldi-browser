// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/vivaldi_overflow_menu/vivaldi_overflow_menu_util.h"

NSString* vResetOverflowMenuDestinations = @"vResetOverflowMenuDestinations";
NSString* vResetOverflowMenuActions = @"vResetOverflowMenuActions";

bool ShouldResetOverflowMenuDestinations() {
  return ![[NSUserDefaults standardUserDefaults]
              boolForKey:vResetOverflowMenuDestinations];
}

bool ShouldResetOverflowMenuActions() {
  return ![[NSUserDefaults standardUserDefaults]
              boolForKey:vResetOverflowMenuActions];
}

void SetOverflowMenuDestinationsResetComplete() {
  [[NSUserDefaults standardUserDefaults]
      setBool:YES forKey:vResetOverflowMenuDestinations];
  [[NSUserDefaults standardUserDefaults] synchronize];
}

void SetOverflowMenuActionsResetComplete() {
  [[NSUserDefaults standardUserDefaults]
      setBool:YES forKey:vResetOverflowMenuActions];
  [[NSUserDefaults standardUserDefaults] synchronize];
}
