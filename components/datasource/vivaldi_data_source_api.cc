// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "components/datasource/vivaldi_data_source_api.h"

#include "base/guid.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/common/api/extension_types.h"
#include "prefs/vivaldi_gen_prefs.h"

const char kDatasourceFilemappingFilename[] = "file_mapping.json";
const char kDatasourceFilemappingTmpFilename[] = "file_mapping.tmp";
const base::FilePath::StringPieceType kThumbnailDirectory =
    FILE_PATH_LITERAL("VivaldiThumbnails");

namespace extensions {

// Helper to store ref-counted VivaldiDataSourcesAPI in BrowserContext.
class VivaldiDataSourcesAPIHolder : public BrowserContextKeyedAPI {
 public:
  explicit VivaldiDataSourcesAPIHolder(content::BrowserContext* context);
  ~VivaldiDataSourcesAPIHolder() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<VivaldiDataSourcesAPIHolder>*
    GetFactoryInstance();

 private:
  friend class VivaldiDataSourcesAPI;

  friend class BrowserContextKeyedAPIFactory<VivaldiDataSourcesAPIHolder>;
  static const char* service_name() {
    return "VivaldiDataSourcesAPI";
  }
  static const bool kServiceIsNULLWhileTesting = false;
  static const bool kServiceRedirectedInIncognito = true;

  // KeyedService
  void Shutdown() override;

  void OnPrefChange(int pref_index, const std::string& name);

  scoped_refptr<VivaldiDataSourcesAPI> api_;

  // Cached values of profile paths that can contain data mapping urls and the
  // registrar to monitor the corresponding preference changes to delete no
  // longer used mappings.
  std::string profile_path_urls_[VivaldiDataSourcesAPI::kDataMappingPrefsCount];
  PrefChangeRegistrar pref_change_registrar_;
};

const char* VivaldiDataSourcesAPI::kDataMappingPrefs[] = {
    vivaldiprefs::kThemeWindowBackgroundImageUrl,
    vivaldiprefs::kStartpageImagePathCustom,
};

VivaldiDataSourcesAPI::DataSourceItem::DataSourceItem() = default;

VivaldiDataSourcesAPI::DataSourceItem::DataSourceItem(base::FilePath file_path)
    : file_path_(std::move(file_path)) {}

VivaldiDataSourcesAPI::DataSourceItem::DataSourceItem(DataSourceItem&&) =
    default;

VivaldiDataSourcesAPI::DataSourceItem& VivaldiDataSourcesAPI::DataSourceItem::
operator=(DataSourceItem&&) = default;

VivaldiDataSourcesAPI::DataSourceItem::~DataSourceItem() = default;

VivaldiDataSourcesAPI::VivaldiDataSourcesAPI(Profile* profile)
    : profile_(profile),
      user_data_dir_(profile->GetPath()),
      sequence_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::TaskPriority::USER_VISIBLE, base::MayBlock()})) {}

VivaldiDataSourcesAPI::~VivaldiDataSourcesAPI() {}

// static
bool VivaldiDataSourcesAPI::ReadFileOnBlockingThread(
    const base::FilePath& file_path,
    std::vector<unsigned char>* buffer) {
  base::File file(file_path, base::File::FLAG_READ | base::File::FLAG_OPEN);
  if (!file.IsValid()) {
    // Treat the file that does not exist as an empty file and do not log the
    // error.
    if (file.error_details() != base::File::FILE_ERROR_NOT_FOUND) {
      LOG(ERROR) << "Failed to open file " << file_path.value()
                 << " for reading";
    }
    return false;
  }
  int64_t len64 = file.GetLength();
  if (len64 < 0 || len64 >= (static_cast<int64_t>(1) << 31)) {
    LOG(ERROR) << "Unexpected file length for " << file_path.value() << " - "
               << len64;
    return false;
  }
  int len = static_cast<int>(len64);
  if (len == 0)
    return false;

  buffer->resize(static_cast<size_t>(len));
  int read_len = file.Read(0, reinterpret_cast<char*>(buffer->data()), len);
  if (read_len != len) {
    LOG(ERROR) << "Failed to read " << len << "bytes from "
               << file_path.value();
    return false;
  }
  return true;
}

// static
scoped_refptr<base::RefCountedMemory>
VivaldiDataSourcesAPI::ReadFileOnBlockingThread(
    const base::FilePath& file_path) {
  std::vector<unsigned char> buffer;
  if (ReadFileOnBlockingThread(file_path, &buffer)) {
    return base::RefCountedBytes::TakeVector(&buffer);
  }
  return scoped_refptr<base::RefCountedMemory>();
}

