// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved
#include "browser/features/vivaldi_flag_descriptions.h"

namespace vivaldi_flag {

// Internal names of the features MUST be globally unique, thus prefix those
// with 'vivaldi-'. Also, please follow naming convension, k...InternalName, k...Name,
// k...Description.

const char kChromePagesInternalName[] = "vivaldi-chrome-pages";
const char kChromePagesName[] = "Enable Chrome Pages";
const char kChromePagesDescription[] =
    "Do not redirect chrome pages to vivaldi pages.";

const char kCssModsInternalName[] = "vivaldi-css-mods";
const char kCssModsName[] = "Allow CSS modifications";
const char kCssModsDescription[] =
    "Allow user interface CSS modifications set in Appearance settings.";

const char kDoubleClickMenuInternalName[] = "vivaldi-double-click-menu";
const char kDoubleClickMenuName[] = "Show context menu on a double click";
const char kDoubleClickMenuDescription[] =
    "Use a double click anywhere in the page to show its context menu. "
    "Requires a restart of Vivaldi to take effect.";

extern const char kInternalPageReaderModeInternalName[] = "vivaldi-internal-page-reader-mode";
extern const char kInternalPageReaderModeName[] = "Use internal page for reader mode";
extern const char kInternalPageReaderModeDescription[] = "Should give better results on most pages but is lacking in translation features.";

const char kLocationOverrideInternalName[] = "vivaldi-location-override";
const char kLocationOverrideName[] = "Enable Location Override";
const char kLocationOverrideDescription[] =
    "Spoof and protect your geolocation with Location Override.";

const char kNewPrivacyReportInternalName[] = "vivaldi-new-privacy-report";
const char kNewPrivacyReportName[] = "Use new Privacy Report";
const char kNewPrivacyReportDescription[] =
    "Enable vivaldi:privacy-report page.";

const char kShowNewSpeedDialDialogInternalName[] = "vivaldi-show-new-sd-dialog";
const char kShowNewSpeedDialDialogName[] = "Enable new speed dial dialog";
const char kShowNewSpeedDialDialogDescription[] =
    "When enabled, redesigned new speed dial dialog is presented";

const char kShowTopSitesInternalName[] = "vivaldi-show-top-sites";
const char kShowTopSitesName[] = "Enable top sites";
const char kShowTopSitesDescription[] =
    "When enabled, top sites are shown in start page and speed dial add dialog";

const char kSpeeddialWidgetsInternalName[] = "vivaldi-speeddial-widgets";
const char kSpeeddialWidgetsName[] = "Enable Widgets on Speed Dial Groups";
const char kSpeeddialWidgetsDescription[] =
    "Allow dashboard widgets to be added on speed dial groups.";

const char kTabsButtonInternalName[] = "vivaldi-tabs-button";
const char kTabsButtonName[] = "Enable Tabs Button";
const char kTabsButtonDescription[] = "Make the Tabs Button available to use.";

const char kViewMarkdownAsHTMLInternalName[] = "vivaldi-view-markdown-as-html";
const char kViewMarkdownAsHTMLName[] =
    "Enable 'View as HTML' toggle button in notes editor";
const char kkViewMarkdownAsHTMLDescription[] =
    "When enabled the user is able to toggle between plain text and HTML view";

const char kAddCustomSearchEngineOptionInternalName[] =
    "vivaldi-ios-add-custom-search-engine";
const char kAddCustomSearchEngineOption[] =
    "Show Add Custom Search Engine option";
const char kAddCustomSearchEngineOptionDescription[] =
    "When enabled, Add Custom Search Engine option is visible on Search Engine Settings";

const char kNoteEditorName[] = "vivaldi-note-editor";
const char kNoteEditorOption[] = "Use the new note editor";
const char kNoteEditorDescription[] =
    "Give access to a new note editor with more features.";

#if defined(OEM_AUTOMOTIVE_BUILD)
const char kCinemaModeInternalName[] = "vivaldi-cinema-mode";
const char kCinemaModeName[] = "Vivaldi experimental fullscreen/immersive mode";
const char kCinemaModeDescription[] =
    "This enables Vivaldi's experimental fullscreen/immersive mode for "
    "streaming-sites.";
#endif  // defined(OEM_AUTOMOTIVE_BUILD)

// Please insert your name/description above in alphabetical order.

}  // namespace vivaldi_flag
