// Copyright (c) 2015-2022 Vivaldi Technologies. All Rights Reserved.

#ifndef PREFS_VIVALDI_PREF_NAMES_H_
#define PREFS_VIVALDI_PREF_NAMES_H_

#include "chromium/build/build_config.h"

namespace vivaldiprefs {

// Profile prefs go here.
extern const char kAutoUpdateEnabled[];
extern const char kVivaldiAccountPendingRegistration[];
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
extern const char kVivaldiSyncServerUrl[];
extern const char kVivaldiSyncNotificationsServerUrl[];

extern const char kVivaldiClientHintsBrand[];
extern const char kVivaldiClientHintsBrandAppendVivaldi[];
extern const char kVivaldiClientHintsBrandCustomBrand[];
extern const char kVivaldiClientHintsBrandCustomBrandVersion[];

extern const char kVivaldiCrashReportingConsentGranted[];

extern const char kVivaldiPreferredColorScheme[];
#if BUILDFLAG(IS_IOS)
// Dict preference which tracks the current elements and order of the overflow
// menu's vivaldi actions.
extern const char kOverflowMenuVivaldiActionsOrder[];

// Notes
extern const char kVivaldiNoteFolderDefault[];
extern const char kVivaldiNoteCachedTopMostRow[];
extern const char kVivaldiNoteCachedFolderId[];
extern const char kVivaldiNotesSortingMode[];
extern const char kVivaldiNotesSortingOrder[];

// Setting for folder visiblity on bookmark folder page
extern const char kVivaldiBookmarkFoldersViewMode[];

extern const char kVivaldiBookmarksSortingMode[];
extern const char kVivaldiBookmarksSortingOrder[];

// Search Engine
extern const char kVivaldiEnableSearchEngineNickname[];

// Address bar
extern const char kVivaldiShowFullAddressEnabled[];

// Tabs
// Desktop style tabs enabled status
extern const char kVivaldiDesktopTabsEnabled[];
// Reverse search suggestion results order state for bottom address bar
extern const char kVivaldiReverseSearchResultsEnabled[];
// Tab stack use status
extern const char kVivaldiTabStackEnabled[];
// Show X button in background tabs
extern const char kVivaldiShowXButtonBackgroundTabsEnabled[];
// Open new tab when last open tab is closed
extern const char kVivaldiOpenNTPOnClosingLastTabEnabled[];
// Focus omnibox on new tab page
extern const char kVivaldiFocusOmniboxOnNTPEnabled[];
// New Tab URL
extern const char kVivaldiNewTabURL[];
// New Tab Setting
extern const char kVivaldiNewTabSetting[];

// General Settings
// Homepage url
extern const char kVivaldiHomepageURL[];
// Show Homepage Button
extern const char kVivaldiHomepageEnabled[];

// Apearance
// Selected browser theme i.e. Light, Dark, System
extern const char kVivaldiAppearanceMode[];
// Selected website appearance style i.e. Light, Dark, Auto
extern const char kVivaldiWebsiteAppearanceStyle[];
// Force dark theme on website
extern const char kVivaldiWebsiteAppearanceForceDarkTheme[];
// Custom accent color selected either from preloaded colors or manual entry
extern const char kVivaldiCustomAccentColor[];
// Dynamic accent color from webpage
extern const char kVivaldiDynamicAccentColorEnabled[];

// Start page Custom background Image
extern const char kVivaldiStartpagePortraitImage[];
extern const char kVivaldiStartpageLandscapeImage[];
// Start page
// Speed dial/Bookmarks sorting mode
extern const char kVivaldiSpeedDialSortingMode[];
// Speed dial/Bookmarks sorting order e.g ascending/descending
extern const char kVivaldiSpeedDialSortingOrder[];
// Start page layout
extern const char kVivaldiStartPageLayoutStyle[];
// Start page speed dial maximum column
extern const char kVivaldiStartPageSDMaximumColumns[];
// Start page show/hide frequently visited
extern const char kVivaldiStartPageShowFrequentlyVisited[];
// Start page show/hide speed dials
extern const char kVivaldiStartPageShowSpeedDials[];
// Start page show/hide customize button
extern const char kVivaldiStartPageShowCustomizeButton[];
// Start page Custom background Image
extern const char kVivaldiStartpagePortraitImage[];
extern const char kVivaldiStartpageLandscapeImage[];
// Preloaded selected wallpaper name
extern const char kVivaldiStartupWallpaper[];
// Start page reopen with item
extern const char kVivaldiStartPageOpenWithItem[];
// Start page last visited group
extern const char kVivaldiStartPageLastVisitedGroup[];

// Panels
// Settings for opening panel instead of dialog for partial translate
// from webpage.
extern const char kVivaldiPreferTranslatePanel[];
// Content Settings
// Global page zoom value
extern const char kVivaldiPageZoomLevel[];
extern const char kGlobalPageZoomEnabled[];
#endif

#if BUILDFLAG(IS_ANDROID)
// Background media playback for YouTube.
extern const char kBackgroundMediaPlaybackAllowed[];
extern const char kPWADisabled[];
#endif
}  // namespace vivaldiprefs

#endif  // PREFS_VIVALDI_PREF_NAMES_H_
