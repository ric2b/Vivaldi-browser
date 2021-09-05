// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTES_NOTE_LOCAL_CHANGES_BUILDER_H_
#define SYNC_NOTES_NOTE_LOCAL_CHANGES_BUILDER_H_

#include <vector>

#include "components/sync/engine/commit_and_get_updates_types.h"

namespace vivaldi {
class NotesModel;
}

namespace sync_notes {

class SyncedNoteTracker;

class NoteLocalChangesBuilder {
 public:
  // |note_tracker| and |notes_model| must not be null and must outlive
  // this object.
  NoteLocalChangesBuilder(SyncedNoteTracker* note_tracker,
                          vivaldi::NotesModel* notes_model);
  // Builds the commit requests list.
  syncer::CommitRequestDataList BuildCommitRequests(size_t max_entries) const;

 private:
  SyncedNoteTracker* const note_tracker_;
  vivaldi::NotesModel* const notes_model_;

  DISALLOW_COPY_AND_ASSIGN(NoteLocalChangesBuilder);
};

}  // namespace sync_notes

#endif  // SYNC_NOTES_NOTE_LOCAL_CHANGES_BUILDER_H_
