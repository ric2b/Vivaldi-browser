// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#include "sync/file_sync/file_store_storage.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/sync/base/data_type.h"

namespace file_sync {

namespace {
constexpr char kFilesInfo[] = "files_info";

constexpr char kHasContentLocally[] = "has_content_locally";
constexpr char kLocalReferences[] = "local_references";
constexpr char kSyncReferences[] = "sync_refrences";
constexpr char kMimeType[] = "mimetype";

constexpr base::FilePath::CharType kStoreInfoFileName[] =
    FILE_PATH_LITERAL("SyncedFilesData");
constexpr base::FilePath::CharType kBackupExtension[] =
    FILE_PATH_LITERAL("bak");

constexpr base::TimeDelta kSaveDelay = base::Milliseconds(2500);

void BackupCallback(const base::FilePath& path) {
  base::FilePath backup_path = path.ReplaceExtension(kBackupExtension);
  base::CopyFile(path, backup_path);
}

const std::string& AsString(const std::string& value) {
  return value;
}

const std::string& AsString(const base::Uuid& value) {
  return value.AsLowercaseString();
}

template <typename T>
base::Value::List SerializeReferenceSet(const std::set<T>& reference_set) {
  base::Value::List references_list;

  for (const auto& reference : reference_set) {
    references_list.Append(AsString(reference));
  }

  return references_list;
}

template <typename T>
base::Value::Dict SerializeReferences(
    const std::map<syncer::DataType, std::set<T>>& references) {
  base::Value::Dict references_dict;

  for (const auto& reference_set : references) {
    references_dict.Set(
        base::NumberToString(
            syncer::GetSpecificsFieldNumberFromDataType(reference_set.first)),
        SerializeReferenceSet(reference_set.second));
  }

  return references_dict;
}

base::Value::Dict SerializeFileInfo(const SyncedFileData& file_data) {
  base::Value::Dict file_info;

  file_info.Set(kHasContentLocally, file_data.has_content_locally);
  file_info.Set(kMimeType, file_data.mimetype);
  file_info.Set(kLocalReferences,
                SerializeReferences<base::Uuid>(file_data.local_references));
  file_info.Set(kSyncReferences,
                SerializeReferences<std::string>(file_data.sync_references));

  return file_info;
}

template <typename T>
struct ReferenceConverter {};

template <>
struct ReferenceConverter<base::Uuid> {
  static base::Uuid Convert(const std::string& uuid) {
    return base::Uuid::ParseLowercase(uuid);
  }
};

template <>
struct ReferenceConverter<std::string> {
  static const std::string& Convert(const std::string& value) { return value; }
};

template <typename T>
std::set<T> LoadReferencesList(const base::Value::List& references_list) {
  std::set<T> references;
  for (const auto& reference : references_list) {
    if (!reference.is_string())
      continue;
    references.insert(ReferenceConverter<T>::Convert(reference.GetString()));
  }
  return references;
}

template <typename T>
std::map<syncer::DataType, std::set<T>> LoadReferences(
    const base::Value::Dict& references_dict) {
  std::map<syncer::DataType, std::set<T>> references;
  for (const auto references_for_type : references_dict) {
    int model_type_field_number;
    if (!base::StringToInt(references_for_type.first, &model_type_field_number))
      continue;
    syncer::DataType model_type =
        syncer::GetDataTypeFromSpecificsFieldNumber(model_type_field_number);
    if (model_type == syncer::UNSPECIFIED)
      continue;

    if (!references_for_type.second.is_list())
      continue;

    std::set<T> references_set =
        LoadReferencesList<T>(references_for_type.second.GetList());
    if (references_set.empty())
      continue;
    references[model_type] = std::move(references_set);
  }

  return references;
}

std::optional<SyncedFileData> LoadFileInfo(
    const base::Value::Dict& file_info) {
  SyncedFileData file_data;

  std::optional<bool> has_content_locally =
      file_info.FindBool(kHasContentLocally);
  if (!has_content_locally)
    return std::nullopt;

  file_data.has_content_locally = *has_content_locally;

  const std::string* mimetype = file_info.FindString(kMimeType);
  if (!mimetype)
    return std::nullopt;
  file_data.mimetype = *mimetype;

  const base::Value::Dict* local_references_dict =
      file_info.FindDict(kLocalReferences);
  if (!local_references_dict)
    return std::nullopt;

  file_data.local_references =
      LoadReferences<base::Uuid>(*local_references_dict);

  const base::Value::Dict* sync_references_dict =
      file_info.FindDict(kSyncReferences);
  if (!sync_references_dict)
    return std::nullopt;

  file_data.sync_references =
      LoadReferences<std::string>(*sync_references_dict);

  // File has no references and no local content. It practically doesn't exist.
  if (file_data.IsUnreferenced() && !file_data.has_content_locally) {
    return std::nullopt;
  }

  return file_data;
}

SyncedFilesData DoLoad(const base::FilePath& path) {
  SyncedFilesData files_data;

  // The output directory needs to be available both when writing the files
  // information and for writing the files themselves. Since we don't know which
  // will happen first, create it here, before any of them can happen.
  base::CreateDirectory(path.DirName());

  JSONFileValueDeserializer serializer(path);
  std::unique_ptr<base::Value> json(serializer.Deserialize(nullptr, nullptr));

  if (!json || !json->is_dict())
    return files_data;

  auto& root = json->GetDict();

  base::Value::Dict* files_info = root.FindDict(kFilesInfo);
  if (!files_info)
    return files_data;

  for (auto file_info : *files_info) {
    if (!file_info.second.is_dict())
      continue;
    std::optional<SyncedFileData> file_data =
        LoadFileInfo(file_info.second.GetDict());
    if (file_data)
      files_data[file_info.first] = std::move(*file_data);
  }

  return files_data;
}
}  // namespace

/*static*/
void SyncedFileStoreStorage::Load(const base::FilePath& local_store_path,
                                  LoadCallback loading_done_callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&DoLoad, local_store_path.Append(kStoreInfoFileName)),
      std::move(loading_done_callback));
}

