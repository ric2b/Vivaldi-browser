// Copyright (c) 2015-2016 Vivaldi Technologies. All Rights Reserved.

#ifndef PREFS_VIVALDI_PREF_NAMES_H_
#define PREFS_VIVALDI_PREF_NAMES_H_

#include "chromium/build/build_config.h"

namespace vivaldiprefs {

// Profile prefs go here.

extern const char kAlwaysLoadPinnedTabAfterRestore[];
extern const char kAutoUpdateEnabled[];
extern const char kDeferredTabLoadingAfterRestore[];
extern const char kMousegesturesEnabled[];
extern const char kPluginsWidevideEnabled[];
extern const char kRockerGesturesEnabled[];
extern const char kSmoothScrollingEnabled[];
extern const char kVivaldiCaptureDirectory[];
extern const char kVivaldiHomepage[];
extern const char kVivaldiLastTopSitesVacuumDate[];
extern const char kVivaldiNumberOfDaysToKeepVisits[];
extern const char kVivaldiTabZoom[];
extern const char kVivaldiTabsToLinks[];
extern const char kVivaldiHasDesktopWallpaperProtocol[];

extern const char kVivaldiExperiments[];

#if defined(USE_AURA)
extern const char kHideMouseCursorInFullscreen[];
#endif  // USE_AURA

#if defined(OS_MACOSX)
extern const char kAppleKeyboardUIMode[];
extern const char kAppleMiniaturizeOnDoubleClick[];
extern const char kAppleAquaColorVariant[];
extern const char kAppleInterfaceStyle[];
extern const char kTableViewDefaultSizeMode[];
extern const char kAppleActionOnDoubleClick[];
extern const char kSwipeScrollDirection[];
#endif

// Local state prefs go here.

extern const char kVivaldiUniqueUserId[];

}  // namespace vivaldiprefs

#endif  // PREFS_VIVALDI_PREF_NAMES_H_
