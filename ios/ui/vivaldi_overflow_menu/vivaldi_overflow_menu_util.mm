// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/vivaldi_overflow_menu/vivaldi_overflow_menu_util.h"

NSString* vResetOverflowMenuActions = @"vResetOverflowMenuActions";

bool ShouldResetOverflowMenuActions() {
  return ![[NSUserDefaults standardUserDefaults]
              boolForKey:vResetOverflowMenuActions];
}

void SetOverflowMenuActionsResetComplete() {
  [[NSUserDefaults standardUserDefaults]
      setBool:YES forKey:vResetOverflowMenuActions];
  [[NSUserDefaults standardUserDefaults] synchronize];
}
