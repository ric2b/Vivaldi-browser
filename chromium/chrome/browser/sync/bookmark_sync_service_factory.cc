// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/bookmark_sync_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"

#include "sync/file_sync/file_store_factory.h"

// static
sync_bookmarks::BookmarkSyncService* BookmarkSyncServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<sync_bookmarks::BookmarkSyncService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
BookmarkSyncServiceFactory* BookmarkSyncServiceFactory::GetInstance() {
  return base::Singleton<BookmarkSyncServiceFactory>::get();
}

BookmarkSyncServiceFactory::BookmarkSyncServiceFactory()
    : ProfileKeyedServiceFactory(
          "BookmarkSyncServiceFactory",
          ProfileSelections::BuildRedirectedInIncognito()) {
  DependsOn(BookmarkUndoServiceFactory::GetInstance());
  DependsOn(SyncedFileStoreFactory::GetInstance());
}

BookmarkSyncServiceFactory::~BookmarkSyncServiceFactory() = default;

KeyedService* BookmarkSyncServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  auto* sync_service = new sync_bookmarks::BookmarkSyncService(
      BookmarkUndoServiceFactory::GetForProfileIfExists(profile));
  sync_service->SetVivaldiSyncedFileStore(
      SyncedFileStoreFactory::GetForBrowserContext(context));
  return sync_service;
}
