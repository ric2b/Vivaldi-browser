// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "ui/lazy_load_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "ui/lazy_load_service.h"

namespace vivaldi {

// static
LazyLoadService* LazyLoadServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<LazyLoadService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
LazyLoadServiceFactory* LazyLoadServiceFactory::GetInstance() {
  return base::Singleton<LazyLoadServiceFactory>::get();
}

LazyLoadServiceFactory::LazyLoadServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "LazyLoadService",
          BrowserContextDependencyManager::GetInstance()) {}

LazyLoadServiceFactory::~LazyLoadServiceFactory() {}

KeyedService* LazyLoadServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new LazyLoadService(Profile::FromBrowserContext(context));
}

}  // namespace vivaldi
