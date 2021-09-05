// Copyright (c) 2015-2018 Vivaldi Technologies AS. All rights reserved

#include "extraparts/vivaldi_browser_main_extra_parts.h"

#include <string>

#include "app/vivaldi_apptools.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "browser/stats_reporter.h"
#include "calendar/calendar_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/adverse_adblocking/adverse_ad_filter_list_factory.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/datasource/vivaldi_data_source_api.h"
#include "components/translate/core/browser/translate_language_list.h"
#include "contact/contact_service_factory.h"
#include "content/public/common/content_switches.h"
#include "extensions/buildflags/buildflags.h"
#include "notes/notes_factory.h"
#include "ui/lazy_load_service_factory.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/api/bookmark_context_menu/bookmark_context_menu_api.h"
#include "extensions/api/bookmarks/bookmarks_private_api.h"
#include "extensions/api/calendar/calendar_api.h"
#include "extensions/api/contacts/contacts_api.h"
#include "extensions/api/content_blocking/content_blocking_api.h"
#include "extensions/api/extension_action_utils/extension_action_utils_api.h"
#include "extensions/api/history/history_private_api.h"
#include "extensions/api/import_data/import_data_api.h"
#include "extensions/api/menu_content/menu_content_api.h"
#include "extensions/api/notes/notes_api.h"
#include "extensions/api/prefs/prefs_api.h"
#include "extensions/api/runtime/runtime_api.h"
#include "extensions/api/settings/settings_api.h"
#include "extensions/api/sync/sync_api.h"
#include "extensions/api/tabs/tabs_private_api.h"
#include "extensions/api/vivaldi_account/vivaldi_account_api.h"
#include "extensions/api/vivaldi_utilities/vivaldi_utilities_api.h"
#include "extensions/api/window/window_private_api.h"
#include "extensions/api/zoom/zoom_api.h"
#include "extensions/vivaldi_extensions_init.h"
#include "ui/devtools/devtools_connector.h"
#endif

#if defined(OS_LINUX)
#include "base/environment.h"
#include "base/nix/xdg_util.h"
#endif

VivaldiBrowserMainExtraParts::VivaldiBrowserMainExtraParts() {}

VivaldiBrowserMainExtraParts::~VivaldiBrowserMainExtraParts() {}

// Overridden from ChromeBrowserMainExtraParts:
void VivaldiBrowserMainExtraParts::PostEarlyInitialization() {
  if (!vivaldi::IsVivaldiRunning()) {
    return;
  }
  stats_reporter_ = vivaldi::StatsReporter::CreateInstance();
#if defined(OS_LINUX) || defined(OS_MACOSX)
  base::FilePath messaging(
    // Hardcoded from chromium/chrome/common/chrome_paths.cc
#if defined(OS_MACOSX)
    FILE_PATH_LITERAL("/Library/Google/Chrome/NativeMessagingHosts")
#else   // OS_MACOSX
    FILE_PATH_LITERAL("/etc/opt/chrome/native-messaging-hosts")
#endif  // OS_MACOSX
  );
  base::PathService::Override(chrome::DIR_NATIVE_MESSAGING, messaging);
#endif
#if defined(OS_LINUX)
  {
    base::FilePath cur;
    std::unique_ptr<base::Environment> env(base::Environment::Create());
    cur = base::nix::GetXDGDirectory(
        env.get(), base::nix::kXdgConfigHomeEnvVar, base::nix:: kDotConfigDir);
    cur = cur.Append("google-chrome");
    cur = cur.Append(FILE_PATH_LITERAL("PepperFlash"));
    cur = cur.Append(FILE_PATH_LITERAL("latest-component-updated-flash"));

    base::PathService::Override(chrome::FILE_COMPONENT_FLASH_HINT, cur);
  }
  base::FilePath pepper(
      FILE_PATH_LITERAL("/usr/lib/adobe-flashplugin/libpepflashplayer.so"));
  base::PathService::Override(chrome::FILE_PEPPER_FLASH_SYSTEM_PLUGIN,
                              pepper);
#endif
}

void VivaldiBrowserMainExtraParts::
    EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  translate::TranslateLanguageList::DisableUpdate();
