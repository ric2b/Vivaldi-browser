// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_suggestion_icon_util.h"

#import "base/notreached.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
#import "ios/ui/ntp/vivaldi_speed_dial_constants.h"
#import "ios/ui/context_menu/vivaldi_context_menu_constants.h"
#import "ios/ui/vivaldi_overflow_menu/vivaldi_oveflow_menu_constants.h"

using vivaldi::IsVivaldiRunning;
// End Vivaldi

namespace {
const CGFloat kSymbolSize = 18;
}  // namespace

UIImage* GetOmniboxSuggestionIcon(OmniboxSuggestionIconType icon_type) {
  NSString* symbol_name = kGlobeSymbol;
  bool default_symbol = true;
  switch (icon_type) {
    case OmniboxSuggestionIconType::kCalculator:
      symbol_name = kEqualSymbol;
      break;
    case OmniboxSuggestionIconType::kDefaultFavicon:
      symbol_name = kGlobeAmericasSymbol;
      break;
    case OmniboxSuggestionIconType::kSearch:
      symbol_name = kSearchSymbol;
      break;
    case OmniboxSuggestionIconType::kSearchHistory:
      symbol_name = kHistorySymbol;
      break;
    case OmniboxSuggestionIconType::kConversion:
      symbol_name = kSyncEnabledSymbol;
      break;
    case OmniboxSuggestionIconType::kDictionary:
      symbol_name = kBookClosedSymbol;
      break;
    case OmniboxSuggestionIconType::kStock:
      symbol_name = kSortSymbol;
      break;
    case OmniboxSuggestionIconType::kSunrise:
      symbol_name = kSunFillSymbol;
      break;
    case OmniboxSuggestionIconType::kWhenIs:
      symbol_name = kCalendarSymbol;
      break;
    case OmniboxSuggestionIconType::kTranslation:

      if (IsVivaldiRunning()) {
        symbol_name = vOverflowTranslate;
      } else {
      symbol_name = kTranslateSymbol;
      } // End Vivaldi

      default_symbol = false;
      break;
    case OmniboxSuggestionIconType::kFallbackAnswer:
      symbol_name = kSearchSymbol;
      break;
    case OmniboxSuggestionIconType::kSearchTrend:
      symbol_name = kUpTrendSymbol;
      default_symbol = false;
      break;
    case OmniboxSuggestionIconType::kCount:
      NOTREACHED();
  }

  // Vivaldi
  switch (icon_type) {
    case OmniboxSuggestionIconType::kDefaultFavicon:
      return [[UIImage imageNamed:vNTPSDFallbackFavicon]
                imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    case OmniboxSuggestionIconType::kCount:
      return [[UIImage imageNamed:vNTPSDFallbackFavicon]
                imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    case OmniboxSuggestionIconType::kSearch:
    case OmniboxSuggestionIconType::kFallbackAnswer:
      return [[UIImage imageNamed:vSearch]
                imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    default:
      break;
  }
  // End Vivaldi

  if (default_symbol) {
    return DefaultSymbolWithPointSize(symbol_name, kSymbolSize);
  }
  return CustomSymbolWithPointSize(symbol_name, kSymbolSize);
}

#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
UIImage* GetBrandedGoogleIconForOmnibox() {
  return MakeSymbolMonochrome(
      CustomSymbolWithPointSize(kGoogleIconSymbol, kSymbolSize));
}
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
