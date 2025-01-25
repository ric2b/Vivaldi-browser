// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/adverse_adblocking/adverse_ad_filter_list_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/adverse_adblocking/adverse_ad_filter_list.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

AdverseAdFilterListService* VivaldiAdverseAdFilterListFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AdverseAdFilterListService*>(
      GetFactoryInstance()->GetServiceForBrowserContext(profile, true));
}

VivaldiAdverseAdFilterListFactory*
VivaldiAdverseAdFilterListFactory::GetFactoryInstance() {
  return base::Singleton<VivaldiAdverseAdFilterListFactory>::get();
}

VivaldiAdverseAdFilterListFactory::VivaldiAdverseAdFilterListFactory()
    : BrowserContextKeyedServiceFactory(
          "AdverseAdFilterListService",
          BrowserContextDependencyManager::GetInstance()) {}

KeyedService* VivaldiAdverseAdFilterListFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AdverseAdFilterListService(static_cast<Profile*>(context));
}

bool VivaldiAdverseAdFilterListFactory::ServiceIsNULLWhileTesting() const {
  return false;
}

bool VivaldiAdverseAdFilterListFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

content::BrowserContext*
VivaldiAdverseAdFilterListFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetBrowserContextRedirectedInIncognito(context);
}
