// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/sync/file_store_factory.h"

#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "sync/file_sync/file_store.h"
#include "sync/file_sync/file_store_impl.h"

// static
file_sync::SyncedFileStore* SyncedFileStoreFactory::GetForProfile(
  ProfileIOS* profile) {
  return static_cast<file_sync::SyncedFileStore*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
SyncedFileStoreFactory* SyncedFileStoreFactory::GetInstance() {
  static base::NoDestructor<SyncedFileStoreFactory> instance;
  return instance.get();
}

SyncedFileStoreFactory::SyncedFileStoreFactory()
    : BrowserStateKeyedServiceFactory(
          "SyncedFileStore",
          BrowserStateDependencyManager::GetInstance()) {}

SyncedFileStoreFactory::~SyncedFileStoreFactory() = default;

std::unique_ptr<KeyedService> SyncedFileStoreFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  auto synced_file_store =
      std::make_unique<file_sync::SyncedFileStoreImpl>(context->GetStatePath());
  synced_file_store->Load();
  return synced_file_store;
}

web::BrowserState* SyncedFileStoreFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

bool SyncedFileStoreFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
