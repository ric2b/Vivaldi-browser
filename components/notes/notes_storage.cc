// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notes/notes_storage.h"

#include <memory>
#include <utility>

#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/notes/note_constants.h"
#include "components/notes/note_node.h"
#include "components/notes/notes_codec.h"
#include "components/notes/notes_model.h"
#include "sync/notes/note_sync_service.h"

using base::TimeTicks;

namespace vivaldi {
namespace {
// Extension used for backup files (copy of main file created during startup).
const base::FilePath::CharType kBackupExtension[] = FILE_PATH_LITERAL("bak");

void BackupCallback(const base::FilePath& path) {
  base::FilePath backup_path = path.ReplaceExtension(kBackupExtension);
  base::CopyFile(path, backup_path);
}
}  // namespace

// static
constexpr base::TimeDelta NotesStorage::kSaveDelay;

NotesStorage::NotesStorage(NotesModel* model,
                           const base::FilePath& profile_path)
    : model_(model),
      backend_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      writer_(profile_path.Append(kNotesFileName),
              backend_task_runner_,
              kSaveDelay) {}

NotesStorage::~NotesStorage() {
  if (writer_.HasPendingWrite())
    writer_.DoScheduledWrite();
}

void NotesStorage::ScheduleSave() {
  // If this is the first scheduled save, create a backup before overwriting the
  // JSON file.
  if (!backup_triggered_) {
    backup_triggered_ = true;
    backend_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&BackupCallback, writer_.path()));
  }

  writer_.ScheduleWrite(this);
}

void NotesStorage::NotesModelDeleted() {
  // We need to save now as otherwise by the time SerializeData() is invoked
  // the model is gone.
  if (writer_.HasPendingWrite()) {
    writer_.DoScheduledWrite();
    DCHECK(!writer_.HasPendingWrite());
  }

  model_ = nullptr;
}

std::optional<std::string> NotesStorage::SerializeData() {
  NotesCodec codec;
  std::string output;
  base::Value value(
      codec.Encode(model_, model_->sync_service()->EncodeNoteSyncMetadata()));
  JSONStringValueSerializer serializer(&output);
  serializer.set_pretty_print(true);
  if (!serializer.Serialize(value))
    return std::nullopt;

  return output;
}

}  // namespace vivaldi
