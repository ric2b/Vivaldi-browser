// Copyright (c) 2015-2018 Vivaldi Technologies AS. All rights reserved

#include "extraparts/vivaldi_browser_main_extra_parts.h"

#include <string>

#include "app/vivaldi_apptools.h"
#include "base/command_line.h"
#include "calendar/calendar_model_loaded_observer.h"
#include "calendar/calendar_service_factory.h"
#include "chrome/browser/net/url_info.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "components/datasource/vivaldi_data_source_api.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/translate_language_list.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "contact/contact_model_loaded_observer.h"
#include "contact/contact_service_factory.h"
#include "content/public/common/content_switches.h"
#include "extensions/buildflags/buildflags.h"
#include "notes/notes_factory.h"
#include "notes/notes_model.h"
#include "notes/notes_model_loaded_observer.h"
#include "notes/notesnode.h"
#include "ui/lazy_load_service_factory.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/api/bookmarks/bookmarks_private_api.h"
#include "extensions/api/calendar/calendar_api.h"
#include "extensions/api/contacts/contacts_api.h"
#include "extensions/api/extension_action_utils/extension_action_utils_api.h"
#include "extensions/api/history/history_private_api.h"
#include "extensions/api/import_data/import_data_api.h"
#include "extensions/api/notes/notes_api.h"
#include "extensions/api/prefs/prefs_api.h"
#include "extensions/api/runtime/runtime_api.h"
#include "extensions/api/settings/settings_api.h"
#include "extensions/api/show_menu/show_menu_api.h"
#include "extensions/api/sync/sync_api.h"
#include "extensions/api/tabs/tabs_private_api.h"
#include "extensions/api/vivaldi_utilities/vivaldi_utilities_api.h"
#include "extensions/api/window/window_private_api.h"
#include "extensions/api/zoom/zoom_api.h"
#include "extensions/vivaldi_extensions_init.h"
#include "ui/devtools/devtools_connector.h"
#endif

VivaldiBrowserMainExtraParts::VivaldiBrowserMainExtraParts() {}

VivaldiBrowserMainExtraParts::~VivaldiBrowserMainExtraParts() {}

// Overridden from ChromeBrowserMainExtraParts:
void VivaldiBrowserMainExtraParts::PostEarlyInitialization() {
  // base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (vivaldi::IsVivaldiRunning()) {
    // Options to be set when Vivaldi is running, but not during unit tests
  }
}

void VivaldiBrowserMainExtraParts::
    EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  translate::TranslateLanguageList::DisableUpdate();
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::CalendarAPI::GetFactoryInstance();
  extensions::ContactsAPI::GetFactoryInstance();
  extensions::VivaldiBookmarksAPI::GetFactoryInstance();
  extensions::DevtoolsConnectorAPI::GetFactoryInstance();
  extensions::ExtensionActionUtilFactory::GetInstance();
  extensions::ImportDataAPI::GetFactoryInstance();
  extensions::NotesAPI::GetFactoryInstance();
  extensions::TabsPrivateAPI::GetFactoryInstance();
  extensions::ShowMenuAPI::GetFactoryInstance();
  extensions::SyncAPI::GetFactoryInstance();
  extensions::VivaldiDataSourcesAPI::GetFactoryInstance();
  extensions::VivaldiExtensionInit::GetFactoryInstance();
  extensions::VivaldiPrefsApiNotificationFactory::GetInstance();
  extensions::VivaldiRuntimeFeaturesFactory::GetInstance();
  extensions::VivaldiUtilitiesAPI::GetFactoryInstance();
  extensions::VivaldiWindowsAPI::GetFactoryInstance();
  extensions::ZoomAPI::GetFactoryInstance();
  extensions::HistoryPrivateAPI::GetFactoryInstance();
#endif
}

void VivaldiBrowserMainExtraParts::PreProfileInit() {
  EnsureBrowserContextKeyedServiceFactoriesBuilt();
}

void VivaldiBrowserMainExtraParts::PostProfileInit() {
#if !defined(OS_ANDROID)
  Profile* profile = ProfileManager::GetActiveUserProfile();

  vivaldi::Notes_Model* notes_model =
      vivaldi::NotesModelFactory::GetForBrowserContext(profile);
  notes_model->AddObserver(new vivaldi::NotesModelLoadedObserver(profile));

  calendar::CalendarService* calendar_service =
      calendar::CalendarServiceFactory::GetForProfile(profile);
  calendar_service->AddObserver(new calendar::CalendarModelLoadedObserver());

  contact::ContactService* contact_service =
      contact::ContactServiceFactory::GetForProfile(profile);
  contact_service->AddObserver(new contact::ContactModelLoadedObserver());

  if (!vivaldi::IsVivaldiRunning())
    return;

  vivaldi::LazyLoadServiceFactory::GetForProfile(profile);

  PrefService* pref_service = profile->GetPrefs();
  pref_service->SetBoolean(prefs::kOfferTranslateEnabled, false);

  if (pref_service->GetBoolean(vivaldiprefs::kWebpagesSmoothScrollingEnabled) ==
      false) {
    vivaldi::CommandLineAppendSwitchNoDup(
        base::CommandLine::ForCurrentProcess(),
        switches::kDisableSmoothScrolling);
  }
#endif
}
