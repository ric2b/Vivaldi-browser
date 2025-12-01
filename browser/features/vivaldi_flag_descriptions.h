// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef BROWSER_FLAGS_VIVALDI_FLAG_DEFINITIONS_H_
#define BROWSER_FLAGS_VIVALDI_FLAG_DEFINITIONS_H_

// This file is included to chromium/chrome/browser/about_flags.cc.
// That means you need to add flag descriptions, feature choices, and custom
// types here.

// Includes here might be unused but are required for vivaldi_flags.inc file.
#include "browser/features/vivaldi_features.h"
#include "build/build_config.h"

namespace vivaldi_flag {

// Cross-platform
extern const char kChromePagesInternalName[];
extern const char kChromePagesName[];
extern const char kChromePagesDescription[];

extern const char kCssModsInternalName[];
extern const char kCssModsName[];
extern const char kCssModsDescription[];

extern const char kDoubleClickMenuInternalName[];
extern const char kDoubleClickMenuName[];
extern const char kDoubleClickMenuDescription[];

extern const char kInternalPageReaderModeInternalName[];
extern const char kInternalPageReaderModeName[];
extern const char kInternalPageReaderModeDescription[];

extern const char kLocationOverrideInternalName[];
extern const char kLocationOverrideName[];
extern const char kLocationOverrideDescription[];

extern const char kNewAboutPageInternalName[];
extern const char kNewAboutPageName[];
extern const char kNewAboutPageDescription[];

extern const char kNewPrivacyReportInternalName[];
extern const char kNewPrivacyReportName[];
extern const char kNewPrivacyReportDescription[];

extern const char kShowNewSpeedDialDialogInternalName[];
extern const char kShowNewSpeedDialDialogName[];
extern const char kShowNewSpeedDialDialogDescription[];

extern const char kShowTopSitesInternalName[];
extern const char kShowTopSitesName[];
extern const char kShowTopSitesDescription[];

extern const char kSpeeddialWidgetsInternalName[];
extern const char kSpeeddialWidgetsName[];
extern const char kSpeeddialWidgetsDescription[];

extern const char kViewMarkdownAsHTMLInternalName[];
extern const char kViewMarkdownAsHTMLName[];
extern const char kkViewMarkdownAsHTMLDescription[];

extern const char kAddCustomSearchEngineOptionInternalName[];
extern const char kAddCustomSearchEngineOption[];
extern const char kAddCustomSearchEngineOptionDescription[];

extern const char kNoteEditorName[];
extern const char kNoteEditorOption[];
extern const char kNoteEditorDescription[];

#if defined(OEM_AUTOMOTIVE_BUILD)
extern const char kCinemaModeInternalName[];
extern const char kCinemaModeName[];
extern const char kCinemaModeDescription[];
#endif  // defined(OEM_AUTOMOTIVE_BUILD)

// Please add names and descriptions above in alphabetical order.

}  // namespace vivaldi_flag

#endif  // BROWSER_FLAGS_VIVALDI_FLAG_DEFINITIONS_H_
