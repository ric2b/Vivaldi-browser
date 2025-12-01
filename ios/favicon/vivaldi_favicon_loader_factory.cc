// Copyright 2025 Vivaldi Technologies. All rights reserved.

#include "ios/favicon/vivaldi_favicon_loader_factory.h"

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/favicon/vivaldi_favicon_loader.h"
#include "ios/favicon/vivaldi_favicon_service_factory.h"

namespace ios {

// Creates a VivaldiFaviconLoader instance with dependency injection.
std::unique_ptr<KeyedService> BuildVivaldiFaviconLoader(
    web::BrowserState* context) {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  VivaldiFaviconService* service =
      VivaldiFaviconServiceFactory::GetForProfile(profile);

  if (!service) {
    return nullptr;
  }
  return std::make_unique<ios::VivaldiFaviconLoader>(service);
}

// static
VivaldiFaviconLoader* VivaldiFaviconLoaderFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<VivaldiFaviconLoader>(
      profile, /*create=*/true);
}

// static
VivaldiFaviconLoader* VivaldiFaviconLoaderFactory::GetForProfileIfExists(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<VivaldiFaviconLoader>(
      profile, /*create=*/false);
}

// static
VivaldiFaviconLoaderFactory* VivaldiFaviconLoaderFactory::GetInstance() {
  static base::NoDestructor<VivaldiFaviconLoaderFactory> instance;
  return instance.get();
}

VivaldiFaviconLoaderFactory::VivaldiFaviconLoaderFactory()
    : ProfileKeyedServiceFactoryIOS("VivaldiFaviconLoader",
                                    ProfileSelection::kOwnInstanceInIncognito,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(VivaldiFaviconServiceFactory::GetInstance());
}

VivaldiFaviconLoaderFactory::~VivaldiFaviconLoaderFactory() = default;

std::unique_ptr<KeyedService>
VivaldiFaviconLoaderFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildVivaldiFaviconLoader(context);
}

}  // namespace ios
