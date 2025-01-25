// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/file_sync/file_store_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "sync/file_sync/file_store.h"
#include "sync/file_sync/file_store_impl.h"

// static
file_sync::SyncedFileStore* SyncedFileStoreFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<file_sync::SyncedFileStore*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
SyncedFileStoreFactory* SyncedFileStoreFactory::GetInstance() {
  static base::NoDestructor<SyncedFileStoreFactory> instance;
  return instance.get();
}

SyncedFileStoreFactory::SyncedFileStoreFactory()
    : BrowserContextKeyedServiceFactory(
          "SyncedFileStore",
          BrowserContextDependencyManager::GetInstance()) {}

SyncedFileStoreFactory::~SyncedFileStoreFactory() {}

KeyedService* SyncedFileStoreFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto synced_file_store =
      std::make_unique<file_sync::SyncedFileStoreImpl>(context->GetPath());
  synced_file_store->Load();
  return synced_file_store.release();
}

content::BrowserContext* SyncedFileStoreFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetBrowserContextRedirectedInIncognito(context);
}

bool SyncedFileStoreFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