void VivaldiDataSourcesAPI::LoadMappings() {
  sequence_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VivaldiDataSourcesAPI::LoadMappingsOnFileThread, this));
}

void VivaldiDataSourcesAPI::LoadMappingsOnFileThread() {
  DCHECK(sequence_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(id_to_file_map_.empty());

  base::FilePath file_path = GetFileMappingFilePath();
  std::vector<unsigned char> data;
  if (!ReadFileOnBlockingThread(file_path, &data))
    return;

  base::JSONReader json_reader;
  base::Optional<base::Value> root = json_reader.ReadToValue(
      base::StringPiece(reinterpret_cast<char*>(data.data()), data.size()));
  if (!root) {
    LOG(ERROR) << file_path.value() << " is not a valid JSON - "
               << json_reader.GetErrorMessage();
    return;
  }

  if (root->is_dict()) {
    if (const base::Value* mappings_value = root->FindDictKey("mappings")) {
      InitMappingsOnFileThread(
          static_cast<const base::DictionaryValue*>(mappings_value));
    }
  }
}

void VivaldiDataSourcesAPI::InitMappingsOnFileThread(
    const base::DictionaryValue* dict) {
  DCHECK(sequence_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(id_to_file_map_.empty());

  for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    const std::string& id = it.key();
    const base::Value* root = nullptr;
    if (!dict->GetWithoutPathExpansion(id, &root)) {
      LOG(WARNING) << "Invalid entry in \"" << kDatasourceFilemappingFilename
        << "\" file.";
      continue;
    }
    const base::DictionaryValue* value = nullptr;
    if (root->GetAsDictionary(&value)) {
      for (base::DictionaryValue::Iterator values_it(*value);
           !values_it.IsAtEnd(); values_it.Advance()) {
        const base::Value* sub_value = nullptr;
        if (value->GetWithoutPathExpansion(values_it.key(), &sub_value)) {
          if (values_it.key() == "local_path" ||
              values_it.key() == "relative_path") {
            std::string file;
            if (sub_value->GetAsString(&file)) {
#if defined(OS_POSIX)
              base::FilePath path(file);
#elif defined(OS_WIN)
              base::FilePath path(base::UTF8ToWide(file));
#endif
              id_to_file_map_.emplace(id, DataSourceItem(std::move(path)));
            }
          }
        }
      }
    }
  }
}

std::string VivaldiDataSourcesAPI::GetMappingJSONOnFileThread() {
  DCHECK(sequence_task_runner_->RunsTasksInCurrentSequence());

  if (id_to_file_map_.empty())
    return std::string();

  std::vector<base::Value::DictStorage::value_type> items;
  for (const auto& it : id_to_file_map_) {
    const base::FilePath& path = it.second.GetFilePath();
    auto item = std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
    if (path.IsAbsolute()) {
      item->SetStringKey("local_path", path.value());
    } else {
      item->SetStringKey("relative_path", path.value());
    }
    items.emplace_back(it.first, std::move(item));
  }

  base::Value root(base::Value::Type::DICTIONARY);
  root.SetKey("mappings",
              base::Value(base::Value::DictStorage(std::move(items))));

  std::string json;
  base::JSONWriter::WriteWithOptions(
      root, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  return json;
}

void VivaldiDataSourcesAPI::SaveMappingsOnFileThread() {
  DCHECK(sequence_task_runner_->RunsTasksInCurrentSequence());

  if (bulk_changes_) {
    unsaved_changes_ = true;
    return;
  }

  std::string json = GetMappingJSONOnFileThread();
  base::FilePath path = GetFileMappingFilePath();
  if (json.empty()) {
    if (!base::DeleteFile(path, false)) {
      LOG(ERROR) << "failed to delete " << path.value();
    }
    return;
  }

  if (json.length() >= (1U << 31)) {
    LOG(ERROR) << "the size to write is too big - " << json.length();
    return;
  }

  // Write via a temporary to prevent leaving a corrupted file on browser
  // crashes, disk full etc. Note that still can leave the file corrupted
  // on OS crashes or power loss, but loosing thumbnails is not the end of
  // the world.

  base::FilePath tmp_path =
      user_data_dir_.AppendASCII(kDatasourceFilemappingTmpFilename);

  int length = static_cast<int>(json.length());
  if (length != base::WriteFile(tmp_path, json.data(), length)) {
    LOG(ERROR) << "Failed to write to " << tmp_path.value() << " " << length
               << " bytes";
    return;
  }
  if (!base::ReplaceFile(tmp_path, path, nullptr)) {
    LOG(ERROR) << "Failed to rename " << tmp_path.value() << " to "
               << path.value();
  }
}

// static
void VivaldiDataSourcesAPI::SetBulkChangesMode(
    content::BrowserContext* browser_context,
    bool enable) {
  VivaldiDataSourcesAPI* api = FromBrowserContext(browser_context);
  DCHECK(api);
  if (!api) {
    return;
  }

  api->sequence_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VivaldiDataSourcesAPI::SetBulkChangesModeOnFileThread,
                     api, enable));
}

