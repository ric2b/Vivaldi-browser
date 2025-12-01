// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_MANAGER_FACTORY_H_
#define IOS_VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace vivaldi {

class VivaldiAccountManager;

class VivaldiAccountManagerFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static VivaldiAccountManager* GetForProfile(ProfileIOS* profile);
  static VivaldiAccountManagerFactory* GetInstance();

 private:
  friend base::NoDestructor<VivaldiAccountManagerFactory>;

  VivaldiAccountManagerFactory();
  ~VivaldiAccountManagerFactory() override;

  // ProfileKeyedServiceFactoryIOS methods:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace vivaldi

#endif  // IOS_VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_MANAGER_FACTORY_H_
