// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/dynamic_type_util.h"

#import "base/metrics/histogram_macros.h"
#import "ios/chrome/common/ui/util/dynamic_type_util.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
// End Vivaldi

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

UIFont* LocationBarSteadyViewFont(UIContentSizeCategory currentCategory) {

  if (vivaldi::IsVivaldiRunning())
    return PreferredFontForTextStyleWithMaxCategory(
        UIFontTextStyleSubheadline, currentCategory,
        UIContentSizeCategoryAccessibilityExtraLarge);
  // End Vivaldi

  return PreferredFontForTextStyleWithMaxCategory(
      UIFontTextStyleBody, currentCategory,
      UIContentSizeCategoryAccessibilityExtraLarge);
}
