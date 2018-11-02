// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "notes/notes_storage.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

#include "notes/notes_codec.h"
#include "notes/notes_model.h"

using base::TimeTicks;
using content::BrowserThread;

namespace vivaldi {
const base::FilePath::CharType kNotesFileName[] = FILE_PATH_LITERAL("Notes");

// Extension used for backup files (copy of main file created during startup).
const base::FilePath::CharType kBackupExtension[] = FILE_PATH_LITERAL("bak");

// How often we save.
const int kSaveDelayMS = 2500;

void BackupCallback(const base::FilePath& path) {
  base::FilePath backup_path = path.ReplaceExtension(kBackupExtension);
  base::CopyFile(path, backup_path);
}

void LoadCallback(const base::FilePath& path,
                  const base::WeakPtr<vivaldi::NotesStorage> storage,
                  std::unique_ptr<NotesLoadDetails> details) {
  bool notes_file_exists = base::PathExists(path);
  if (notes_file_exists) {
    JSONFileValueDeserializer serializer(path);
    std::unique_ptr<base::Value> root(serializer.Deserialize(NULL, NULL));

    if (root.get()) {
      // Building the index can take a while, so we do it on the background
      // thread.
      int64_t max_node_id = 0;
      NotesCodec codec;
      TimeTicks start_time = TimeTicks::Now();
      codec.Decode(details->notes_node(), details->other_notes_node(),
                   details->trash_notes_node(), &max_node_id, *root.get());
      details->update_highest_id(max_node_id);
      details->set_computed_checksum(codec.computed_checksum());
      details->set_stored_checksum(codec.stored_checksum());
      details->set_ids_reassigned(codec.ids_reassigned());
      UMA_HISTOGRAM_TIMES("Notes.DecodeTime", TimeTicks::Now() - start_time);
    }
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&NotesStorage::OnLoadFinished, storage,
                                     base::Passed(&details)));
}

// NotesLoadDetails ---------------------------------------------------------
NotesLoadDetails::NotesLoadDetails(Notes_Node* notes_node,
                                   Notes_Node* other_notes_node,
                                   Notes_Node* trash_notes_node,
                                   int64_t max_id)
    : notes_node_(notes_node),
      other_notes_node_(other_notes_node),
      trash_notes_node_(trash_notes_node),
      highest_id_found_(max_id),
      ids_reassigned_(false) {}

NotesLoadDetails::~NotesLoadDetails() {}

// NotesStorage -------------------------------------------------------------

NotesStorage::NotesStorage(content::BrowserContext* context,
                           Notes_Model* model,
                           base::SequencedTaskRunner* sequenced_task_runner)
    : model_(model),
      writer_(context->GetPath().Append(kNotesFileName),
              sequenced_task_runner,
              base::TimeDelta::FromMilliseconds(kSaveDelayMS)),
      weak_factory_(this) {
  sequenced_task_runner_ = sequenced_task_runner;
  sequenced_task_runner_->PostTask(FROM_HERE,
                                   base::Bind(&BackupCallback, writer_.path()));
}

NotesStorage::~NotesStorage() {
  if (writer_.HasPendingWrite())
    writer_.DoScheduledWrite();
}

void NotesStorage::LoadNotes(std::unique_ptr<NotesLoadDetails> details) {
  DCHECK(details);
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LoadCallback, writer_.path(), weak_factory_.GetWeakPtr(),
                 base::Passed(&details)));
}

void NotesStorage::ScheduleSave() {
  DCHECK(model_);
  writer_.ScheduleWrite(this);
}

void NotesStorage::NotesModelDeleted() {
  // We need to save now as otherwise by the time SaveNow is invoked
  // the model is gone.
  if (writer_.HasPendingWrite())
    SaveNow();

  model_ = NULL;
}

bool NotesStorage::SerializeData(std::string* output) {
  NotesCodec codec;
  std::unique_ptr<base::Value> value(codec.Encode(model_));
  JSONStringValueSerializer serializer(output);
  serializer.set_pretty_print(true);
  return serializer.Serialize(*(value.get()));
}

void NotesStorage::OnLoadFinished(std::unique_ptr<NotesLoadDetails> details) {
  if (!model_)
    return;

  model_->DoneLoading(std::move(details));
}

bool NotesStorage::SaveNow() {
  if (!model_ || !model_->loaded()) {
    // We should only get here if we have a valid model and it's finished
    // loading.
    NOTREACHED();
    return false;
  }

  std::unique_ptr<std::string> data(new std::string);
  if (!SerializeData(data.get()))
    return false;
  writer_.WriteNow(std::move(data));
  return true;
}

}  // namespace vivaldi
