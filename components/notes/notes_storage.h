// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOTES_NOTES_STORAGE_H_
#define COMPONENTS_NOTES_NOTES_STORAGE_H_

#include <string>

#include "base/files/file_path.h"
#include "base/files/important_file_writer.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "components/notes/note_node.h"

namespace base {
class SequencedTaskRunner;
}

namespace vivaldi {
class NotesModel;

// NotesStorage handles writing note model to disk (as opposed to
// NoteModelLoader which takes care of loading).
//
// Internally NotesStorage uses NotesCodec to do the actual read/write.
class NotesStorage : public base::ImportantFileWriter::DataSerializer {
 public:
  // How often the file is saved at most.
  static constexpr base::TimeDelta kSaveDelay = base::Milliseconds(2500);

  // Creates a NotesStorage for the specified model. The data will saved to a
  // location derived from |profile_path|. The disk writes will be executed as a
  // task in a backend task runner.
  NotesStorage(NotesModel* model, const base::FilePath& profile_path);
  ~NotesStorage() override;
  NotesStorage(const NotesStorage&) = delete;
  NotesStorage& operator=(const NotesStorage&) = delete;

  // Schedules saving the notes model to disk.
  void ScheduleSave();

  // Notification the notes model is going to be deleted. If there is
  // a pending save, it is saved immediately.
  void NotesModelDeleted();

  // ImportantFileWriter::DataSerializer implementation.
  std::optional<std::string> SerializeData() override;

 private:
  // The model. The model is NULL once NotesModelDeleted has been invoked.
  raw_ptr<NotesModel> model_;

  // Sequenced task runner where disk writes will be performed at.
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // Helper to write notes data safely.
  base::ImportantFileWriter writer_;

  // The state of the backup file creation which is created lazily just before
  // the first scheduled save.
  bool backup_triggered_ = false;
};

}  // namespace vivaldi

#endif  // COMPONENTS_NOTES_NOTES_STORAGE_H_
