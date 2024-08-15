// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NTP_VIVALDI_NTP_UTIL_H_
#define IOS_UI_NTP_VIVALDI_NTP_UTIL_H_

#import <Foundation/Foundation.h>

extern NSString* vNTPCustomizeStartPageButtonShownKey;

bool ShouldShowCustomizeStartPageButton();
void SetCustomizeStartPageButtonShown();

#endif  // IOS_UI_NTP_VIVALDI_NTP_UTIL_H_
