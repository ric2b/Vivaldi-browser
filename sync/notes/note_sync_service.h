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
#include "sync/notes/note_data_type_processor.h"
#include "sync/notes/note_model_view.h"

namespace syncer {
class DataTypeControllerDelegate;
}

namespace vivaldi {
class NotesModel;
}

namespace file_sync {
class SyncedFileStore;
}

namespace sync_notes {
class NoteDataTypeProcessor;

// This service owns the NoteDataTypeProcessor.
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
      std::unique_ptr<sync_notes::NoteModelView> model);

  // Returns the DataTypeControllerDelegate for syncer::NOTES.
  virtual base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetNoteSyncControllerDelegate();

  // Returns true if sync metadata is being tracked. This means sync is enabled
  // and the initial download of data is completed, which implies that the
  // relevant NotesModel already reflects remote data. Note however that this
  // doesn't mean notes are actively sync-ing at the moment, for example
  // sync could be paused due to an auth error.
  bool IsTrackingMetadata() const;

  // Returns the NoteModelView representing the subset of notes that this
  // service is dealing with (potentially sync-ing, but not necessarily). It
  // returns null until notes are loaded, i.e. until DecodeNoteSyncMetadata() is
  // invoked. It must not be invoked after Shutdown(), i.e. during profile
  // destruction.
  sync_notes::NoteModelView* note_model_view();

  // For integration tests.
  void SetIsTrackingMetadataForTesting();
  void SetNotesLimitForTesting(size_t limit);

 private:
  std::unique_ptr<NoteModelView> note_model_view_;
  // NoteDataTypeProcessor handles communications between sync engine and
  // NotesModel.
  NoteDataTypeProcessor note_data_type_processor_;
  bool is_tracking_metadata_for_testing_ = false;
};

}  // namespace sync_notes

#endif  // SYNC_NOTES_NOTE_SYNC_SERVICE_H_