void VivaldiDataSourcesAPI::SetBulkChangesModeOnFileThread(bool enable) {
  DCHECK(sequence_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(enable != bulk_changes_);
  bulk_changes_ = enable;
  if (enable) {
    DCHECK(!unsaved_changes_);
  } else {
    if (unsaved_changes_) {
      unsaved_changes_ = false;
      SaveMappingsOnFileThread();
    }
  }
}

base::FilePath VivaldiDataSourcesAPI::GetFileMappingFilePath() {
  return user_data_dir_.AppendASCII(kDatasourceFilemappingFilename);
}

base::FilePath VivaldiDataSourcesAPI::GetBookmarkThumbnailPath(
    int64_t bookmark_id) {
  base::FilePath path = user_data_dir_.Append(kThumbnailDirectory);
  std::string filename =
      base::StringPrintf("%jd.png", static_cast<intmax_t>(bookmark_id));
#if defined(OS_POSIX)
  path = path.Append(filename);
#elif defined(OS_WIN)
  path = path.Append(base::UTF8ToUTF16(filename));
#endif
  return path;
}

// static
std::string VivaldiDataSourcesAPI::GetBookmarkThumbnailUrl(
    int64_t bookmark_id) {
  return base::StringPrintf("%s%jd", ::vivaldi::kBaseFileMappingUrl,
                            static_cast<intmax_t>(bookmark_id));
}

// static
bool VivaldiDataSourcesAPI::IsBookmarkCapureUrl(const std::string& url) {
  std::string id;
  int64_t bookmark_id = 0;
  return GetDataMappingId(url, &id) && base::StringToInt64(id, &bookmark_id);
}

// static
bool VivaldiDataSourcesAPI::GetDataMappingId(const std::string& url,
                                             std::string* id) {
  if (url.empty())
    return false;

  // Special-case resource URLs that do not have scheme or path components
  if (base::StringPiece(url).starts_with("/resources/"))
    return false;

  GURL gurl(url);
  if (!gurl.is_valid()) {
    LOG(WARNING) << "The url argument is not a valid URL - " << url;
    return false;
  }

  if (gurl.SchemeIs(VIVALDI_DATA_URL_SCHEME) &&
      gurl.host_piece() == VIVALDI_DATA_URL_HOST) {
    base::StringPiece path = gurl.path_piece();
    if (path.starts_with(VIVALDI_DATA_URL_MAPPING_DIR)) {
      path.remove_prefix(strlen(VIVALDI_DATA_URL_MAPPING_DIR));
      path.CopyToString(id);
      return true;
    }
  }
  return false;
}

// static
std::string VivaldiDataSourcesAPI::GetDataMappingUrl(const std::string& id) {
  return base::StringPrintf("%s%s", ::vivaldi::kBaseFileMappingUrl, id.c_str());
}

// static
void VivaldiDataSourcesAPI::AddMapping(content::BrowserContext* browser_context,
                                       base::FilePath file_path,
                                       AddMappingCallback callback) {
  VivaldiDataSourcesAPI* api = FromBrowserContext(browser_context);
  DCHECK(api);
  if (!api) {
    std::move(callback).Run(nullptr, false, std::string());
    return;
  }

  api->sequence_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VivaldiDataSourcesAPI::AddMappingOnFileThread,
                     api, std::move(file_path), std::move(callback)));
}

void VivaldiDataSourcesAPI::AddMappingOnFileThread(
    base::FilePath file_path,
    AddMappingCallback callback) {
  DCHECK(sequence_task_runner_->RunsTasksInCurrentSequence());

  std::string id = base::GenerateGUID();
  id_to_file_map_.emplace(id, DataSourceItem(std::move(file_path)));

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&VivaldiDataSourcesAPI::FinishAddMappingOnUIThread, this,
                     std::move(id), std::move(callback)));
  SaveMappingsOnFileThread();
}

void VivaldiDataSourcesAPI::FinishAddMappingOnUIThread(
    std::string id,
    AddMappingCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // profile_ is null on shutdown.
  std::move(callback).Run(profile_, true, GetDataMappingUrl(id));
}

