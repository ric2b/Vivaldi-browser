// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

// This file is included to chromium/chrome/browser/flags/android/chrome_feature_list.cc
// at the end of the definition of kFeaturesExposedToJava.

&vivaldi_features::kNewPrivacyReport,

#if defined(OEM_AUTOMOTIVE_BUILD)
&vivaldi_features::kCinemaMode,
#endif  // defined(OEM_AUTOMOTIVE_BUILD)
