// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#include "ui/window_registry_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "ui/window_registry_service.h"

namespace vivaldi {

// static
WindowRegistryService* WindowRegistryServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<WindowRegistryService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
WindowRegistryServiceFactory* WindowRegistryServiceFactory::GetInstance() {
  return base::Singleton<WindowRegistryServiceFactory>::get();
}

WindowRegistryServiceFactory::WindowRegistryServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "WindowRegistryService",
          BrowserContextDependencyManager::GetInstance()) {}

WindowRegistryServiceFactory::~WindowRegistryServiceFactory() {}

std::unique_ptr<KeyedService>
WindowRegistryServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<WindowRegistryService>();
}

content::BrowserContext*
WindowRegistryServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Make sure the service exist in incognito mode.
  return context;
}

}  // namespace vivaldi