// static
void VivaldiDataSourcesAPI::OnUrlChange(
    content::BrowserContext* browser_context,
    const std::string& old_url,
    const std::string& new_url) {
  VivaldiDataSourcesAPI* api = FromBrowserContext(browser_context);
  DCHECK(api);
  if (!api)
    return;
  api->OnUrlChange(old_url, new_url);
}

void VivaldiDataSourcesAPI::OnUrlChange(const std::string& old_url,
                                        const std::string& new_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::string old_id;
  if (!GetDataMappingId(old_url, &old_id))
    return;
  std::string new_id;
  if (GetDataMappingId(new_url, &new_id) && new_id == old_id)
    return;

  sequence_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VivaldiDataSourcesAPI::RemoveMappingOnFileThread,
                     this, std::move(old_id)));
}

void VivaldiDataSourcesAPI::RemoveMappingOnFileThread(std::string id) {
  if (!id_to_file_map_.erase(id)) {
    LOG(WARNING) << "Data mapping URL with unknown id - " << id;
    return;
  }
  int64_t bookmark_id;
  if (!base::StringToInt64(id, &bookmark_id)) {
    // If id starts with digits, StringToInt64 sets bookmark_id to number that
    // it was able to extract before returning false.
    bookmark_id = 0;
  }

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&VivaldiDataSourcesAPI::ClearCacheOnIOThread,
                     this, std::move(id)));
  SaveMappingsOnFileThread();

  // Remove the captured thumbnail file if any
  if (bookmark_id) {
    base::DeleteFile(GetBookmarkThumbnailPath(bookmark_id), false);
  }
}

void VivaldiDataSourcesAPI::SetCacheOnIOThread(
    std::string id,
    scoped_refptr<base::RefCountedMemory> data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(data);
  io_thread_data_cache_[id] = std::move(data);
}

void VivaldiDataSourcesAPI::ClearCacheOnIOThread(std::string id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  io_thread_data_cache_.erase(id);
}

void VivaldiDataSourcesAPI::GetDataForId(
    const std::string& id,
    content::URLDataSource::GotDataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  auto it = io_thread_data_cache_.find(id);
  if (it != io_thread_data_cache_.end()) {
    callback.Run(it->second);
    return;
  }

  sequence_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VivaldiDataSourcesAPI::GetDataForIdOnFileThread, this, id,
                     std::move(callback)));
}

void VivaldiDataSourcesAPI::GetDataForIdOnFileThread(
    std::string id,
    content::URLDataSource::GotDataCallback callback) {
  DCHECK(sequence_task_runner_->RunsTasksInCurrentSequence());

  // It is not an error if id is not in id_to_file_map_. The IO thread may not
  // be aware yet that the id was removed when it called this.
  scoped_refptr<base::RefCountedMemory> data;

  auto it = id_to_file_map_.find(id);
  if (it != id_to_file_map_.end()) {
    base::FilePath file_path = it->second.GetFilePath();
    if (!file_path.empty()) {
      if (!file_path.IsAbsolute()) {
        file_path = user_data_dir_.Append(file_path);
      }
      data = ReadFileOnBlockingThread(file_path);
    }
  }

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&VivaldiDataSourcesAPI::FinishGetDataForIdOnIOThread, this,
                     std::move(id), std::move(data), std::move(callback)));
}

void VivaldiDataSourcesAPI::FinishGetDataForIdOnIOThread(
    std::string id,
    scoped_refptr<base::RefCountedMemory> data,
    content::URLDataSource::GotDataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (data) {
    SetCacheOnIOThread(std::move(id), data);
  }
  callback.Run(std::move(data));
}

// static
void VivaldiDataSourcesAPI::AddImageDataForBookmark(
    content::BrowserContext* browser_context,
    int64_t bookmark_id,
    scoped_refptr<base::RefCountedMemory> png_data,
    AddBookmarkImageCallback callback) {
  DCHECK(png_data->size() > 0);
  VivaldiDataSourcesAPI* api = FromBrowserContext(browser_context);
  DCHECK(api);
  if (!api) {
    std::move(callback).Run(false);
    return;
  }
  api->AddImageDataForBookmark(bookmark_id, std::move(png_data),
                               std::move(callback));
}

void VivaldiDataSourcesAPI::AddImageDataForBookmark(
    int64_t bookmark_id,
    scoped_refptr<base::RefCountedMemory> png_data,
    AddBookmarkImageCallback ui_thread_callback) {
  DCHECK(png_data->size() > 0);
  sequence_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VivaldiDataSourcesAPI::AddImageDataForBookmarkOnFileThread, this,
          bookmark_id, std::move(png_data), std::move(ui_thread_callback)));
}

