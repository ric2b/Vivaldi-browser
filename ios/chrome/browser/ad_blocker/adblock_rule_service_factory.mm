// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#import "ios/chrome/browser/ad_blocker/adblock_rule_service_factory.h"

#import "chrome/browser/profiles/incognito_helpers.h"
#import "components/ad_blocker/ios/adblock_rule_service_impl.h"
#import "components/ad_blocker/ios/ios_rules_compiler.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/language/core/browser/pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

#import "vivaldi/prefs/vivaldi_gen_prefs.h"

namespace adblock_filter {

// static
RuleService* RuleServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<RuleService>(profile,
                                                            /*create=*/true);
}

// static
RuleService* RuleServiceFactory::GetForProfileIfExists(ProfileIOS* profile) {
  // Since this is called as part of destroying the browser state, we need this
  // extra test to avoid running into code that tests whether the browser state
  // is still valid.
  if (!GetInstance()->IsServiceCreated(profile)) {
    return nullptr;
  }
  return GetInstance()->GetServiceForProfileAs<RuleService>(profile,
                                                            /*create=*/false);
}

// static
RuleServiceFactory* RuleServiceFactory::GetInstance() {
  static base::NoDestructor<RuleServiceFactory> instance;
  return instance.get();
}

RuleServiceFactory::RuleServiceFactory()
    : ProfileKeyedServiceFactoryIOS("FilterManager",
                                    ProfileSelection::kRedirectedInIncognito) {}

RuleServiceFactory::~RuleServiceFactory() {}

std::unique_ptr<KeyedService> RuleServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  std::string locale =
      local_state->HasPrefPath(language::prefs::kApplicationLocale)
          ? local_state->GetString(language::prefs::kApplicationLocale)
          : GetApplicationContext()->GetApplicationLocaleStorage()->Get();

  PrefService* pref_service =
      ProfileIOS::FromBrowserState(browser_state)->GetPrefs();

  auto rule_service = std::make_unique<RuleServiceImpl>(
      browser_state, pref_service,
      base::BindRepeating(
          &CompileIosRules,
          pref_service->GetBoolean(
              vivaldiprefs::kPrivacyAdBlockerEnableDocumentBlocking)),
      locale);
  rule_service->Load();
  return rule_service;
}

}  // namespace adblock_filter
