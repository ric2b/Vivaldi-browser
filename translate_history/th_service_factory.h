// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved
//

#ifndef TRANSLATE_HISTORY_TH_SERVICE_FACTORY_H_
#define TRANSLATE_HISTORY_TH_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

template <typename T>
struct DefaultSingletonTraits;

namespace translate_history {
class TH_Model;
}

namespace translate_history {

// Singleton that owns all MenuModel entries and associates them with Profiles.
class TH_ServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static TH_Model* GetForBrowserContext(content::BrowserContext* context);

  static TH_Model* GetForBrowserContextIfExists(
      content::BrowserContext* context);

  static TH_ServiceFactory* GetInstance();

  static void ShutdownForProfile(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<TH_ServiceFactory>;

  TH_ServiceFactory();
  ~TH_ServiceFactory() override;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  bool ServiceIsNULLWhileTesting() const override;
};

}  //  namespace translate_history

#endif  // TRANSLATE_HISTORY_TH_SERVICE_FACTORY_H_
