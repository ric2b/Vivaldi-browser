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
const char kVivaldiSyncServerUrl[] = "vivaldi.sync.server_url";
const char kVivaldiSyncNotificationsServerUrl[] =
    "vivaldi.sync.notifications.server_url";

const char kVivaldiClientHintsBrand[] = "vivaldi.ClientHintsBrand";
const char kVivaldiClientHintsBrandAppendVivaldi[] =
    "vivaldi.ClientHintsBrandAppendVivaldi";
const char kVivaldiClientHintsBrandCustomBrand[] =
    "vivaldi.ClientHintsCustomBrand";
const char kVivaldiClientHintsBrandCustomBrandVersion[] =
    "vivaldi.ClientHintsCustomBrandVersion";

const char kVivaldiCrashReportingConsentGranted[] =
    "vivaldi.CrashReportingConsentGranted";

const char kVivaldiPreferredColorScheme[] =
    "vivaldi.PreferredColorScheme";
#if BUILDFLAG(IS_IOS)
// Dict preference which tracks the current elements and order of the overflow
// menu's vivaldi actions.
const char kOverflowMenuVivaldiActionsOrder[] =
    "overflow_menu.vivaldi_actions_order";

// Caches the folder id of user's position in the note hierarchy navigator.
const char kVivaldiNoteCachedFolderId[] = "vivaldi.note.cached_folder_id";

// Caches the scroll position of Notes.
const char kVivaldiNoteCachedTopMostRow[] = "vivaldi.note.cached_top_most_row";

const char kVivaldiNoteFolderDefault[] = "vivaldi.note.default_folder";

// Sort order of notes
const char kVivaldiNotesSortingMode[] = "vivaldi.notes.sorting_mode";
const char kVivaldiNotesSortingOrder[] = "vivaldi.notes.sorting_order";

// Setting for folder visiblity on bookmark folder page
const char kVivaldiBookmarkFoldersViewMode[] =
    "vivaldi.bookmark_folders.view_mode";

const char kVivaldiBookmarksSortingMode[] = "vivaldi.bookmarks.sorting_mode";
const char kVivaldiBookmarksSortingOrder[] = "vivaldi.bookmarks.sorting_order";

// Search Engine
const char kVivaldiEnableSearchEngineNickname[] =
    "vivaldi.search_engine.enable_nickname";

// Address bar
const char kVivaldiShowFullAddressEnabled[] =
    "vivaldi.addressbar.show_full_address";

// Tabs
// Desktop style tabs enabled status
const char kVivaldiDesktopTabsEnabled[] = "vivaldi.desktop_tabs.mode";
// Reverse search suggestion results order state for bottom address bar
const char kVivaldiReverseSearchResultsEnabled[] =
    "vivaldi.tabs.reverse_search_results";
// Tab stack use status
const char kVivaldiTabStackEnabled[] = "vivaldi.desktop_tabs.tab_stack";
// Show X button in background tabs
const char kVivaldiShowXButtonBackgroundTabsEnabled[] =
    "vivaldi.tabs.show_x_button_background_tabs";
// Open new tab when last open tab is closed
const char kVivaldiOpenNTPOnClosingLastTabEnabled[] =
    "vivaldi.tabs.open_ntp_on_closing_last_tab";
// Focus omnibox on new tab page
const char kVivaldiFocusOmniboxOnNTPEnabled[] =
    "vivaldi.tabs.focus_omnibox_on_ntp";
const char kVivaldiNewTabURL[] = "vivaldi.tabs.newtab.url";
const char kVivaldiNewTabSetting[] = "vivaldi.tabs.newtab.setting";

// General Settings
const char kVivaldiHomepageURL[] = "vivaldi.general.homepage.url";
const char kVivaldiHomepageEnabled[] = "vivaldi.general.homepage.enabled";

// Apearance
const char kVivaldiAppearanceMode[] = "vivaldi.appearance.selected.mode";
const char kVivaldiWebsiteAppearanceStyle[] =
    "vivaldi.appearance.website_appearance.style";
const char kVivaldiWebsiteAppearanceForceDarkTheme[] =
    "vivaldi.appearance.website_appearance.force_dark_theme";
const char kVivaldiCustomAccentColor[] =
    "vivaldi.appearance.custom.accent_color";
const char kVivaldiDynamicAccentColorEnabled[] =
    "vivaldi.appearance.dynamic.accent_color";

// Start page
const char kVivaldiSpeedDialSortingMode[] = "vivaldi.speed_dial.sorting_mode";
const char kVivaldiSpeedDialSortingOrder[] = "vivaldi.speed_dial.sorting_order";
const char kVivaldiStartPageLayoutStyle[] = "vivaldi.start_page.layout_style";
const char kVivaldiStartPageSDMaximumColumns[] =
    "vivaldi.start_page.speed_dial.maximum_columns";
const char kVivaldiStartPageShowFrequentlyVisited[] =
    "vivaldi.start_page.show_frequently_visited";
const char kVivaldiStartPageShowSpeedDials[] =
    "vivaldi.start_page.show_speed_dials";
const char kVivaldiStartPageShowCustomizeButton[] =
    "vivaldi.start_page.show_customize_button";
const char kVivaldiStartpagePortraitImage[] =
    "vivaldi.appearance.startpage.image";
const char kVivaldiStartpageLandscapeImage[] =
    "vivaldi.appearance.startpage_image.landscape";
const char kVivaldiStartupWallpaper[] = "vivaldi.startup.wallpaper.name";
const char kVivaldiStartPageOpenWithItem[] =
    "vivaldi.start_page.open_with.item";
const char kVivaldiStartPageLastVisitedGroup[] =
    "vivaldi.start_page.last_visited_group";

// Panels
const char kVivaldiPreferTranslatePanel[] = "vivaldi.translate.prefer_panel";

// Content Settings
extern const char kVivaldiPageZoomLevel[] =
    "vivaldi.content_setting.pagezoom.level";
extern const char kGlobalPageZoomEnabled[] =
    "vivaldi.content_setting.pagezoom.global_enabled";
#endif

#if BUILDFLAG(IS_ANDROID)
const char kBackgroundMediaPlaybackAllowed[] =
    "vivaldi.background.media_playback.allowed";
const char kPWADisabled[] =
    "vivaldi.site.PWADisabled.disabled";
#endif
}  // namespace vivaldiprefs
