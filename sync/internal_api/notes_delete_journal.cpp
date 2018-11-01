// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/notes_delete_journal.h"

#include <set>
#include <string>

#include "components/sync/syncable/base_transaction.h"
#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/syncable_base_transaction.h"

using syncer::NOTES;

namespace notessyncer {

// static
void DeleteJournal::GetNotesDeleteJournals(
    BaseTransaction* trans,
    NotesDeleteJournalList* delete_journal_list) {
  syncer::syncable::EntryKernelSet deleted_entries;
  trans->GetDirectory()->delete_journal()->GetDeleteJournals(
      trans->GetWrappedTrans(), NOTES, &deleted_entries);
  std::set<int64_t> undecryptable_journal;
  for (syncer::syncable::EntryKernelSet::const_iterator i =
           deleted_entries.begin();
       i != deleted_entries.end(); ++i) {
    delete_journal_list->push_back(NotesDeleteJournal());
    delete_journal_list->back().id = (*i)->ref(syncer::syncable::META_HANDLE);
    delete_journal_list->back().external_id =
        (*i)->ref(syncer::syncable::LOCAL_EXTERNAL_ID);
    delete_journal_list->back().is_folder = (*i)->ref(syncer::syncable::IS_DIR);

    const sync_pb::EntitySpecifics& specifics =
        (*i)->ref(syncer::syncable::SPECIFICS);
    if (!specifics.has_encrypted()) {
      delete_journal_list->back().specifics = specifics;
    } else {
      std::string plaintext_data =
          trans->GetCryptographer()->DecryptToString(specifics.encrypted());
      sync_pb::EntitySpecifics unencrypted_data;
      if (plaintext_data.length() == 0 ||
          !unencrypted_data.ParseFromString(plaintext_data)) {
        // Fail to decrypt, Add this delete journal to purge.
        undecryptable_journal.insert(delete_journal_list->back().id);
        delete_journal_list->pop_back();
      } else {
        delete_journal_list->back().specifics = unencrypted_data;
      }
    }
  }

  if (!undecryptable_journal.empty()) {
    trans->GetDirectory()->delete_journal()->PurgeDeleteJournals(
        trans->GetWrappedTrans(), undecryptable_journal);
  }
}

// static
void DeleteJournal::PurgeDeleteJournals(BaseTransaction* trans,
                                        const std::set<int64_t>& ids) {
  trans->GetDirectory()->delete_journal()->PurgeDeleteJournals(
      trans->GetWrappedTrans(), ids);
}

}  // namespace notessyncer
