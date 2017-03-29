// Copyright (c) 2014 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIVALDI_NOTES_NOTES_STORAGE_H_
#define VIVALDI_NOTES_NOTES_STORAGE_H_

#include "base/files/important_file_writer.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace base {
  class SequencedTaskRunner;
}

namespace content {
  class BrowserContext;
}

namespace Vivaldi
{

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
    NotesLoadDetails(Notes_Node* notes_node);
    ~NotesLoadDetails();

    Notes_Node* notes_node() { return notes_node_.get(); }
    Notes_Node* release_notes_node() { return notes_node_.release(); }

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

    const int64 highest_id() { return highest_id_found_; }
    void update_highest_id(int64 idx) {
      if (idx > highest_id_found_) {
        highest_id_found_ = idx;
      }
    }

  private:
    scoped_ptr<Notes_Node> notes_node_;
    std::string computed_checksum_;
    std::string stored_checksum_;
    int64 highest_id_found_;

    DISALLOW_COPY_AND_ASSIGN(NotesLoadDetails);
  };

  // NotesStorage handles reading/write the bookmark bar model. The
  // Notes_Model uses the NotesStorage to load notes from disk, as well
  // as notifying the NotesStorage every time the model changes.
  //
  // Internally NotesStorage uses BookmarkCodec to do the actual read/write.
  class NotesStorage : public base::ImportantFileWriter::DataSerializer,
    public base::RefCountedThreadSafe < NotesStorage > {
  public:
    // Creates a NotesStorage for the specified model
    NotesStorage(content::BrowserContext* context,
      Notes_Model* model,
      base::SequencedTaskRunner* sequenced_task_runner);

    // Loads the notes into the model, notifying the model when done. This
    // takes ownership of |details|. See NotesLoadDetails for details.
    void LoadNotes(NotesLoadDetails* details);

    // Schedules saving the bookmark bar model to disk.
    void ScheduleSave();

    // Notification the bookmark bar model is going to be deleted. If there is
    // a pending save, it is saved immediately.
    void NotesModelDeleted();

    // Callback from backend after loading the bookmark file.
    void OnLoadFinished();

    // ImportantFileWriter::DataSerializer implementation.
    bool SerializeData(std::string* output) override;

  private:
    friend class base::RefCountedThreadSafe < NotesStorage > ;

    ~NotesStorage() override;

    // Serializes the data and schedules save using ImportantFileWriter.
    // Returns true on successful serialization.
    bool SaveNow();

    // The model. The model is NULL once NotesModelDeleted has been invoked.
    Notes_Model* model_;

    // Helper to write bookmark data safely.
    base::ImportantFileWriter writer_;

    // See class description of NotesLoadDetails for details on this.
    scoped_ptr<NotesLoadDetails> details_;

    // Sequenced task runner where file I/O operations will be performed at.
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

    DISALLOW_COPY_AND_ASSIGN(NotesStorage);
  };

}
#endif  // VIVALDI_NOTES_NOTES_STORAGE_H_
