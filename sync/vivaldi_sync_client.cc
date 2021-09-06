// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_sync_client.h"

#include "app/vivaldi_apptools.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "content/public/browser/browser_thread.h"
#include "notes/notes_factory.h"
#include "sync/invalidation/vivaldi_invalidation_service_factory.h"

namespace vivaldi {
VivaldiSyncClient::VivaldiSyncClient(Profile* profile)
    : browser_sync::ChromeSyncClient(profile), profile_(profile) {}

VivaldiSyncClient::~VivaldiSyncClient() {}

invalidation::InvalidationService* VivaldiSyncClient::GetInvalidationService() {
  if (vivaldi::ForcedVivaldiRunning()) {
    invalidation::ProfileInvalidationProvider* provider =
        invalidation::ProfileInvalidationProviderFactory::GetForProfile(
            profile_);

    if (provider)
      return provider->GetInvalidationService();
    return nullptr;
  }
  return VivaldiInvalidationServiceFactory::GetForProfile(profile_);
}

}  // namespace vivaldi
