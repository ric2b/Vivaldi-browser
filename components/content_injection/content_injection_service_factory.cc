// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/content_injection/content_injection_service_factory.h"

#include "app/vivaldi_apptools.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_injection/content_injection_service_impl.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace content_injection {

// static
Service* ServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<Service*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ServiceFactory* ServiceFactory::GetInstance() {
  return base::Singleton<ServiceFactory>::get();
}

ServiceFactory::ServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ContentInjectionService",
          BrowserContextDependencyManager::GetInstance()) {}

ServiceFactory::~ServiceFactory() {}

content::BrowserContext* ServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* ServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  ServiceImpl* service = new ServiceImpl(context);
  return service;
}

}  // namespace content_injection
