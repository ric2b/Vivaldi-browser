// Copyright (c) 2015-2016 Vivaldi Technologies. All Rights Reserved.

#ifndef PREFS_VIVALDI_PREF_NAMES_H_
#define PREFS_VIVALDI_PREF_NAMES_H_

#include "chromium/build/build_config.h"

namespace vivaldiprefs {

// Profile prefs go here.
extern const char kAutoUpdateEnabled[];

// Old pref names that have been changed during the migration
// to the new prefs api.
extern const char kOldAlwaysLoadPinnedTabAfterRestore[];
extern const char kOldDeferredTabLoadingAfterRestore[];
#if defined(USE_AURA)
extern const char kOldHideMouseCursorInFullscreen[];
#endif  // USE_AURA
extern const char kOldMousegesturesEnabled[];
extern const char kOldPluginsWidevideEnabled[];
extern const char kOldRockerGesturesEnabled[];
extern const char kOldSmoothScrollingEnabled[];
extern const char kOldVivaldiCaptureDirectory[];
extern const char kOldVivaldiHomepage[];
extern const char kOldVivaldiNumberOfDaysToKeepVisits[];
extern const char kOldVivaldiTabZoom[];
extern const char kOldVivaldiTabsToLinks[];
extern const char kOldVivaldiUseNativeWindowDecoration[];

extern const char kVivaldiExperiments[];
extern const char kVivaldiLastTopSitesVacuumDate[];

// Local state prefs go here.

extern const char kVivaldiUniqueUserId[];

}  // namespace vivaldiprefs

#endif  // PREFS_VIVALDI_PREF_NAMES_H_
