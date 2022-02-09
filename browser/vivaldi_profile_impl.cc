// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#include "browser/vivaldi_profile_impl.h"

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_version_info.h"
#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "base/version.h"
#include "calendar/calendar_model_loaded_observer.h"
#include "calendar/calendar_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/common/pref_names.h"
#include "components/content_injection/content_injection_service_factory.h"
#include "components/db/mail_client/mail_client_service_factory.h"
#include "components/language/core/browser/pref_names.h"
#include "components/page_actions/page_actions_service_factory.h"
#include "components/prefs/pref_change_registrar.h"
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
#include "extensions/api/features/vivaldi_runtime_feature.h"
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

class VivaldiProfileObserver : public ProfileObserver {
 public:
  VivaldiProfileObserver(Profile* profile) : profile_(profile) {
    profile->AddObserver(this);

    AddPrefsObservers();

    OnPrefsChange(vivaldiprefs::kTranslateEnabled);
  }
  ~VivaldiProfileObserver() override = default;

  void AddPrefsObservers() {
    prefs_registrar_ = std::make_unique<PrefChangeRegistrar>();
    prefs_registrar_->Init(profile_->GetPrefs());

    // callback follows the lifetime of the registrar.
    prefs_registrar_->Add(
        vivaldiprefs::kTranslateEnabled,
        base::BindRepeating(&VivaldiProfileObserver::OnPrefsChange,
                            base::Unretained(this)));
    prefs_registrar_->Add(
        prefs::kOfferTranslateEnabled,
        base::BindRepeating(&VivaldiProfileObserver::OnPrefsChange,
                            base::Unretained(this)));
  }

  void RemovePrefsObservers() {
    if (prefs_registrar_) {
      prefs_registrar_->RemoveAll();
      prefs_registrar_ = nullptr;
    }
  }

  void OnProfileWillBeDestroyed(Profile* profile) override {
    profile->RemoveObserver(this);
    RemovePrefsObservers();
    delete this;
  }

 private:
  void OnPrefsChange(const std::string& prefs) {
    PrefService* pref_service = profile_->GetPrefs();
    // We used to hardcode kOfferTranslateEnabled to disabled, so we need a new
    // prefs we can use to let the chromium prefs mirror. We sync these 2 prefs
    // so what whatever method the user uses to set them, they are in sync.
    if (prefs == vivaldiprefs::kTranslateEnabled) {
      bool translate_enabled =
          pref_service->GetBoolean(vivaldiprefs::kTranslateEnabled);
      pref_service->SetBoolean(prefs::kOfferTranslateEnabled,
                               translate_enabled);
    } else if (prefs == prefs::kOfferTranslateEnabled) {
      // Also handle if the user uses chrome://settings to disable translation
      // offers.
      bool translate_enabled =
          pref_service->GetBoolean(prefs::kOfferTranslateEnabled);
      pref_service->SetBoolean(vivaldiprefs::kTranslateEnabled,
                               translate_enabled);
    }
  }

  VivaldiProfileObserver(const VivaldiProfileObserver&) = delete;
  VivaldiProfileObserver& operator=(const VivaldiProfileObserver&) = delete;

  std::unique_ptr<PrefChangeRegistrar> prefs_registrar_;
  Profile* profile_;
};

}  // namespace

namespace vivaldi {

void PerformUpdates(Profile* profile) {
  PrefService* pref_service = profile->GetPrefs();
  base::Version version(::vivaldi::GetVivaldiVersion());
  base::Version last_seen_version(
      pref_service->GetString(vivaldiprefs::kStartupLastSeenVersion));

  // We shouldn't update any prefs if in incognito or guest mode.
  if (profile->IsIncognitoProfile() || profile->IsGuestSession()) {
    return;
  }
  if (last_seen_version.IsValid() && last_seen_version != version) {
    // Force translation on if we upgrade from 3.8 to 3.9 or 4.x
    if (last_seen_version.components()[0] == 3 &&
        (version.components()[0] == 4 ||
         (version.components()[0] == 3 && version.components()[1] == 9))) {
      pref_service->SetBoolean(prefs::kOfferTranslateEnabled, true);
      pref_service->SetBoolean(vivaldiprefs::kTranslateEnabled, true);
    }
  }
}

void VivaldiInitProfile(Profile* profile) {
  adblock_filter::RuleServiceFactory::GetForBrowserContext(profile)
      ->SetDelegate(new RuleServiceDelegate);
  content_injection::ServiceFactory::GetForBrowserContext(profile);
  page_actions::ServiceFactory::GetForBrowserContext(profile);

  vivaldi::NotesModel* notes_model =
      vivaldi::NotesModelFactory::GetForBrowserContext(profile);
  notes_model->AddObserver(new vivaldi::NotesModelLoadedObserver(profile));

  PerformUpdates(profile);

  if (vivaldi::IsVivaldiRunning()) {
    // Manages its own lifetime.
    new VivaldiProfileObserver(profile);
  }

#if !defined(OS_ANDROID)
  PrefService* pref_service = profile->GetPrefs();

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

  mail_client::MailClientService* mail_client_service =
      mail_client::MailClientServiceFactory::GetForProfile(profile);
  mail_client_service->AddObserver(new mail_client::MailClientModelObserver());

  vivaldi::LazyLoadServiceFactory::GetForProfile(profile);

  if (pref_service->GetBoolean(vivaldiprefs::kWebpagesSmoothScrollingEnabled) ==
      false) {
    vivaldi::CommandLineAppendSwitchNoDup(
        base::CommandLine::ForCurrentProcess(),
        switches::kDisableSmoothScrolling);
  }

  // Hook to redisplay the Welcome page. When needed, append new feature in
  // src/prefs_definitions.json and update test here.
  // TODO: Move this to PerformUpdates() above.
  if (pref_service->GetInteger(vivaldiprefs::kStartupHasSeenFeature) <
      static_cast<int>(vivaldiprefs::StartupHasSeenFeatureValues::kMail)) {
    if (IsBetaOrFinal()) {
      std::string version = ::vivaldi::GetVivaldiVersionString();
      std::vector<std::string> version_array = base::SplitString(
        version, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      if (version_array.size() == 4) {
        int major;
        if (base::StringToInt(version_array[0], &major) && major >= 4) {
          pref_service->SetInteger(vivaldiprefs::kStartupHasSeenFeature,
            static_cast<int>(vivaldiprefs::StartupHasSeenFeatureValues::kMail));
          pref_service->SetBoolean(prefs::kHasSeenWelcomePage, false);
        }
      }
    }
  }

#endif
}

}  // namespace vivaldi
