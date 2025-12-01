// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#import "ios/translate/vivaldi_ios_th_service_factory.h"

#import "components/history/core/common/pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "translate_history/th_model.h"

namespace translate_history {

// static
TH_Model* VivaldiIOSTHServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<TH_Model>(
      profile, /*create=*/true);
}

// static
TH_Model* VivaldiIOSTHServiceFactory::GetForProfileIfExists(
   ProfileIOS* profile) {
  // Since this is called as part of destroying the browser state, we need this
  // extra test to avoid running into code that tests whether the browser state
  // is still valid.
  if (!GetInstance()->IsServiceCreated(profile)) {
    return nullptr;
  }
  return GetInstance()->GetServiceForProfileAs<TH_Model>(
      profile, /*create=*/false);
}

// static
VivaldiIOSTHServiceFactory* VivaldiIOSTHServiceFactory::GetInstance() {
  static base::NoDestructor<VivaldiIOSTHServiceFactory> instance;
  return instance.get();
}

VivaldiIOSTHServiceFactory::VivaldiIOSTHServiceFactory()
    : ProfileKeyedServiceFactoryIOS("TranslateHistoryService",
                                    ProfileSelection::kRedirectedInIncognito) {}

VivaldiIOSTHServiceFactory::~VivaldiIOSTHServiceFactory() {}

std::unique_ptr<KeyedService>
    VivaldiIOSTHServiceFactory::BuildServiceInstanceFor(
  web::BrowserState* browser_state) const {
    ProfileIOS* profile = ProfileIOS::FromBrowserState(browser_state);
    PrefService* pref_service = profile->GetOriginalProfile()->GetPrefs();
    bool session_only =
        pref_service->GetBoolean(prefs::kSavingBrowserHistoryDisabled);
    auto th_model = std::make_unique<TH_Model>(session_only);
    return th_model;
}

}  // namespace translate_history
