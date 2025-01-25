// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/notreached.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_theme_resources.h"
#import "ios/public/provider/chrome/browser/branded_images/branded_images_api.h"
#import "ui/base/resource/resource_bundle.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
#import "ios/ui/promos/vivaldi_promo_constants.h"

using vivaldi::IsVivaldiRunning;
// End Vivaldi

namespace ios {
namespace provider {

UIImage* GetBrandedImage(BrandedImage branded_image) {
  switch (branded_image) {
    case BrandedImage::kDownloadGoogleDrive:

      // Vivaldi: We wont promote google drive install.
      if (IsVivaldiRunning())
        return nil; // End Vivaldi

      return [UIImage imageNamed:@"download_drivium"];

    case BrandedImage::kOmniboxAnswer:
      return nil;

    case BrandedImage::kStaySafePromo:

      if (IsVivaldiRunning())
        return [UIImage imageNamed:vStaySafe]; // End Vivaldi

      return [UIImage imageNamed:@"chromium_stay_safe"];

    case BrandedImage::kMadeForIOSPromo:

      if (IsVivaldiRunning())
        return [UIImage imageNamed:vMadeForiOS]; // End Vivaldi

      return [UIImage imageNamed:@"chromium_ios_made"];

    case BrandedImage::kMadeForIPadOSPromo:

      if (IsVivaldiRunning())
        return [UIImage imageNamed:vMadeForiPad]; // End Vivaldi

      return [UIImage imageNamed:@"chromium_ipados_made"];

    case BrandedImage::kNonModalDefaultBrowserPromo:

      if (IsVivaldiRunning())
        return [UIImage imageNamed:vDefaultNonModal]; // End Vivaldi

      return [UIImage imageNamed:@"chromium_non_default_promo"];

    case BrandedImage::kPasswordSuggestionKey:

      if (IsVivaldiRunning())
        return [UIImage imageNamed:vPasswordsSuggestion]; // End Vivaldi

      return [UIImage imageNamed:@"password_suggestion_key"];
  }

  NOTREACHED();
}

}  // namespace provider
}  // namespace ios