#if !defined(OS_ANDROID)
  vivaldi::NotesModelFactory::GetInstance();
  calendar::CalendarServiceFactory::GetInstance();
  contact::ContactServiceFactory::GetInstance();
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::BookmarkContextMenuAPI::GetFactoryInstance();
  extensions::CalendarAPI::GetFactoryInstance();
  extensions::ContactsAPI::GetFactoryInstance();
  extensions::ContentBlockingAPI::GetFactoryInstance();
  extensions::VivaldiBookmarksAPI::GetFactoryInstance();
  extensions::DevtoolsConnectorAPI::GetFactoryInstance();
  extensions::ExtensionActionUtilFactory::GetInstance();
  extensions::ImportDataAPI::GetFactoryInstance();
  extensions::NotesAPI::GetFactoryInstance();
  extensions::MenuContentAPI::GetFactoryInstance();
  extensions::TabsPrivateAPI::GetFactoryInstance();
  extensions::SyncAPI::GetFactoryInstance();
  extensions::VivaldiAccountAPI::GetFactoryInstance();
  extensions::VivaldiDataSourcesAPI::InitFactory();
  extensions::VivaldiExtensionInit::GetFactoryInstance();
  extensions::VivaldiPrefsApiNotificationFactory::GetInstance();
  extensions::VivaldiRuntimeFeaturesFactory::GetInstance();
  extensions::VivaldiUtilitiesAPI::GetFactoryInstance();
  extensions::VivaldiWindowsAPI::GetFactoryInstance();
  extensions::ZoomAPI::GetFactoryInstance();
  extensions::HistoryPrivateAPI::GetFactoryInstance();
#endif
  VivaldiAdverseAdFilterListFactory::GetFactoryInstance();
  if (!vivaldi::IsVivaldiRunning())
    return;

#if !defined(OS_ANDROID)
  vivaldi::LazyLoadServiceFactory::GetInstance();
#endif
}

void VivaldiBrowserMainExtraParts::PreProfileInit() {
  EnsureBrowserContextKeyedServiceFactoriesBuilt();
}

void VivaldiBrowserMainExtraParts::PostProfileInit() {
#if !defined(OS_ANDROID)
  if (!vivaldi::IsVivaldiRunning())
    return;

  vivaldi::CommandLineAppendSwitchNoDup(base::CommandLine::ForCurrentProcess(),
                                        switches::kSavePageAsMHTML);

  // Sanetize contentsettings visible in our web-ui.
  std::list<ContentSettingsType> ui_exposed_settings = {
      ContentSettingsType::BLUETOOTH_SCANNING,
      ContentSettingsType::GEOLOCATION,
      ContentSettingsType::MEDIASTREAM_CAMERA,
      ContentSettingsType::MEDIASTREAM_MIC,
      ContentSettingsType::MIDI_SYSEX,
      ContentSettingsType::NOTIFICATIONS,
      ContentSettingsType::POPUPS,
      ContentSettingsType::SENSORS,
      ContentSettingsType::SOUND
  };

  Profile* profile =
      g_browser_process->profile_manager()->GetActiveUserProfile();

  HostContentSettingsMap* content_settings_map =
    HostContentSettingsMapFactory::GetForProfile(profile);

  std::list<ContentSettingsType>::iterator it;
  for (it = ui_exposed_settings.begin(); it != ui_exposed_settings.end();
       it++) {
    ContentSettingsType content_type = *it;
    ContentSetting default_setting =
        HostContentSettingsMapFactory::GetForProfile(profile)
            ->GetDefaultContentSetting(content_type, nullptr);
    const content_settings::ContentSettingsInfo* info =
        content_settings::ContentSettingsRegistry::GetInstance()->Get(
            content_type);

    bool is_valid_settings_value = info->IsDefaultSettingValid(default_setting);
    if (!is_valid_settings_value) {
      LOG(INFO)
          << "Vivaldi changed invalid default setting "
          << extensions::content_settings_helpers::ContentSettingsTypeToString(
                 content_type);
      default_setting = CONTENT_SETTING_DEFAULT;
      content_settings_map->SetDefaultContentSetting(content_type,
                                                     default_setting);
    }
  }

#endif //OS_ANDROID

}
