// Copyright (c) 2015-2016 Vivaldi Technologies. All Rights Reserved.

#ifndef VIVALDI_PREF_NAMES_H_
#define VIVALDI_PREF_NAMES_H_

#include "chromium/build/build_config.h"

namespace vivaldiprefs {

extern const char kDeferredTabLoadingAfterRestore[];
extern const char kMousegesturesEnabled[];
extern const char kRockerGesturesEnabled[];
extern const char kSmoothScrollingEnabled[];
extern const char kVivaldiTabsToLinks[];
extern const char kVivaldiLastTopSitesVacuumDate[];
extern const char kVivaldiTabZoom[];
extern const char kVivaldiHomepage[];
extern const char kVivaldiNumberOfDaysToKeepVisits[];
extern const char kVivaldiExperiments[];

#if defined(OS_MACOSX)
extern const char kAppleKeyboardUIMode[];
extern const char kAppleMiniaturizeOnDoubleClick[];
extern const char kAppleAquaColorVariant[];
extern const char kAppleInterfaceStyle[];
extern const char kTableViewDefaultSizeMode[];
extern const char kAppleActionOnDoubleClick[];
extern const char kSwipeScrollDirection[];
#endif

}  // namespace vivaldiprefs

#endif  // VIVALDI_PREF_NAMES_H_
