// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_item.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_constants.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
#import "ios/ui/settings/vivaldi_settings_constants.h"

using vivaldi::IsVivaldiRunning;
// End Vivaldi

@implementation PinnedItem

#pragma mark - Favicons

- (UIImage*)NTPFavicon {

  if (IsVivaldiRunning())
    return [[UIImage imageNamed:vToolbarMenu]
                imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  // End Vivaldi

  return CustomSymbolWithPointSize(kChromeProductSymbol,
                                   kPinnedCellFaviconSymbolPointSize);
}

@end
