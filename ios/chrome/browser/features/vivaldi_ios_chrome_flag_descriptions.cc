// Copyright 2024 Vivaldi Technologies. All rights reserved.

#include "ios/chrome/browser/features/vivaldi_ios_chrome_flag_descriptions.h"

// This file declares strings used in chrome://flags. These messages are not
// translated, because instead of end-users they target Chromium developers and
// testers. See https://crbug.com/587272 and https://crbug.com/703134 for more
// details.

namespace flag_descriptions {

const char kShowTopSites[] = "Enable top sites";
const char kShowTopSitesDescription[] =
    "When enabled, top sites are shown in start page and speed dial add dialog";

const char kViewMarkdownAsHTMLName[] =
    "Enable 'View as HTML' toggle button in notes editor";
const char kkViewMarkdownAsHTMLDescription[] =
    "When enabled the user is able to toggle between plain text and HTML view";

const char kShowNewSpeedDialDialog[] = "Enable new speed dial dialog";
const char kShowNewSpeedDialDialogDescription[] =
    "When enabled, redesigned new speed dial dialog is presented";

// Please insert your name/description above in alphabetical order.

}  // namespace flag_descriptions
