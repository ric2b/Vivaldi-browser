// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ntp/vivaldi_ntp_util.h"

NSString* vNTPCustomizeStartPageButtonShownKey =
    @"vNTPCustomizeStartPageButtonShownKey";

bool ShouldShowCustomizeStartPageButton() {
  return ![[NSUserDefaults standardUserDefaults]
              boolForKey:vNTPCustomizeStartPageButtonShownKey];
}

void SetCustomizeStartPageButtonShown() {
  [[NSUserDefaults standardUserDefaults]
      setBool:YES forKey:vNTPCustomizeStartPageButtonShownKey];
  [[NSUserDefaults standardUserDefaults] synchronize];
}
