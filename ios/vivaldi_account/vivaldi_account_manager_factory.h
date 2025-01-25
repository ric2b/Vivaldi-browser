// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_MANAGER_FACTORY_H_
#define IOS_VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ProfileIOS;

namespace vivaldi {

class VivaldiAccountManager;

class VivaldiAccountManagerFactory : public BrowserStateKeyedServiceFactory {
 public:
  static VivaldiAccountManager* GetForProfile(ProfileIOS* profile);
  static VivaldiAccountManagerFactory* GetInstance();

 private:
  friend base::NoDestructor<VivaldiAccountManagerFactory>;

  VivaldiAccountManagerFactory();
  ~VivaldiAccountManagerFactory() override;

  // BrowserContextKeyedBaseFactory methods:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace vivaldi

#endif  // IOS_VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_MANAGER_FACTORY_H_
