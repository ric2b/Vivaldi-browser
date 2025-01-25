// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/request_filter_manager_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/request_filter/request_filter_manager.h"

namespace vivaldi {

// static
RequestFilterManager* RequestFilterManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<RequestFilterManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
RequestFilterManagerFactory* RequestFilterManagerFactory::GetInstance() {
  return base::Singleton<RequestFilterManagerFactory>::get();
}

RequestFilterManagerFactory::RequestFilterManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "FilterManager",
          BrowserContextDependencyManager::GetInstance()) {}

RequestFilterManagerFactory::~RequestFilterManagerFactory() {}

content::BrowserContext* RequestFilterManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* RequestFilterManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new RequestFilterManager(context);
}

}  // namespace vivaldi
