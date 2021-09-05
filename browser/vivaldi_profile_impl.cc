// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#include "browser/vivaldi_profile_impl.h"

#include "app/vivaldi_apptools.h"
#include "base/command_line.h"
#include "calendar/calendar_model_loaded_observer.h"
#include "calendar/calendar_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/request_filter/adblock_filter/adblock_rule_service.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_factory.h"
#include "components/translate/core/browser/translate_language_list.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "contact/contact_model_loaded_observer.h"
#include "contact/contact_service_factory.h"
#include "content/public/common/content_switches.h"
#include "extensions/buildflags/buildflags.h"
#include "menus/context_menu_service_factory.h"
#include "menus/main_menu_service_factory.h"
#include "menus/menu_model.h"
#include "menus/menu_model_loaded_observer.h"
#include "notes/notes_factory.h"
#include "notes/notes_model.h"
#include "notes/notes_model_loaded_observer.h"
#include "ui/lazy_load_service_factory.h"
#include "vivaldi/prefs/vivaldi_gen_pref_enums.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/api/vivaldi_utilities/vivaldi_utilities_api.h"
#endif

namespace {
class RuleServiceDelegate : public adblock_filter::RuleService::Delegate {
 public:
  RuleServiceDelegate() = default;
  ~RuleServiceDelegate() override = default;
  std::string GetLocaleForDefaultLists() override {
    PrefService* pref_service = g_browser_process->local_state();
    if (pref_service->HasPrefPath(language::prefs::kApplicationLocale))
      return pref_service->GetString(language::prefs::kApplicationLocale);
    return g_browser_process->GetApplicationLocale();
  }
  void RuleServiceDeleted() override { delete this; }
};
}  // namespace

namespace vivaldi {
void VivaldiInitProfile(Profile* profile) {
  PrefService* pref_service = profile->GetPrefs();
  pref_service->SetBoolean(prefs::kOfferTranslateEnabled, false);

  adblock_filter::RuleServiceFactory::GetForBrowserContext(profile)
      ->SetDelegate(new RuleServiceDelegate);

  vivaldi::NotesModel* notes_model =
      vivaldi::NotesModelFactory::GetForBrowserContext(profile);
  notes_model->AddObserver(new vivaldi::NotesModelLoadedObserver(profile));

#if !defined(OS_ANDROID)
  menus::Menu_Model* menu_model =
      menus::MainMenuServiceFactory::GetForBrowserContext(profile);
  menu_model->AddObserver(new menus::MenuModelLoadedObserver());
  // The context menu model content is loaded on demand so no observer here.
  menus::ContextMenuServiceFactory::GetForBrowserContext(profile);

  extensions::VivaldiUtilitiesAPI::GetFactoryInstance()
      ->Get(profile)
      ->PostProfileSetup();

  if (!vivaldi::IsVivaldiRunning())
    return;

  calendar::CalendarService* calendar_service =
      calendar::CalendarServiceFactory::GetForProfile(profile);
  calendar_service->AddObserver(new calendar::CalendarModelLoadedObserver());

  contact::ContactService* contact_service =
      contact::ContactServiceFactory::GetForProfile(profile);
  contact_service->AddObserver(new contact::ContactModelLoadedObserver());

  vivaldi::LazyLoadServiceFactory::GetForProfile(profile);

  if (pref_service->GetBoolean(vivaldiprefs::kWebpagesSmoothScrollingEnabled) ==
      false) {
    vivaldi::CommandLineAppendSwitchNoDup(
        base::CommandLine::ForCurrentProcess(),
        switches::kDisableSmoothScrolling);
  }
#endif
}

}  // namespace vivaldi
