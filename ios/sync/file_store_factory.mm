// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/sync/file_store_factory.h"

#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "sync/file_sync/file_store.h"
#include "sync/file_sync/file_store_impl.h"

// static
file_sync::SyncedFileStore* SyncedFileStoreFactory::GetForProfile(
  ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<file_sync::SyncedFileStore>(
          profile, /*create=*/true);
}

// static
SyncedFileStoreFactory* SyncedFileStoreFactory::GetInstance() {
  static base::NoDestructor<SyncedFileStoreFactory> instance;
  return instance.get();
}

SyncedFileStoreFactory::SyncedFileStoreFactory()
    : ProfileKeyedServiceFactoryIOS("SyncedFileStore",
                                    ProfileSelection::kRedirectedInIncognito,
                                    TestingCreation::kNoServiceForTests) {}

SyncedFileStoreFactory::~SyncedFileStoreFactory() = default;

std::unique_ptr<KeyedService> SyncedFileStoreFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  auto synced_file_store =
      std::make_unique<file_sync::SyncedFileStoreImpl>(context->GetStatePath());
  synced_file_store->Load();
  return synced_file_store;
}