void VivaldiDataSourcesAPI::AddImageDataForBookmarkOnFileThread(
    int64_t bookmark_id,
    scoped_refptr<base::RefCountedMemory> png_data,
    AddBookmarkImageCallback ui_thread_callback) {
  DCHECK(sequence_task_runner_->RunsTasksInCurrentSequence());

  bool success = false;
  base::FilePath path = GetBookmarkThumbnailPath(bookmark_id);
  base::FilePath dir = path.DirName();
  if (!base::DirectoryExists(dir)) {
    LOG(INFO) << "Creating thumbnail directory: " << path.value();
    base::CreateDirectory(dir);
  }
  // The caller must ensure that data fit 2G.
  int bytes = base::WriteFile(path, png_data->front_as<char>(),
                              static_cast<int>(png_data->size()));
  if (bytes < 0 || static_cast<size_t>(bytes) != png_data->size()) {
    LOG(ERROR) << "Error writing to file: " << path.value();
  } else {
    // We use the relative path in the mapping file.
    base::FilePath relative_path =
        base::FilePath(kThumbnailDirectory).Append(path.BaseName());

    std::string id = std::to_string(bookmark_id);

    // If the old mapping for bookmark captured thumbnail exists, it should
    // match the new one.  But the file may be editted etc. so we do not check
    // and always overwrite it.
    id_to_file_map_[id] = DataSourceItem(std::move(relative_path));
    success = true;

    // Populate the cache
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(&VivaldiDataSourcesAPI::SetCacheOnIOThread, this,
                       std::move(id), std::move(png_data)));
  }

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(std::move(ui_thread_callback), success));

  SaveMappingsOnFileThread();
}

// static
void VivaldiDataSourcesAPI::InitFactory() {
  VivaldiDataSourcesAPIHolder::GetFactoryInstance();
}

VivaldiDataSourcesAPI* VivaldiDataSourcesAPI::FromBrowserContext(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  VivaldiDataSourcesAPIHolder* holder =
      VivaldiDataSourcesAPIHolder::GetFactoryInstance()->Get(browser_context);
  if (!holder)
    return nullptr;
  return holder->api_.get();
}

VivaldiDataSourcesAPIHolder::VivaldiDataSourcesAPIHolder(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  api_ = base::MakeRefCounted<VivaldiDataSourcesAPI>(profile);
  api_->LoadMappings();

  PrefService* pref_service = profile->GetPrefs();
  pref_change_registrar_.Init(pref_service);
  for (int i = 0; i < VivaldiDataSourcesAPI::kDataMappingPrefsCount; ++i) {
    profile_path_urls_[i] =
        pref_service->GetString(VivaldiDataSourcesAPI::kDataMappingPrefs[i]);
  }
  for (int i = 0; i < VivaldiDataSourcesAPI::kDataMappingPrefsCount; ++i) {
    // base::Unretained() is safe as this class owns pref_change_registrar_
    pref_change_registrar_.Add(
        VivaldiDataSourcesAPI::kDataMappingPrefs[i],
        base::BindRepeating(&VivaldiDataSourcesAPIHolder::OnPrefChange,
                             base::Unretained(this), i));
  }
}

VivaldiDataSourcesAPIHolder::~VivaldiDataSourcesAPIHolder() {}

// static
BrowserContextKeyedAPIFactory<VivaldiDataSourcesAPIHolder>*
VivaldiDataSourcesAPIHolder::GetFactoryInstance() {
  static base::LazyInstance<BrowserContextKeyedAPIFactory<
      VivaldiDataSourcesAPIHolder>>::DestructorAtExit factory =
      LAZY_INSTANCE_INITIALIZER;

  return factory.Pointer();
}

void VivaldiDataSourcesAPIHolder::Shutdown() {
  // Prevent farther access to api_ from UI thread. Note that if it can still
  // be used on IO or worker threads.
  api_->profile_ = nullptr;
  api_.reset();
  pref_change_registrar_.RemoveAll();
}

void VivaldiDataSourcesAPIHolder::OnPrefChange(int pref_index,
                                               const std::string& name) {
  DCHECK(0 <= pref_index &&
         pref_index < VivaldiDataSourcesAPI::kDataMappingPrefsCount);
  PrefService* pref_service = pref_change_registrar_.prefs();
  std::string old_url = std::move(profile_path_urls_[pref_index]);
  std::string new_url = pref_service->GetString(name);
  api_->OnUrlChange(old_url, new_url);
  profile_path_urls_[pref_index] = std::move(new_url);
}

}  // namespace extensions
