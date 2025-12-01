// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "browser/ad_blocker/adblock_rule_service_factory.h"

#include "app/vivaldi_apptools.h"
#include "browser/ad_blocker/adblock_rule_service_client.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/ad_blocker/content/adblock_rule_service_impl.h"
#include "components/ad_blocker/content/index/flat_rules_compiler.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/request_filter/request_filter_manager.h"
#include "components/request_filter/request_filter_manager_factory.h"

namespace adblock_filter {

// static
RuleService* RuleServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<RuleService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
RuleServiceFactory* RuleServiceFactory::GetInstance() {
  return base::Singleton<RuleServiceFactory>::get();
}

RuleServiceFactory::RuleServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "AdBlocker",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(vivaldi::RequestFilterManagerFactory::GetInstance());
}

RuleServiceFactory::~RuleServiceFactory() {}

content::BrowserContext* RuleServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetBrowserContextRedirectedInIncognito(context);
}

std::unique_ptr<KeyedService>
RuleServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  PrefService* pref_service = g_browser_process->local_state();
  // Test browser-process does not have prefs.
  if (!pref_service)
    return nullptr;

  std::string locale =
      pref_service->HasPrefPath(language::prefs::kApplicationLocale)
          ? pref_service->GetString(language::prefs::kApplicationLocale)
          : g_browser_process->GetApplicationLocale();

  std::unique_ptr<RuleServiceImpl> rule_service =
      std::make_unique<RuleServiceImpl>(
          std::make_unique<vivaldi::AdblockRuleServiceClient>(), context,
          base::BindRepeating(&CompileFlatRules), locale);
  // Avoid actually loading the service during unit tests.
  if (vivaldi::IsVivaldiRunning())
    rule_service->Load(
        vivaldi::RequestFilterManagerFactory::GetForBrowserContext(context),
        Profile::FromBrowserContext(context)->GetPrefs());
  return rule_service;
}

}  // namespace adblock_filter
