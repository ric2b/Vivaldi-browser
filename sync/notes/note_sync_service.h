// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTES_NOTE_SYNC_SERVICE_H_
#define SYNC_NOTES_NOTE_SYNC_SERVICE_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/model/wipe_model_upon_sync_disabled_behavior.h"
#include "sync/notes/note_model_type_processor.h"

namespace syncer {
class ModelTypeControllerDelegate;
}

namespace vivaldi {
class NotesModel;
}

namespace file_sync {
class SyncedFileStore;
}

namespace sync_notes {
class NoteModelTypeProcessor;

// This service owns the NoteModelTypeProcessor.
class NoteSyncService : public KeyedService {
 public:
  NoteSyncService(file_sync::SyncedFileStore* synced_file_store,
                  syncer::WipeModelUponSyncDisabledBehavior
                      wipe_model_upon_sync_disabled_behavior);

  NoteSyncService(const NoteSyncService&) = delete;
  NoteSyncService& operator=(const NoteSyncService&) = delete;

  // KeyedService implemenation.
  ~NoteSyncService() override;

  // Analgous to Encode/Decode methods in NoteClient.
  std::string EncodeNoteSyncMetadata();
  void DecodeNoteSyncMetadata(
      const std::string& metadata_str,
      const base::RepeatingClosure& schedule_save_closure,
      vivaldi::NotesModel* model);

  // Returns the ModelTypeControllerDelegate for syncer::NOTES.
  virtual base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetNoteSyncControllerDelegate();

  // Returns true if sync metadata is being tracked. This means sync is enabled
  // and the initial download of data is completed, which implies that the
  // relevant NotesModel already reflects remote data. Note however that this
  // doesn't mean notes are actively sync-ing at the moment, for example
  // sync could be paused due to an auth error.
  bool IsTrackingMetadata() const;

  // For integration tests.
  void SetNotesLimitForTesting(size_t limit);

 private:
  // NoteModelTypeProcessor handles communications between sync engine and
  // NotesModel.
  NoteModelTypeProcessor note_model_type_processor_;
};

}  // namespace sync_notes

#endif  // SYNC_NOTES_NOTE_SYNC_SERVICE_H_
