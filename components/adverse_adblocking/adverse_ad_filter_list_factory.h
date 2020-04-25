// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_ADVERSE_ADBLOCKING_ADVERSE_AD_FILTER_LIST_FACTORY_H_
#define COMPONENTS_ADVERSE_ADBLOCKING_ADVERSE_AD_FILTER_LIST_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class AdverseAdFilterListService;
class Profile;

class VivaldiAdverseAdFilterListFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static AdverseAdFilterListService* GetForProfile(Profile* profile);

  static VivaldiAdverseAdFilterListFactory* GetFactoryInstance();

 private:
  friend struct base::DefaultSingletonTraits<VivaldiAdverseAdFilterListFactory>;

  VivaldiAdverseAdFilterListFactory();

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // COMPONENTS_ADVERSE_ADBLOCKING_ADVERSE_AD_FILTER_LIST_FACTORY_H_
