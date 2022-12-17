// Copyright (c) 2015-2022 Vivaldi Technologies

#include "prefs/vivaldi_pref_names.h"

namespace vivaldiprefs {

// Profile prefs go here.

const char kAutoUpdateEnabled[] = "vivaldi.autoupdate_enabled";
const char kVivaldiAccountPendingRegistration[] =
    "vivaldi.vivaldi_account.pending_registration";
const char kVivaldiExperiments[] = "vivaldi.experiments";
const char kVivaldiLastTopSitesVacuumDate[] =
    "vivaldi.last_topsites_vacuum_date";
const char kVivaldiPIPPlacement[] = "vivaldi.pip_placement";


// Local state prefs go here
const char kVivaldiAutoUpdateStandalone[] = "vivaldi.autoupdate.standalone";
const char kVivaldiUniqueUserId[] = "vivaldi.unique_user_id";
const char kVivaldiStatsNextDailyPing[] = "vivaldi.stats.next_daily_ping";
const char kVivaldiStatsNextWeeklyPing[] = "vivaldi.stats.next_weekly_ping";
const char kVivaldiStatsNextMonthlyPing[] = "vivaldi.stats.next_monthly_ping";
const char kVivaldiStatsNextTrimestrialPing[] =
    "vivaldi.stats.next_trimestrial_ping";
const char kVivaldiStatsNextSemestrialPing[] =
    "vivaldi.stats.next_semestrial_ping";
const char kVivaldiStatsNextYearlyPing[] = "vivaldi.stats.next_yearly_ping";
const char kVivaldiStatsExtraPing[] = "vivaldi.stats.extra_ping";
const char kVivaldiStatsExtraPingTime[] = "vivaldi.stats.extra_ping_time";
const char kVivaldiStatsPingsSinceLastMonth[] =
    "vivaldi.stats.pings_since_last_month";
const char kVivaldiProfileImagePath[] = "vivaldi.profile_image_path";
const char kVivaldiTranslateLanguageList[] = "vivaldi.translate.language_list";
const char kVivaldiTranslateLanguageListLastUpdate[] =
    "vivaldi.translate.language_list_last_update";
const char kVivaldiAccountServerUrlIdentity[] =
    "vivaldi.vivaldi.account.server_url.identity";
const char kVivaldiAccountServerUrlOpenId[] =
    "vivaldi.vivaldi.account.server_url.open_id";
const char kVivaldiSyncServerUrl[] = "vivaldi.sync.server_url";
const char kVivaldiSyncNotificationsServerUrl[] =
    "vivaldi.sync.notifications.server_url";

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
const char kOldVivaldiUseNativeWindowDecoration[] =
    "vivaldi.use_native_window_decoration";

#if BUILDFLAG(IS_IOS)
// Caches the folder id of user's position in the note hierarchy navigator.
const char kIosNoteCachedFolderId[] = "ios.note.cached_folder_id";

// Caches the scroll position of Notes.
const char kIosNoteCachedTopMostRow[] = "ios.note.cached_top_most_row";

const char kIosNoteFolderDefault[] = "ios.note.default_folder";
#endif
}  // namespace vivaldiprefs
