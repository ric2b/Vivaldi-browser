// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#include "sessions/index_storage.h"

#include <memory>
#include <utility>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/uuid.h"
#include "base/values.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "components/datasource/resource_reader.h"
#include "sessions/index_codec.h"
#include "sessions/index_model.h"
#include "sessions/index_node.h"

namespace sessions {

const base::FilePath::CharType kSessionsFolder[] = FILE_PATH_LITERAL("Sessions");
const base::FilePath::CharType kFileName[] =
    FILE_PATH_LITERAL("sessions.json");
const base::FilePath::CharType kBackupExtension[] = FILE_PATH_LITERAL("bak");

// How often we save.
const int kSaveDelayMS = 2500;

// Make a backup. Note, this file only exists during startup, It is deleted
// elsewhere once startup completes.
void MakeBackup(const base::FilePath& path) {
  base::FilePath backup_path = path.ReplaceExtension(kBackupExtension);
  base::CopyFile(path, backup_path);
}

bool GetVersion(std::string* version, const base::FilePath& file) {
  bool exists = base::PathExists(file);
  if (exists) {
    JSONFileValueDeserializer serializer(file);
    std::unique_ptr<base::Value> root(serializer.Deserialize(NULL, NULL));
    if (root.get()) {
      IndexCodec codec;
      return codec.GetVersion(version, *root.get());
    }
  }
  return false;
}

void OnLoad(const base::FilePath& directory,
            const base::FilePath::StringPieceType& filename,
            const base::WeakPtr<sessions::IndexStorage> storage,
            std::unique_ptr<IndexLoadDetails> details) {
  const base::FilePath file = directory.Append(filename);
  bool exists = base::PathExists(file);
  if (!exists) {
    IndexCodec codec;
    if (!codec.Decode(details->items_node(), directory, filename)) {
      LOG(ERROR)
          << "Session Index Storage: Failed to set up from file list";
      details->SetLoadingFailed(true);
    }
    details->SetLoadedFromFileScan(true);
  }
  if (exists) {
    JSONFileValueDeserializer serializer(file);
    std::unique_ptr<base::Value> root(serializer.Deserialize(NULL, NULL));
    if (!root.get()) {
      LOG(ERROR) << "Session Index Storage: Failed to parse JSON. Check format";
      std::string content;
      base::ReadFileToString(file, &content);
      LOG(ERROR) << "Session Index Storage: " << file;
      LOG(ERROR) << "Session Index Storage: Content: " << content;
      details->SetLoadingFailed(true);
    } else {
      IndexCodec codec;
      if (!codec.Decode(details->items_node(), details->backup_node(),
                        details->persistent_node(), *root.get())) {
        LOG(ERROR)
          << "Session Index Storage: Failed to decode JSON content from: "
          << file;
        details->SetLoadingFailed(true);
      }
    }
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindRepeating(&IndexStorage::OnLoadFinished, storage,
                                     base::Passed(&details)));
}

IndexLoadDetails::IndexLoadDetails(Index_Node* items_node,
                                   Index_Node* backup_node,
                                   Index_Node* persistent_node)
  : items_node_(items_node), backup_node_(backup_node),
    persistent_node_(persistent_node) {}

IndexLoadDetails::~IndexLoadDetails() {}


IndexStorage::IndexStorage(content::BrowserContext* context,
                           Index_Model* model,
                           base::SequencedTaskRunner* sequenced_task_runner)
    : model_(model),
      directory_(context->GetPath().Append(kSessionsFolder)),
      writer_(context->GetPath().Append(kSessionsFolder).Append(kFileName),
              sequenced_task_runner,
              base::Milliseconds(kSaveDelayMS)),
      weak_factory_(this) {
  sequenced_task_runner_ = sequenced_task_runner;
  sequenced_task_runner_->PostTask(FROM_HERE,
                                   base::BindOnce(&MakeBackup, writer_.path()));
}

IndexStorage::~IndexStorage() {
  if (writer_.HasPendingWrite())
    writer_.DoScheduledWrite();
}

// static
const base::FilePath::CharType* IndexStorage::GetFolderName() {
  return kSessionsFolder;
}

void IndexStorage::Load(std::unique_ptr<IndexLoadDetails> details) {
  DCHECK(details);
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindRepeating(&OnLoad, directory_/*writer_.path()*/, kFileName,
                          weak_factory_.GetWeakPtr(), base::Passed(&details)));
}

void IndexStorage::ScheduleSave() {
  switch (backup_state_) {
    case BACKUP_NONE:
      backup_state_ = BACKUP_DISPATCHED;
      sequenced_task_runner_->PostTaskAndReply(
          FROM_HERE, base::BindOnce(&MakeBackup, writer_.path()),
          base::BindOnce(&IndexStorage::OnBackupFinished,
                         weak_factory_.GetWeakPtr()));
      return;
    case BACKUP_DISPATCHED:
      return;
    case BACKUP_ATTEMPTED:
      writer_.ScheduleWrite(this);
      return;
  }
  NOTREACHED();
}

void IndexStorage::OnBackupFinished() {
  backup_state_ = BACKUP_ATTEMPTED;
  ScheduleSave();
}

void IndexStorage::OnModelWillBeDeleted() {
  // We need to save now as otherwise by the time SaveNow is invoked
  // the model is gone.
  if (writer_.HasPendingWrite())
    SaveNow();

  model_ = nullptr;
}

std::optional<std::string> IndexStorage::SerializeData() {
  IndexCodec codec;
  std::string output;
  JSONStringValueSerializer serializer(&output);
  serializer.set_pretty_print(true);
  base::Value value = codec.Encode(model_);
  if (!serializer.Serialize(value))
    return std::nullopt;

  return output;
}

void IndexStorage::OnLoadFinished(std::unique_ptr<IndexLoadDetails> details) {
  if (model_) {
    model_->LoadFinished(std::move(details));
  }
}

bool IndexStorage::SaveNow() {
  if (!model_ || !model_->loaded()) {
    // We should only get here if we have a valid model and it's finished
    // loading.
    NOTREACHED();
    //return false;
  }

  std::optional<std::string> data = SerializeData();
  if (!data)
    return false;
  writer_.WriteNow(data.value());
  return true;
}

bool IndexStorage::SaveValue(const std::unique_ptr<base::Value>& value) {
  std::string data;
  JSONStringValueSerializer serializer(&data);
  serializer.set_pretty_print(true);
  if (!serializer.Serialize(*(value.get()))) {
    return false;
  }
  writer_.WriteNow(data);
  return true;
}

}  // namespace sessions
