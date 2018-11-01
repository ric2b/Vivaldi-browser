// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NOTES_NOTES_STORAGE_H_
#define NOTES_NOTES_STORAGE_H_

#include <memory>
#include <string>
#include <utility>

#include "base/files/important_file_writer.h"
#include "base/memory/ref_counted.h"

namespace base {
class SequencedTaskRunner;
}

namespace content {
class BrowserContext;
}

namespace vivaldi {
class Notes_Model;
class Notes_Node;

// NotesLoadDetails is used by NotesStorage when loading notes.
// Notes_Model creates a NotesLoadDetails and passes it (including
// ownership) to NotesStorage. NotesStorage loads the notes (and
// index) in the background thread, then calls back to the Notes_Model (on
// the main thread) when loading is done, passing ownership back to the
// Notes_Model. While loading Notes_Model does not maintain references to
// the contents of the NotesLoadDetails, this ensures we don't have any
// threading problems.
class NotesLoadDetails {
 public:
  NotesLoadDetails(Notes_Node* notes_node,
                   Notes_Node* other_notes_node,
                   Notes_Node* trash_notes_node,
                   int64_t max_id);
  ~NotesLoadDetails();

  Notes_Node* notes_node() { return notes_node_.get(); }
  std::unique_ptr<Notes_Node> release_notes_node() {
    return std::move(notes_node_);
  }
  Notes_Node* other_notes_node() { return other_notes_node_.get(); }
  std::unique_ptr<Notes_Node> release_other_notes_node() {
    return std::move(other_notes_node_);
  }
  Notes_Node* trash_notes_node() { return trash_notes_node_.get(); }
  std::unique_ptr<Notes_Node> release_trash_notes_node() {
    return std::move(trash_notes_node_);
  }

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

  int64_t highest_id() const { return highest_id_found_; }
  void update_highest_id(int64_t idx) {
    if (idx > highest_id_found_) {
      highest_id_found_ = idx;
    }
  }

  // Whether ids were reassigned. IDs are reassigned during decoding if the
  // checksum of the file doesn't match, some IDs are missing or not
  // unique. Basically, if the user modified the notes directly we'll
  // reassign the ids to ensure they are unique.
  void set_ids_reassigned(bool value) { ids_reassigned_ = value; }
  bool ids_reassigned() const { return ids_reassigned_; }

 private:
  std::unique_ptr<Notes_Node> notes_node_;
  std::unique_ptr<Notes_Node> other_notes_node_;
  std::unique_ptr<Notes_Node> trash_notes_node_;
  std::string computed_checksum_;
  std::string stored_checksum_;
  int64_t highest_id_found_;
  bool ids_reassigned_;

  DISALLOW_COPY_AND_ASSIGN(NotesLoadDetails);
};

// NotesStorage handles reading/write the notes model. The
// Notes_Model uses the NotesStorage to load notes from disk, as well
// as notifying the NotesStorage every time the model changes.
//
// Internally NotesStorage uses NotesCodec to do the actual read/write.
class NotesStorage : public base::ImportantFileWriter::DataSerializer {
 public:
  // Creates a NotesStorage for the specified model
  NotesStorage(content::BrowserContext* context,
               Notes_Model* model,
               base::SequencedTaskRunner* sequenced_task_runner);

  ~NotesStorage() override;

  // Loads the notes into the model, notifying the model when done. This
  // takes ownership of |details|. See NotesLoadDetails for details.
  void LoadNotes(std::unique_ptr<NotesLoadDetails> details);

  // Schedules saving the notes model to disk.
  void ScheduleSave();

  // Notification the notes model is going to be deleted. If there is
  // a pending save, it is saved immediately.
  void NotesModelDeleted();

  // Callback from backend after loading the notes file.
  void OnLoadFinished(std::unique_ptr<NotesLoadDetails> details);

  // ImportantFileWriter::DataSerializer implementation.
  bool SerializeData(std::string* output) override;

 private:
  friend class base::RefCountedThreadSafe<NotesStorage>;

  // Serializes the data and schedules save using ImportantFileWriter.
  // Returns true on successful serialization.
  bool SaveNow();

  // The model. The model is NULL once NotesModelDeleted has been invoked.
  Notes_Model* model_;

  // Helper to write notes data safely.
  base::ImportantFileWriter writer_;

  // Sequenced task runner where file I/O operations will be performed at.
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  base::WeakPtrFactory<NotesStorage> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NotesStorage);
};

}  // namespace vivaldi

#endif  // NOTES_NOTES_STORAGE_H_
