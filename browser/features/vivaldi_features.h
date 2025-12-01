// Copyright 2025 Vivaldi Technologies. All rights reserved.

#ifndef BROWSER_FEATURES_VIVALDI_FEATURES_H_
#define BROWSER_FEATURES_VIVALDI_FEATURES_H_

#include "base/feature_list.h"

// Flags must be in alphabetical order.
namespace vivaldi_features {

BASE_DECLARE_FEATURE(kAddCustomSearchEngineOption);
bool IsAddCustomSearchEngineEnabled();

BASE_DECLARE_FEATURE(kChromePages);

BASE_DECLARE_FEATURE(kCssMods);

BASE_DECLARE_FEATURE(kDoubleClickMenu);

BASE_DECLARE_FEATURE(kInternalPageReaderMode);

BASE_DECLARE_FEATURE(kLocationOverride);

BASE_DECLARE_FEATURE(kNewAboutPage);

BASE_DECLARE_FEATURE(kNewPrivacyReport);

BASE_DECLARE_FEATURE(kNoteEditor);

BASE_DECLARE_FEATURE(kShowNewSpeedDialDialog);
bool IsNewSpeedDialDialogEnabled();

BASE_DECLARE_FEATURE(kShowTopSites);
bool IsTopSitesEnabled();

BASE_DECLARE_FEATURE(kSpeeddialWidgets);

BASE_DECLARE_FEATURE(kViewMarkdownAsHTML);
bool IsViewMarkdownAsHTMLEnabled();

#if defined(OEM_AUTOMOTIVE_BUILD)
BASE_DECLARE_FEATURE(kCinemaMode);
#endif  // defined(OEM_AUTOMOTIVE_BUILD)

}  // namespace vivaldi_features

#endif  // BROWSER_FEATURES_VIVALDI_FEATURES_H_
