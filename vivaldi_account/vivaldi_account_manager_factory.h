// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_MANAGER_FACTORY_H_
#define VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace vivaldi {

class VivaldiAccountManager;

class VivaldiAccountManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static VivaldiAccountManager* GetForProfile(Profile* profile);
  static VivaldiAccountManagerFactory* GetInstance();

 private:
  friend base::DefaultSingletonTraits<VivaldiAccountManagerFactory>;

  VivaldiAccountManagerFactory();
  ~VivaldiAccountManagerFactory() override;
  VivaldiAccountManagerFactory(const VivaldiAccountManagerFactory&) = delete;
  VivaldiAccountManagerFactory& operator=(const VivaldiAccountManagerFactory&) =
      delete;

  // BrowserContextKeyedBaseFactory methods:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace vivaldi

#endif  // VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_MANAGER_FACTORY_H_
