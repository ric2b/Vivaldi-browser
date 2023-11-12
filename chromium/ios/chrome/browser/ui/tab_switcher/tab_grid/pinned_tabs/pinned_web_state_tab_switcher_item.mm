// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_web_state_tab_switcher_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "components/favicon/ios/web_favicon_driver.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_constants.h"
#import "ios/chrome/browser/url/url_util.h"
#import "ios/web/public/web_state.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
#import "ios/chrome/browser/ui/ntp/vivaldi_speed_dial_constants.h"

using vivaldi::IsVivaldiRunning;
// End Vivaldi

@implementation PinnedWebStateTabSwitcherItem

#pragma mark - Favicons

- (UIImage*)regularDefaultFavicon {

  if (IsVivaldiRunning())
    return [[UIImage imageNamed:vNTPSDFallbackFavicon]
              imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  // End Vivaldi

  NSString* symbolName = kGlobeSymbol;
  if (@available(iOS 15, *)) {
    symbolName = kGlobeAmericasSymbol;
  }
  return DefaultSymbolWithPointSize(symbolName,
                                    kPinnedCellFaviconSymbolPointSize);
}

- (UIImage*)incognitoDefaultFavicon {
  NOTREACHED_NORETURN();
}

- (UIImage*)NTPFavicon {

  if (IsVivaldiRunning())
    return [[UIImage imageNamed:vNTPSDFallbackFavicon]
              imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  // End Vivaldi

  return CustomSymbolWithPointSize(kChromeProductSymbol,
                                   kPinnedCellFaviconSymbolPointSize);
}

@end
