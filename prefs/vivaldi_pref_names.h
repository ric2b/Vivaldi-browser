// Copyright (c) 2015-2022 Vivaldi Technologies. All Rights Reserved.

#ifndef PREFS_VIVALDI_PREF_NAMES_H_
#define PREFS_VIVALDI_PREF_NAMES_H_

#include "chromium/build/build_config.h"

namespace vivaldiprefs {

// Profile prefs go here.
extern const char kAutoUpdateEnabled[];
extern const char kVivaldiAccountPendingRegistration[];
extern const char kVivaldiAddressBarSearchDirectMatchEnabled[];
extern const char kVivaldiExperiments[];
extern const char kVivaldiLastTopSitesVacuumDate[];
extern const char kVivaldiPIPPlacement[];

// Local state prefs go here.
extern const char kVivaldiAutoUpdateStandalone[];
extern const char kVivaldiUniqueUserId[];
extern const char kVivaldiStatsNextDailyPing[];
extern const char kVivaldiStatsNextWeeklyPing[];
extern const char kVivaldiStatsNextMonthlyPing[];
extern const char kVivaldiStatsNextTrimestrialPing[];
extern const char kVivaldiStatsNextSemestrialPing[];
extern const char kVivaldiStatsNextYearlyPing[];
extern const char kVivaldiStatsExtraPing[];
extern const char kVivaldiStatsExtraPingTime[];
extern const char kVivaldiStatsPingsSinceLastMonth[];
extern const char kVivaldiProfileImagePath[];
extern const char kVivaldiTranslateLanguageList[];
extern const char kVivaldiTranslateLanguageListLastUpdate[];
extern const char kVivaldiAccountServerUrlIdentity[];
extern const char kVivaldiAccountServerUrlOpenId[];
extern const char kVivaldiSyncServerUrl[];
extern const char kVivaldiSyncNotificationsServerUrl[];

extern const char kVivaldiClientHintsBrand[];
extern const char kVivaldiClientHintsBrandAppendVivaldi[];
extern const char kVivaldiClientHintsBrandCustomBrand[];
extern const char kVivaldiClientHintsBrandCustomBrandVersion[];


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

#if BUILDFLAG(IS_IOS)
extern const char kVivaldiNoteFolderDefault[];
extern const char kVivaldiNoteCachedTopMostRow[];
extern const char kVivaldiNoteCachedFolderId[];

// Speed dial sorting mode
extern const char kVivaldiSpeedDialSortingMode[];
// Start page layout
extern const char kVivaldiStartPageLayoutStyle[];
// Setting for folder visiblity on bookmark folder page
extern const char kVivaldiBookmarkFoldersViewMode[];

// Tabs
// Desktop style tabs enabled status
extern const char kVivaldiDesktopTabsEnabled[];
// Tab stack use status
extern const char kVivaldiTabStackEnabled[];
#endif

#if BUILDFLAG(IS_ANDROID)
// Background media playback for YouTube.
extern const char kBackgroundMediaPlaybackAllowed[];
#endif
}  // namespace vivaldiprefs

#endif  // PREFS_VIVALDI_PREF_NAMES_H_
