// Copyright (c) 2015 Vivaldi Technologies

#include "prefs/vivaldi_pref_names.h"

namespace vivaldiprefs {

// Profile prefs go here.

const char kAutoUpdateEnabled[] = "vivaldi.autoupdate_enabled";

// Old pref names that have been changed during the migration
// to the new prefs api.
const char kOldAlwaysLoadPinnedTabAfterRestore[] =
    "vivaldi.always_load_restored_pinned_tabs";
const char kOldDeferredTabLoadingAfterRestore[] =
    "vivaldi.deferred_tab_loading_after_restore";
#if defined(USE_AURA)
const char kOldHideMouseCursorInFullscreen[] =
    "vivaldi.hide_mouse_in_fullscreen";
#endif  // USE_AURA
const char kOldMousegesturesEnabled[] = "mousegestures_enabled";
const char kOldPluginsWidevideEnabled[] = "plugins.widevine_enabled";
const char kOldRockerGesturesEnabled[] = "vivaldi.rocker_gestures_enabled";
const char kOldSmoothScrollingEnabled[] = "smooth_scrolling_enabled";
const char kOldVivaldiCaptureDirectory[] = "vivaldi.capture_directory";
const char kOldVivaldiHomepage[] = "vivaldi.home_page";
const char kOldVivaldiNumberOfDaysToKeepVisits[] =
    "vivaldi.days_to_keep_visits";
const char kOldVivaldiTabZoom[] = "vivaldi.tab_zoom_enabled";
const char kOldVivaldiTabsToLinks[] = "vivaldi.tabs_to_links";

// Used to store active vivaldi experiments
const char kVivaldiExperiments[] = "vivaldi.experiments";
const char kVivaldiLastTopSitesVacuumDate[] =
    "vivaldi.last_topsites_vacuum_date";

// Local state prefs go here
const char kVivaldiUniqueUserId[] = "vivaldi.unique_user_id";

// Enable native window decoration
const char kOldVivaldiUseNativeWindowDecoration[] =
    "vivaldi.use_native_window_decoration";

}  // namespace vivaldiprefs
