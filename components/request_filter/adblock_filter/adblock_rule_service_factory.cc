// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/adblock_rule_service_factory.h"

#include "app/vivaldi_apptools.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_impl.h"
#include "components/request_filter/adblock_filter/flat_rules_compiler.h"

namespace adblock_filter {

// static
RuleServiceContent* RuleServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<RuleServiceContent*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
RuleServiceFactory* RuleServiceFactory::GetInstance() {
  return base::Singleton<RuleServiceFactory>::get();
}

RuleServiceFactory::RuleServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "FilterManager",
          BrowserContextDependencyManager::GetInstance()) {}

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
          context, Profile::FromBrowserContext(context)->GetPrefs(),
          base::BindRepeating(&CompileFlatRules), locale);
  // Avoid actually loading the service during unit tests.
  if (vivaldi::IsVivaldiRunning())
    rule_service->Load();
  return rule_service;
}

}  // namespace adblock_filter
