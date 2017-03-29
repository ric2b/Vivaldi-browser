// Copyright (c) 2014 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/startup_metric_utils/startup_metric_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

#include "notes/notes_attachment.h"
#include "notes/notes_model.h"
#include "notes/notes_storage.h"

using base::TimeTicks;
using content::BrowserThread;

namespace Vivaldi {
  const base::FilePath::CharType kNotesFileName[] = FILE_PATH_LITERAL("Notes");


  // Extension used for backup files (copy of main file created during startup).
  const base::FilePath::CharType kBackupExtension[] = FILE_PATH_LITERAL("bak");

  // How often we save.
  const int kSaveDelayMS = 2500;

  // Traverse the nodes in the tree and sets node ids and returns maximum
  void SetIdsAndGetMax(Notes_Node* node, int64& max) {
    max += 1;
    node->set_id(max);
    for (int i = 0; i < node->child_count(); i++)
      SetIdsAndGetMax(node->GetChild(i), max);
  }


  void BackupCallback(const base::FilePath& path) {
    base::FilePath backup_path = path.ReplaceExtension(kBackupExtension);
    base::CopyFile(path, backup_path);
  }

  void LoadCallback(const base::FilePath& path,
    Vivaldi::NotesStorage* storage,
    Vivaldi::NotesLoadDetails* details) {
    bool notes_file_exists = base::PathExists(path);
    if (notes_file_exists) {
      JSONFileValueDeserializer serializer(path);
      scoped_ptr<base::Value> root(serializer.Deserialize(NULL, NULL));

      if (root.get()) {
        // Building the index can take a while, so we do it on the background
        // thread.
        int64 max_node_id = 0;
        TimeTicks start_time = TimeTicks::Now();
        details->notes_node()->ReadJSON(*static_cast<base::DictionaryValue *>(root.get()));
        UMA_HISTOGRAM_TIMES("Notes.DecodeTime",
          TimeTicks::Now() - start_time);
        SetIdsAndGetMax(details->notes_node(), max_node_id);
        details->update_highest_id(max_node_id);
      }
    }

    BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Vivaldi::NotesStorage::OnLoadFinished, storage));
  }

  // NotesLoadDetails ---------------------------------------------------------

  NotesLoadDetails::NotesLoadDetails(
    Notes_Node* notes_node)
    : notes_node_(notes_node),
    highest_id_found_(0)
  {
  }

  NotesLoadDetails::~NotesLoadDetails() {
  }

  // NotesStorage -------------------------------------------------------------

  NotesStorage::NotesStorage(
    content::BrowserContext* context,
    Notes_Model* model,
    base::SequencedTaskRunner* sequenced_task_runner)
    : model_(model),
    writer_(context->GetPath().Append(kNotesFileName),
    sequenced_task_runner) {
    sequenced_task_runner_ = sequenced_task_runner;
    writer_.set_commit_interval(base::TimeDelta::FromMilliseconds(kSaveDelayMS));
    sequenced_task_runner_->PostTask(FROM_HERE,
      base::Bind(&BackupCallback, writer_.path()));
  }

  NotesStorage::~NotesStorage() {
    if (writer_.HasPendingWrite())
      writer_.DoScheduledWrite();
  }

  void NotesStorage::LoadNotes(NotesLoadDetails* details) {
    DCHECK(!details_.get());
    DCHECK(details);
    details_.reset(details);
    sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LoadCallback, writer_.path(), make_scoped_refptr(this),
      details_.get()));
  }

  void NotesStorage::ScheduleSave() {
    writer_.ScheduleWrite(this);
  }

  void NotesStorage::NotesModelDeleted() {
    // We need to save now as otherwise by the time SaveNow is invoked
    // the model is gone.
    if (writer_.HasPendingWrite())
      SaveNow();

    if (model_ && details_.get() && model_->root() == details_->notes_node())
      details_->release_notes_node();

    model_ = NULL;
  }

  bool NotesStorage::SerializeData(std::string* output) {
    scoped_ptr<base::Value> value(model_->root()->WriteJSON());
    JSONStringValueSerializer serializer(output);
    serializer.set_pretty_print(true);
    return serializer.Serialize(*(value.get()));
  }

  void NotesStorage::OnLoadFinished() {
    if (!model_)
      return;

    model_->DoneLoading(details_.release());
  }

  bool NotesStorage::SaveNow() {
    if (!model_ || !model_->loaded()) {
      // We should only get here if we have a valid model and it's finished
      // loading.
      NOTREACHED();
      return false;
    }

    scoped_ptr<std::string> data;
    if (!SerializeData(data.get()))
      return false;
    writer_.WriteNow(data.Pass());
    return true;
  }

}
