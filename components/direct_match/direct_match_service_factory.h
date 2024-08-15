// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef DIRECT_MATCH_SERVICE_FACTORY_H_
#define DIRECT_MATCH_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace direct_match {

class DirectMatchService;

class DirectMatchServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static DirectMatchService* GetForBrowserContext(
      content::BrowserContext* context);
  static DirectMatchService* GetForBrowserContextIfExists(
      content::BrowserContext* context);
  static DirectMatchServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<DirectMatchServiceFactory>;

  DirectMatchServiceFactory();
  ~DirectMatchServiceFactory() override;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace direct_match

#endif  // DIRECT_MATCH_SERVICE_FACTORY_H_
