// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#include "ios/sync/note_sync_service_factory.h"

#include "components/sync/model/wipe_model_upon_sync_disabled_behavior.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/sync/file_store_factory.h"
#include "sync/notes/note_sync_service.h"

namespace vivaldi {

// static
sync_notes::NoteSyncService* NoteSyncServiceFactory::GetForProfile(
   ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<sync_notes::NoteSyncService>(
      profile, /*create=*/true);
}

// static
NoteSyncServiceFactory* NoteSyncServiceFactory::GetInstance() {
  static base::NoDestructor<NoteSyncServiceFactory> instance;
  return instance.get();
}

NoteSyncServiceFactory::NoteSyncServiceFactory()
    : ProfileKeyedServiceFactoryIOS("NoteSyncServiceFactory",
                                    ProfileSelection::kRedirectedInIncognito) {
  DependsOn(SyncedFileStoreFactory::GetInstance());
}

NoteSyncServiceFactory::~NoteSyncServiceFactory() {}

std::unique_ptr<KeyedService> NoteSyncServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  auto note_sync_service = std::make_unique<sync_notes::NoteSyncService>(
      SyncedFileStoreFactory::GetForProfile(profile),
      syncer::WipeModelUponSyncDisabledBehavior::kNever);
  return note_sync_service;
}

}  // namespace vivaldi
