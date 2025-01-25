// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_CHROME_BROWSER_FEATURES_VIVALDI_FEATURES_H_
#define IOS_CHROME_BROWSER_FEATURES_VIVALDI_FEATURES_H_

#include "base/feature_list.h"

// Remember to add a corresponding entry to
// chromium/ios/chrome/browser/flags/about_flags.mm
// so the flag gets picked up by the flag UI

// Feature flag to enable top sites.
BASE_DECLARE_FEATURE(kShowTopSites);

// Whether the top site is enabled.
bool IsTopSitesEnabled();

BASE_DECLARE_FEATURE(kViewMarkdownAsHTML);

bool IsViewMarkdownAsHTMLEnabled();

// Feature flag to enable new speed dial dialog.
BASE_DECLARE_FEATURE(kShowNewSpeedDialDialog);

// Whether new speed dial dialog is enabled
bool IsNewSpeedDialDialogEnabled();

#endif  // IOS_CHROME_BROWSER_FEATURES_VIVALDI_FEATURES_H_
