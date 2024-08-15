// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOTES_NOTE_LOAD_DETAILS_H_
#define COMPONENTS_NOTES_NOTE_LOAD_DETAILS_H_

#include <memory>
#include <string>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "components/notes/note_node.h"

namespace vivaldi {

// NoteLoadDetails is used by NotesStorage when loading notes.
// NotesModel creates a NoteLoadDetails and passes it (including
// ownership) to NotesStorage. NotesStorage loads the notes (and
// index) in the background thread, then calls back to the NotesModel (on
// the main thread) when loading is done, passing ownership back to the
// NotesModel. While loading NotesModel does not maintain references to
// the contents of the NoteLoadDetails, this ensures we don't have any
// threading problems.
class NoteLoadDetails {
 public:
  NoteLoadDetails();
  ~NoteLoadDetails();

  std::unique_ptr<NoteNode> release_root() { return std::move(root_node_); }

  PermanentNoteNode* main_notes_node() { return main_notes_node_; }
  PermanentNoteNode* other_notes_node() { return other_notes_node_; }
  PermanentNoteNode* trash_notes_node() { return trash_notes_node_; }

  // Computed checksum.
  void set_computed_checksum(const std::string& value) {
    computed_checksum_ = value;
  }
  const std::string& computed_checksum() const { return computed_checksum_; }

  // Stored checksum.
  void set_stored_checksum(const std::string& value) {
    stored_checksum_ = value;
  }
  const std::string& stored_checksum() const { return stored_checksum_; }

  int64_t max_id() const { return max_id_; }
  void set_max_id(int64_t max_id) { max_id_ = max_id; }

  // Whether ids were reassigned. IDs are reassigned during decoding if the
  // checksum of the file doesn't match, some IDs are missing or not
  // unique. Basically, if the user modified the notes directly we'll
  // reassign the ids to ensure they are unique.
  void set_ids_reassigned(bool value) { ids_reassigned_ = value; }
  bool ids_reassigned() const { return ids_reassigned_; }

  // Whether new UUIDs were assigned to Notes that lacked them.
  void set_uuids_reassigned(bool value) { uuids_reassigned_ = value; }
  bool uuids_reassigned() const { return uuids_reassigned_; }

  void set_has_deprecated_attachments(bool value) {
    has_deprecated_attachments_ = value;
  }
  bool has_deprecated_attachments() const {
    return has_deprecated_attachments_;
  }

  // Returns the string blob representing the sync metadata in the json file.
  // The string blob is set during decode time upon the call to
  // NotesModel::Load.
  void set_sync_metadata_str(std::string sync_metadata_str) {
    sync_metadata_str_ = std::move(sync_metadata_str);
  }
  const std::string& sync_metadata_str() const { return sync_metadata_str_; }

 private:
  std::unique_ptr<NoteNode> root_node_;
  raw_ptr<PermanentNoteNode> main_notes_node_ = nullptr;
  raw_ptr<PermanentNoteNode> other_notes_node_ = nullptr;
  raw_ptr<PermanentNoteNode> trash_notes_node_ = nullptr;
  std::string computed_checksum_;
  std::string stored_checksum_;
  int64_t max_id_ = 1;
  bool ids_reassigned_ = false;
  bool uuids_reassigned_ = false;
  bool has_deprecated_attachments_ = false;
  // A string blob represetning the sync metadata stored in the json file.
  std::string sync_metadata_str_;
};

}  // namespace vivaldi

#endif  // COMPONENTS_NOTES_NOTE_LOAD_DETAILS_H_
