// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_PAGE_ACTIONS_PAGE_ACTIONS_SERVICE_FACTORY_H_
#define COMPONENTS_PAGE_ACTIONS_PAGE_ACTIONS_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace page_actions {

class Service;

class ServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static Service* GetForBrowserContext(content::BrowserContext* context);
  static Service* GetForBrowserContextIfExists(
      content::BrowserContext* context);
  static ServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ServiceFactory>;

  ServiceFactory();
  ~ServiceFactory() override;
  ServiceFactory(const ServiceFactory&) = delete;
  ServiceFactory& operator=(const ServiceFactory&) = delete;

  // BrowserContextKeyedBaseFactory methods:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace page_actions

#endif  // COMPONENTS_PAGE_ACTIONS_PAGE_ACTIONS_SERVICE_FACTORY_H_
