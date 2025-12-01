// Copyright 2025 Vivaldi Technologies. All rights reserved.

#include "ios/favicon/vivaldi_favicon_service_factory.h"

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/image_fetcher/ios/ios_image_decoder_impl.h"
#include "components/keyed_service/core/service_access_type.h"
#include "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#include "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/favicon/vivaldi_favicon_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ios {

// Creates a VivaldiFaviconService instance with dependency injection.
std::unique_ptr<KeyedService> BuildVivaldiFaviconService(ProfileIOS* profile) {
  // Get required dependencies from their respective factories.
  favicon::LargeIconService* large_icon_service =
      IOSChromeLargeIconServiceFactory::GetForProfile(profile);

  favicon::FaviconService* favicon_service =
      ios::FaviconServiceFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS);

  // Create image fetcher for network requests.
  // Note: The service will take ownership of the image_fetcher through
  // unique_ptr
  std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher =
      std::make_unique<image_fetcher::ImageFetcherImpl>(
          image_fetcher::CreateIOSImageDecoder(),
          profile->GetSharedURLLoaderFactory());

  if (!large_icon_service || !favicon_service || !image_fetcher) {
    return nullptr;
  }

  return std::make_unique<ios::VivaldiFaviconService>(
      favicon_service, large_icon_service, std::move(image_fetcher));
}

// static
VivaldiFaviconService* VivaldiFaviconServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<ios::VivaldiFaviconService>(
      profile, /*create=*/true);
}

// static
VivaldiFaviconService* VivaldiFaviconServiceFactory::GetForProfileIfExists(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<ios::VivaldiFaviconService>(
      profile, /*create=*/false);
}

// static
VivaldiFaviconServiceFactory* VivaldiFaviconServiceFactory::GetInstance() {
  static base::NoDestructor<VivaldiFaviconServiceFactory> instance;
  return instance.get();
}

VivaldiFaviconServiceFactory::VivaldiFaviconServiceFactory()
    : ProfileKeyedServiceFactoryIOS("VivaldiFaviconService",
                                    ProfileSelection::kOwnInstanceInIncognito,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(IOSChromeLargeIconServiceFactory::GetInstance());
  DependsOn(ios::FaviconServiceFactory::GetInstance());
}

VivaldiFaviconServiceFactory::~VivaldiFaviconServiceFactory() = default;

std::unique_ptr<KeyedService>
VivaldiFaviconServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildVivaldiFaviconService(profile);
}

}  // namespace ios
