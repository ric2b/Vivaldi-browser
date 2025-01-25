// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/note_sync_service_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sync/model/wipe_model_upon_sync_disabled_behavior.h"
#include "sync/file_sync/file_store.h"
#include "sync/file_sync/file_store_factory.h"
#include "sync/notes/note_sync_service.h"

namespace vivaldi {
// static
sync_notes::NoteSyncService* NoteSyncServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<sync_notes::NoteSyncService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
NoteSyncServiceFactory* NoteSyncServiceFactory::GetInstance() {
  return base::Singleton<NoteSyncServiceFactory>::get();
}

NoteSyncServiceFactory::NoteSyncServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "NoteSyncServiceFactory",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(SyncedFileStoreFactory::GetInstance());
}

NoteSyncServiceFactory::~NoteSyncServiceFactory() {}

KeyedService* NoteSyncServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new sync_notes::NoteSyncService(
      SyncedFileStoreFactory::GetForBrowserContext(context),
      syncer::WipeModelUponSyncDisabledBehavior::kNever);
}

content::BrowserContext* NoteSyncServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetBrowserContextRedirectedInIncognito(context);
}
}  // namespace vivaldi
