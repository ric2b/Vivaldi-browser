// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTES_NOTE_SYNC_SERVICE_H_
#define SYNC_NOTES_NOTE_SYNC_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/keyed_service/core/keyed_service.h"

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
  NoteSyncService(file_sync::SyncedFileStore* synced_file_store);

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

 private:
  // NoteModelTypeProcessor handles communications between sync engine and
  // NotesModel.
  std::unique_ptr<NoteModelTypeProcessor> note_model_type_processor_;
};

}  // namespace sync_notes

#endif  // SYNC_NOTES_NOTE_SYNC_SERVICE_H_
