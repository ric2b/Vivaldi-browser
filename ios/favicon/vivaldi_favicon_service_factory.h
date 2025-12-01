// Copyright 2025 Vivaldi Technologies. All rights reserved.

#ifndef IOS_FAVICON_VIVALDI_FAVICON_SERVICE_FACTORY_H_
#define IOS_FAVICON_VIVALDI_FAVICON_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace ios {
class VivaldiFaviconService;
}  // namespace ios

class KeyedService;
class ProfileIOS;

namespace ios {
// Singleton that owns all VivaldiFaviconService instances and associates them
// with ProfileIOS objects.
class VivaldiFaviconServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static VivaldiFaviconService* GetForProfile(ProfileIOS* profile);
  static VivaldiFaviconService* GetForProfileIfExists(ProfileIOS* profile);

  static VivaldiFaviconServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<VivaldiFaviconServiceFactory>;

  VivaldiFaviconServiceFactory();
  ~VivaldiFaviconServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};
}  // namespace ios

#endif  // IOS_FAVICON_VIVALDI_FAVICON_SERVICE_FACTORY_H_
