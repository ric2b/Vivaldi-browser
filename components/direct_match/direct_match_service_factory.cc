// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include "direct_match_service_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

#include "direct_match_service.h"

namespace direct_match {

// static
DirectMatchService* DirectMatchServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<DirectMatchService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
DirectMatchService* DirectMatchServiceFactory::GetForBrowserContextIfExists(
    content::BrowserContext* context) {
  return static_cast<DirectMatchService*>(
      GetInstance()->GetServiceForBrowserContext(context, false));
}

// static
DirectMatchServiceFactory* DirectMatchServiceFactory::GetInstance() {
  return base::Singleton<DirectMatchServiceFactory>::get();
}

DirectMatchServiceFactory::DirectMatchServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "DirectMatchService",
          BrowserContextDependencyManager::GetInstance()) {}

DirectMatchServiceFactory::~DirectMatchServiceFactory() {}

content::BrowserContext* DirectMatchServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* DirectMatchServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  auto dm_service = new DirectMatchService();
  dm_service->Load(profile);
  return dm_service;
}

bool DirectMatchServiceFactory::ServiceIsNULLWhileTesting() const {
  return false;
}

}  // namespace direct_match
