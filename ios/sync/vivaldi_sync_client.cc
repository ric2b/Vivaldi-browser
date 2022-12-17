// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "ios/sync/vivaldi_sync_client.h"

#include "app/vivaldi_apptools.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "ios/chrome/browser/invalidation/ios_chrome_profile_invalidation_provider_factory.h"
#include "ios/sync/vivaldi_invalidation_service_factory.h"

namespace vivaldi {
VivaldiSyncClient::VivaldiSyncClient(ChromeBrowserState* context)
    : IOSChromeSyncClient(context), context_(context) {}

VivaldiSyncClient::~VivaldiSyncClient() {}

invalidation::InvalidationService* VivaldiSyncClient::GetInvalidationService() {
  if (vivaldi::ForcedVivaldiRunning()) {
    invalidation::ProfileInvalidationProvider* provider =
        IOSChromeProfileInvalidationProviderFactory::GetForBrowserState(
            context_);

    if (provider)
      return provider->GetInvalidationService();
    return nullptr;
  }
  return VivaldiInvalidationServiceFactory::GetForBrowserState(context_);
}

}  // namespace vivaldi
