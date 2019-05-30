// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_MANAGER_FACTORY_H_
#define VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_MANAGER_FACTORY_H_

#include "base/macros.h"
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
  friend struct base::DefaultSingletonTraits<VivaldiAccountManagerFactory>;

  VivaldiAccountManagerFactory();
  ~VivaldiAccountManagerFactory() override;

  // BrowserContextKeyedBaseFactory methods:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(VivaldiAccountManagerFactory);
};

}  // namespace vivaldi

#endif  // VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_MANAGER_FACTORY_H_
