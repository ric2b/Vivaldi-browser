// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#ifndef UI_WINDOW_REGISTRY_SERVICE_FACTORY_H_
#define UI_WINDOW_REGISTRY_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace vivaldi {

class WindowRegistryService;

class WindowRegistryServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static WindowRegistryService* GetForProfile(Profile* profile);
  static WindowRegistryServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<WindowRegistryServiceFactory>;

  WindowRegistryServiceFactory();
  ~WindowRegistryServiceFactory() override;
  WindowRegistryServiceFactory(const WindowRegistryServiceFactory&) = delete;
  WindowRegistryServiceFactory& operator=(const WindowRegistryServiceFactory&) =
      delete;

  // BrowserContextKeyedBaseFactory methods:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace vivaldi

#endif  // UI_WINDOW_REGISTRY_SERVICE_FACTORY_H_
