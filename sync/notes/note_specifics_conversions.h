// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTES_NOTE_SPECIFICS_CONVERSIONS_H_
#define SYNC_NOTES_NOTE_SPECIFICS_CONVERSIONS_H_

#include <stddef.h>

#include <string>

namespace vivaldi {
class NotesModel;
class NoteNode;
}  // namespace vivaldi

namespace sync_pb {
class NotesSpecifics;
class EntitySpecifics;
}  // namespace sync_pb

namespace syncer {
struct EntityData;
}  // namespace syncer

namespace sync_notes {

// Canonicalize |node_title| similar to legacy client's implementation by
// truncating and the appending ' ' in some cases.
std::string FullTitleToLegacyCanonicalizedTitle(const std::string& node_title);

// Used to decide if entity needs to be reuploaded for each remote change.
bool IsNoteEntityReuploadNeeded(const syncer::EntityData& remote_entity_data);

// TODO(crbug.com/978430): Remove argument |include_guid| once the client tag
// hash is required to be populated during sync metadata validation upon
// startup in SyncedNotesTracker::NotesModelMatchesMetadata().
sync_pb::EntitySpecifics CreateSpecificsFromNoteNode(
    const vivaldi::NoteNode* node,
    vivaldi::NotesModel* mode,
    bool include_guid);

// Creates a note node under the given parent node from the given specifics.
// Returns the newly created node. Callers must verify that
// |specifics| passes the IsValidNotesSpecifics().
const vivaldi::NoteNode* CreateNoteNodeFromSpecifics(
    const sync_pb::NotesSpecifics& specifics,
    const vivaldi::NoteNode* parent,
    size_t index,
    bool is_folder,
    vivaldi::NotesModel* model);

// Updates the note node |node| with the data in |specifics|. Callers must
// verify that |specifics| passes the IsValidNotesSpecifics().
void UpdateNoteNodeFromSpecifics(const sync_pb::NotesSpecifics& specifics,
                                 const vivaldi::NoteNode* node,
                                 vivaldi::NotesModel* model);

// Replaces |node| with a NoteNode of equal properties and original node
// creation timestamp but a different GUID, set to |guid|, which must be a
// valid version 4 GUID. Intended to be used in cases where the GUID must be
// modified despite being immutable within the NoteNode itself. Returns
// the newly created node, and the original node gets deleted.
const vivaldi::NoteNode* ReplaceNoteNodeGUID(const vivaldi::NoteNode* node,
                                             const std::string& guid,
                                             vivaldi::NotesModel* model);

// Checks if a note specifics represents a valid note. |is_folder| is
// whether this specifics is for a folder. Valid specifics must not be empty,
// non-folders must contains a valid url, and all keys in the meta_info must be
// unique.
bool IsValidNotesSpecifics(const sync_pb::NotesSpecifics& specifics,
                           bool is_folder);

// Checks if note specifics contain a GUID that matches the value that would
// be inferred from other redundant fields. |specifics| must be valid as per
// IsValidNotesSpecifics().
// TODO(crbug.com/1032052): Replace this with an analogous function that
// verifies that the note's client tag hash matches the GUID.
bool HasExpectedNoteGuid(const sync_pb::NotesSpecifics& specifics,
                         const std::string& originator_cache_guid,
                         const std::string& originator_client_item_id);

}  // namespace sync_notes

#endif  // SYNC_NOTES_NOTE_SPECIFICS_CONVERSIONS_H_
