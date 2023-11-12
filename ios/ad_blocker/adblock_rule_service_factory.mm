// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#import "ios/ad_blocker/adblock_rule_service_factory.h"

#import "chrome/browser/profiles/incognito_helpers.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/language/core/browser/pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/ad_blocker/adblock_rule_service_impl.h"
#import "ios/ad_blocker/ios_rules_compiler.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

namespace adblock_filter {

// static
RuleService* RuleServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<RuleService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
RuleServiceFactory* RuleServiceFactory::GetInstance() {
  static base::NoDestructor<RuleServiceFactory> instance;
  return instance.get();
}

RuleServiceFactory::RuleServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "FilterManager",
          BrowserStateDependencyManager::GetInstance()) {}

RuleServiceFactory::~RuleServiceFactory() {}

web::BrowserState* RuleServiceFactory::GetBrowserStateToUse(
    web::BrowserState* browser_state) const {
  return GetBrowserStateRedirectedInIncognito(browser_state);
}

std::unique_ptr<KeyedService> RuleServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  PrefService* pref_service = GetApplicationContext()->GetLocalState();
  std::string locale =
      pref_service->HasPrefPath(language::prefs::kApplicationLocale)
          ? pref_service->GetString(language::prefs::kApplicationLocale)
          : GetApplicationContext()->GetApplicationLocale();

  auto rule_service = std::make_unique<RuleServiceImpl>(
      browser_state, base::BindRepeating(&CompileIosRules), locale);
  rule_service->Load();
  return rule_service;
}

}  // namespace adblock_filter
