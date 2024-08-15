// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_CHROME_BROWSER_UI_VIVALDI_WHATS_NEW_WHATS_NEW_UTIL_H_
#define IOS_CHROME_BROWSER_UI_VIVALDI_WHATS_NEW_WHATS_NEW_UTIL_H_

#import <Foundation/Foundation.h>

class PromosManager;

// We don't want to use and mess up with the chromium logic to show
// the first run or what's new related promo when browser is updated.
extern NSString* vLastSeenVersionKey;

bool ShouldShowVivaldiWhatsNewPage();

#endif  // IOS_CHROME_BROWSER_UI_VIVALDI_WHATS_NEW_WHATS_NEW_UTIL_H_