SyncedFileStoreStorage::SyncedFileStoreStorage(
    FilesDataGetter files_data_getter,
    const base::FilePath& local_store_path,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner)
    : files_data_getter_(std::move(files_data_getter)),
      file_task_runner_(file_task_runner),
      writer_(local_store_path.Append(kStoreInfoFileName),
              file_task_runner_,
              kSaveDelay) {}

SyncedFileStoreStorage::~SyncedFileStoreStorage() {
  if (writer_.HasPendingWrite())
    writer_.DoScheduledWrite();
}

void SyncedFileStoreStorage::ScheduleSave() {
  // If this is the first scheduled save, create a backup before overwriting the
  // JSON file.
  if (!backup_triggered_) {
    backup_triggered_ = true;
    file_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&BackupCallback, writer_.path()));
  }

  writer_.ScheduleWriteWithBackgroundDataSerializer(this);
}

void SyncedFileStoreStorage::OnFileStoreDeleted() {
  // We need to save now as otherwise by the time SerializeData() is invoked
  // the model is gone.
  if (writer_.HasPendingWrite()) {
    writer_.DoScheduledWrite();
    DCHECK(!writer_.HasPendingWrite());
  }
}

base::ImportantFileWriter::BackgroundDataProducerCallback
SyncedFileStoreStorage::GetSerializedDataProducerForBackgroundSequence() {
  base::Value::Dict root;

  base::Value::Dict files_info;

  const auto& files_data = files_data_getter_.Run();
  for (const auto& file_data : files_data) {
    files_info.Set(file_data.first, SerializeFileInfo(file_data.second));
  }

  // Currently there is no file-info-independent data to save, but we leave open
  // the option for some to be added in the future by not having the file info
  // directly at the root
  root.Set(kFilesInfo, std::move(files_info));

  return base::BindOnce(
      [](base::Value::Dict root) -> std::optional<std::string> {
        // This runs on the background sequence.
        std::string output;
        JSONStringValueSerializer serializer(&output);
        serializer.set_pretty_print(true);
        if (serializer.Serialize(root))
          return output;
        return std::nullopt;
      },
      std::move(root));
}

}  // namespace file_sync