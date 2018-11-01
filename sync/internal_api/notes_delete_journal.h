// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_NOTES_DELETE_JOURNAL_H_
#define SYNC_INTERNAL_API_NOTES_DELETE_JOURNAL_H_

#include <set>
#include <vector>

#include "components/sync/base/model_type.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {
class BaseTransaction;
}

using syncer::BaseTransaction;

namespace notessyncer {

struct NotesDeleteJournal {
  int64_t id;           // Metahandle of delete journal entry.
  int64_t external_id;  // Notes ID in the native model.
  bool is_folder;
  sync_pb::EntitySpecifics specifics;
};
typedef std::vector<NotesDeleteJournal> NotesDeleteJournalList;

// Static APIs for passing delete journals between syncer::syncable namspace
// and syncer namespace.
class DeleteJournal {
 public:
  // Return info about deleted notes entries stored in the delete journal
  // of |trans|'s directory.
  // TODO(haitaol): remove this after SyncData supports notes and
  //                all types of delete journals can be returned as
  //                SyncDataList.
  static void GetNotesDeleteJournals(BaseTransaction* trans,
                                     NotesDeleteJournalList* delete_journals);

  // Purge delete journals of given IDs from |trans|'s directory.
  static void PurgeDeleteJournals(BaseTransaction* trans,
                                  const std::set<int64_t>& ids);
};

}  // namespace notessyncer

#endif  // SYNC_INTERNAL_API_NOTES_DELETE_JOURNAL_H_
