// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_REQUEST_FILTER_MANAGER_FACTORY_H_
#define COMPONENTS_REQUEST_FILTER_REQUEST_FILTER_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace vivaldi {

class RequestFilterManager;

class RequestFilterManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static RequestFilterManager* GetForBrowserContext(
      content::BrowserContext* context);
  static RequestFilterManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<RequestFilterManagerFactory>;

  RequestFilterManagerFactory();

  RequestFilterManagerFactory(const RequestFilterManagerFactory&) = delete;
  RequestFilterManagerFactory& operator=(const RequestFilterManagerFactory&) =
      delete;

  ~RequestFilterManagerFactory() override;

  // BrowserContextKeyedBaseFactory methods:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace vivaldi

#endif  // COMPONENTS_REQUEST_FILTER_REQUEST_FILTER_MANAGER_FACTORY_H_
