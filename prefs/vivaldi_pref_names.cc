// Copyright (c) 2015 Vivaldi Technologies

#include "prefs/vivaldi_pref_names.h"

namespace vivaldiprefs {

// Profile prefs go here.

const char kAlwaysLoadPinnedTabAfterRestore[] =
    "vivaldi.always_load_restored_pinned_tabs";
const char kDeferredTabLoadingAfterRestore[] =
    "vivaldi.deferred_tab_loading_after_restore";
const char kMousegesturesEnabled[] = "mousegestures_enabled";
const char kRockerGesturesEnabled[] = "vivaldi.rocker_gestures_enabled";
const char kSmoothScrollingEnabled[] = "smooth_scrolling_enabled";
const char kVivaldiTabsToLinks[] = "vivaldi.tabs_to_links";
const char kVivaldiLastTopSitesVacuumDate[] =
    "vivaldi.last_topsites_vacuum_date";
const char kVivaldiTabZoom[] = "vivaldi.tab_zoom_enabled";
const char kVivaldiHomepage[] = "vivaldi.home_page";
const char kVivaldiNumberOfDaysToKeepVisits[] =
    "vivaldi.days_to_keep_visits";

// Used to store active vivaldi experiments
const char kVivaldiExperiments[] = "vivaldi.experiments";

#if defined(USE_AURA)
const char kHideMouseCursorInFullscreen[] = "vivaldi.hide_mouse_in_fullscreen";
#endif  // USE_AURA

#if defined(OS_MACOSX)
const char kAppleKeyboardUIMode[] = "vivaldi.apple_keyboard_ui_mode";
const char kAppleMiniaturizeOnDoubleClick[] =
    "vivaldi.apple_miniaturize_on_double_click";
const char kAppleAquaColorVariant[] = "vivaldi.apple_aqua_color_variant";
const char kAppleInterfaceStyle[] = "vivaldi.apple_interface_style";
const char kTableViewDefaultSizeMode[] = "vivaldi.table_view_default_size_mode";
const char kAppleActionOnDoubleClick[] = "vivaldi.apple_action_on_double_click";
const char kSwipeScrollDirection[] = "vivaldi.swipe_scroll_direction";
#endif

// Local state prefs go here
const char kVivaldiUniqueUserId[] = "vivaldi.unique_user_id";

}  // namespace vivaldiprefs
