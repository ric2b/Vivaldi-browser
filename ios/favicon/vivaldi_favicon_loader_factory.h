// Copyright 2025 Vivaldi Technologies. All rights reserved.

#ifndef IOS_FAVICON_VIVALDI_FAVICON_LOADER_FACTORY_H_
#define IOS_FAVICON_VIVALDI_FAVICON_LOADER_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace ios {
class VivaldiFaviconLoader;
}  // namespace ios

class FaviconLoader;
class KeyedService;
class ProfileIOS;

namespace ios {
// Factory that creates VivaldiFaviconLoader instances that can be used as
// replacements for the chromium FaviconLoader.
class VivaldiFaviconLoaderFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static VivaldiFaviconLoader* GetForProfile(ProfileIOS* profile);
  static VivaldiFaviconLoader* GetForProfileIfExists(ProfileIOS* profile);

  static VivaldiFaviconLoaderFactory* GetInstance();

 private:
  friend class base::NoDestructor<VivaldiFaviconLoaderFactory>;

  VivaldiFaviconLoaderFactory();
  ~VivaldiFaviconLoaderFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios

#endif  // IOS_FAVICON_VIVALDI_FAVICON_LOADER_FACTORY_H_
