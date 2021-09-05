// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/adblock_rule_service_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_impl.h"

#include "app/vivaldi_apptools.h"

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
          "FilterManager",
          BrowserContextDependencyManager::GetInstance()) {}

RuleServiceFactory::~RuleServiceFactory() {}

content::BrowserContext* RuleServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* RuleServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  RuleServiceImpl* rule_service = new RuleServiceImpl(context);
  // Avoid actually loading the service during unit tests.
  if (vivaldi::IsVivaldiRunning())
    rule_service->Load(
        Profile::FromBrowserContext(context)->GetIOTaskRunner().get());
  return rule_service;
}

}  // namespace adblock_filter
