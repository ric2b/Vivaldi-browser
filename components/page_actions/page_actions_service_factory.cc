// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/page_actions/page_actions_service_factory.h"

#include "app/vivaldi_apptools.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_injection/content_injection_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/page_actions/page_actions_service_impl.h"

namespace page_actions {

// static
Service* ServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<Service*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
Service* ServiceFactory::GetForBrowserContextIfExists(
    content::BrowserContext* context) {
  return static_cast<Service*>(
      GetInstance()->GetServiceForBrowserContext(context, false));
}

// static
ServiceFactory* ServiceFactory::GetInstance() {
  return base::Singleton<ServiceFactory>::get();
}

ServiceFactory::ServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PageActionsService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(content_injection::ServiceFactory::GetInstance());
}

ServiceFactory::~ServiceFactory() {}

content::BrowserContext* ServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* ServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  ServiceImpl* service = new ServiceImpl(context);
  service->Load();
  return service;
}

}  // namespace page_actions
