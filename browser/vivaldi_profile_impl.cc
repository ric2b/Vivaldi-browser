// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#include "browser/vivaldi_profile_impl.h"

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/common/content_switches.h"
#include "extensions/buildflags/buildflags.h"

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_version_info.h"
#include "browser/removed_partners_tracker.h"
#include "browser/vivaldi_runtime_feature.h"
#include "calendar/calendar_model_loaded_observer.h"
#include "calendar/calendar_service_factory.h"
#include "components/ad_blocker/adblock_rule_service.h"
#include "components/content_injection/content_injection_service_factory.h"
#include "components/datasource/vivaldi_data_source.h"
#include "components/datasource/vivaldi_web_source.h"
#include "components/db/mail_client/mail_client_service_factory.h"
#include "components/direct_match/direct_match_service_factory.h"
#include "components/notes/notes_factory.h"
#include "components/notes/notes_model.h"
#include "components/notes/notes_model_loaded_observer.h"
#include "components/page_actions/page_actions_service_factory.h"
#include "components/ping_block/ping_block.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_factory.h"
#include "components/request_filter/request_filter_manager.h"
#include "components/request_filter/request_filter_manager_factory.h"
#include "components/translate/core/browser/translate_language_list.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "contact/contact_model_loaded_observer.h"
#include "contact/contact_service_factory.h"
#include "menus/context_menu_service_factory.h"
#include "menus/main_menu_service_factory.h"
#include "menus/menu_model.h"
#include "menus/menu_model_loaded_observer.h"
#include "sessions/index_service_factory.h"
#include "ui/lazy_load_service_factory.h"
#include "vivaldi/prefs/vivaldi_gen_pref_enums.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

// locked keystore handling
#include "extraparts/vivaldi_keystore_checker.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/api/vivaldi_utilities/vivaldi_utilities_api.h"
#endif

namespace {
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
        translate::prefs::kOfferTranslateEnabled,
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
      pref_service->SetBoolean(translate::prefs::kOfferTranslateEnabled,
                               translate_enabled);
    } else if (prefs == translate::prefs::kOfferTranslateEnabled) {
      // Also handle if the user uses chrome://settings to disable translation
      // offers.
      bool translate_enabled =
          pref_service->GetBoolean(translate::prefs::kOfferTranslateEnabled);
      pref_service->SetBoolean(vivaldiprefs::kTranslateEnabled,
                               translate_enabled);
    }
  }

  VivaldiProfileObserver(const VivaldiProfileObserver&) = delete;
  VivaldiProfileObserver& operator=(const VivaldiProfileObserver&) = delete;

  std::unique_ptr<PrefChangeRegistrar> prefs_registrar_;
  const raw_ptr<Profile> profile_;
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
      pref_service->SetBoolean(translate::prefs::kOfferTranslateEnabled, true);
      pref_service->SetBoolean(vivaldiprefs::kTranslateEnabled, true);
    }
  }
}

bool VivaldiValidateProfile(Profile* profile) {
  // Handle locked keystore before side-effects of the loaded profile
  // invalidate passwords. This gets called from the profile initialization
  // code inside chrome before applying side-effects of loaded profile.
  return !vivaldi::HasLockedKeystore(profile);
}

void VivaldiInitProfile(Profile* profile) {
  // Note that this is called for !vivaldi::IsVivaldiRunning() as well. So keep.
  if (vivaldi::IsVivaldiRunning()) {
    adblock_filter::RuleServiceFactory::GetForBrowserContext(profile);
    content_injection::ServiceFactory::GetForBrowserContext(profile);
    page_actions::ServiceFactory::GetForBrowserContext(profile);

    RequestFilterManagerFactory::GetForBrowserContext(profile)->AddFilter(
        std::make_unique<PingBlockerFilter>());

    vivaldi::NotesModel* notes_model =
        vivaldi::NotesModelFactory::GetForBrowserContext(profile);
    // `BookmarkModelLoadedObserver` destroys itself eventually, when loading
    // completes.
    new vivaldi::NotesModelLoadedObserver(profile, notes_model);
  }
  PerformUpdates(profile);

  if (vivaldi::IsVivaldiRunning()) {
    bookmarks::BookmarkModel* bookmarks_model =
        BookmarkModelFactory::GetForBrowserContext(profile);
    if (bookmarks_model)
      vivaldi_partners::RemovedPartnersTracker::Create(profile,
                                                       bookmarks_model);

    // Manages its own lifetime.
    new VivaldiProfileObserver(profile);
    content::URLDataSource::Add(profile,
                                std::make_unique<VivaldiDataSource>(profile));
  }

#if !BUILDFLAG(IS_ANDROID)
  PrefService* pref_service = profile->GetPrefs();

  if (vivaldi::IsVivaldiRunning()) {
    menus::Menu_Model* menu_model =
        menus::MainMenuServiceFactory::GetForBrowserContext(profile);
    menu_model->AddObserver(new menus::MenuModelLoadedObserver());
    // The context menu model content is loaded on demand so no observer here.
    menus::ContextMenuServiceFactory::GetForBrowserContext(profile);
    // Index is loaded on demand.
    sessions::IndexServiceFactory::GetForBrowserContext(profile);

    extensions::VivaldiUtilitiesAPI* utility_api =
        extensions::VivaldiUtilitiesAPI::GetFactoryInstance()->Get(profile);
    if (utility_api) {
      utility_api->PostProfileSetup();
    }
  }
  if (!vivaldi::IsVivaldiRunning())
    return;

  content::URLDataSource::Add(
      profile, std::make_unique<VivaldiThumbDataSource>(profile));
  content::URLDataSource::Add(profile,
                              std::make_unique<VivaldiWebSource>(profile));

  calendar::CalendarService* calendar_service =
      calendar::CalendarServiceFactory::GetForProfile(profile);
  calendar_service->AddObserver(new calendar::CalendarModelLoadedObserver());

  contact::ContactService* contact_service =
      contact::ContactServiceFactory::GetForProfile(profile);
  contact_service->AddObserver(new contact::ContactModelLoadedObserver());

  mail_client::MailClientService* mail_client_service =
      mail_client::MailClientServiceFactory::GetForProfile(profile);
  mail_client_service->AddObserver(new mail_client::MailClientModelObserver());

  direct_match::DirectMatchServiceFactory::GetForBrowserContext(profile);
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
    if (ReleaseKind() >= Release::kBeta) {
      std::string version = ::vivaldi::GetVivaldiVersionString();
      std::vector<std::string> version_array = base::SplitString(
          version, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      if (version_array.size() == 4) {
        int major;
        if (base::StringToInt(version_array[0], &major) && major >= 4) {
          pref_service->SetInteger(
              vivaldiprefs::kStartupHasSeenFeature,
              static_cast<int>(
                  vivaldiprefs::StartupHasSeenFeatureValues::kMail));
          pref_service->SetBoolean(prefs::kHasSeenWelcomePage, false);
        }
      }
    }
  }

#endif
}

}  // namespace vivaldi
