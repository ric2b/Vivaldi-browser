// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_sync_client.h"

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "notes/notes_factory.h"
#include "sync/vivaldi_invalidation_service.h"

namespace vivaldi {
VivaldiSyncClient::VivaldiSyncClient(Profile* profile)
    : browser_sync::ChromeSyncClient(profile),
      invalidation_service_(new VivaldiInvalidationService(profile)) {}

VivaldiSyncClient::~VivaldiSyncClient() {}

invalidation::InvalidationService* VivaldiSyncClient::GetInvalidationService() {
  return invalidation_service_.get();
}

}  // namespace vivaldi

namespace browser_sync {

vivaldi::Notes_Model* ChromeSyncClient::GetNotesModel() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return vivaldi::NotesModelFactory::GetForBrowserContext(profile_);
}

}  // namespace browser_sync
