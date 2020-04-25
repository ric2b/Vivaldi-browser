// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "components/datasource/vivaldi_data_source_api.h"

#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/common/api/extension_types.h"
#include "ui/vivaldi_ui_utils.h"

const char kDatasourceFilemappingFilename[] = "file_mapping.json";
const base::FilePath::StringPieceType kThumbnailDirectory =
    FILE_PATH_LITERAL("VivaldiThumbnails");

using extensions::api::extension_types::ImageFormat;
using vivaldi::ui_tools::EncodeBitmap;

namespace extensions {

struct VivaldiDataSourceItem {
  explicit VivaldiDataSourceItem(const base::FilePath& file_path);
  ~VivaldiDataSourceItem();

  // The file on disk.
  const base::FilePath file_path;

  // The cached image data.
  scoped_refptr<base::RefCountedMemory> cached_image_data;
};

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

  scoped_refptr<VivaldiDataSourcesAPI> api_;
};

VivaldiDataSourceItem::VivaldiDataSourceItem(const base::FilePath& file_path)
    : file_path(file_path) {}

VivaldiDataSourceItem::~VivaldiDataSourceItem() {}

VivaldiDataSourcesAPI::VivaldiDataSourcesAPI(base::FilePath user_data_dir)
    : user_data_dir_(user_data_dir),
      id_to_file_map_(LoadMappings(user_data_dir)) {}

VivaldiDataSourcesAPI::~VivaldiDataSourcesAPI() {}

// static
scoped_refptr<base::RefCountedMemory>
VivaldiDataSourcesAPI::ReadFileOnBlockingThread(
    const base::FilePath& file_path) {
  base::File file(file_path, base::File::FLAG_READ | base::File::FLAG_OPEN);
  if (file.IsValid()) {
    int64_t len = file.GetLength();
    if (len > 0) {
      std::vector<unsigned char> buffer(len);
      int read_len =
          file.Read(0, reinterpret_cast<char*>(buffer.data()), len);
      if (read_len == len) {
        return base::RefCountedBytes::TakeVector(&buffer);
      }
    }
  }
  return scoped_refptr<base::RefCountedMemory>();
}

// static
VivaldiDataSourcesAPI::IdFileMap VivaldiDataSourcesAPI::LoadMappings(
    const base::FilePath& user_data_dir) {
  // This might be called outside the startup, eg. during creation of a guest
  // window, so need to allow IO.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  base::FilePath path =
      user_data_dir.AppendASCII(kDatasourceFilemappingFilename);

  // TODO(igor@vivaldi.com): use JsonReader as the store is only used to read
  // the data.
  scoped_refptr<JsonPrefStore> store = base::MakeRefCounted<JsonPrefStore>(
      path, std::unique_ptr<PrefFilter>(),
      base::CreateSequencedTaskRunnerWithTraits({base::MayBlock()}).get());

  store->ReadPrefs();

  const base::Value* mappings_value;
  if (store->GetValue("mappings", &mappings_value)) {
    const base::DictionaryValue* dict;
    if (mappings_value->GetAsDictionary(&dict))
      return GetMappings(dict);
  }
  return IdFileMap();
}

// static
VivaldiDataSourcesAPI::IdFileMap VivaldiDataSourcesAPI::GetMappings(
    const base::DictionaryValue* dict) {
  IdFileMap id_to_file_map;
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
              auto entry = std::make_unique<VivaldiDataSourceItem>(path);
              id_to_file_map.insert(std::make_pair(id, std::move(entry)));
            }
          }
        }
      }
    }
  }
  return id_to_file_map;
}

void VivaldiDataSourcesAPI::ScheduleSaveMappings() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::PostTaskWithTraits(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&VivaldiDataSourcesAPI::SaveMappingsOnFileThread,
                     this));
}

void VivaldiDataSourcesAPI::SaveMappingsOnFileThread() {
  auto item = std::make_unique<base::DictionaryValue>();

  {
    base::AutoLock lock(lock_);
    for (auto& it : id_to_file_map_) {
      const base::FilePath& path = it.second->file_path;
      auto subitems = std::make_unique<base::DictionaryValue>();
      if (path.IsAbsolute()) {
        subitems->SetString("local_path", path.value());
      } else {
        subitems->SetString("relative_path", path.value());
      }
      item->Set(it.first, std::move(subitems));
    }
  }

  std::unique_ptr<base::DictionaryValue> root(new base::DictionaryValue);
  root->Set("mappings", std::move(item));

  base::FilePath path =
      user_data_dir_.AppendASCII(kDatasourceFilemappingFilename);
  base::File file(path,
                  base::File::FLAG_WRITE | base::File::FLAG_CREATE_ALWAYS);
  if (!file.IsValid()) {
    NOTREACHED();
    return;
  }
  std::string json;
  base::JSONWriter::WriteWithOptions(
      *root, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  if (!json.empty()) {
    file.Write(0, json.c_str(), json.length());
  }
  file.Close();
}

// static
bool VivaldiDataSourcesAPI::AddMapping(content::BrowserContext* browser_context,
                                       const std::string& id,
                                       const base::FilePath& file_path) {
  VivaldiDataSourcesAPI* api = FromBrowserContext(browser_context);
  DCHECK(api);
  if (!api)
    return false;

  {
    base::AutoLock lock(api->lock_);

    // Should not be previously registered.
    auto it = api->id_to_file_map_.find(id);
    if (it != api->id_to_file_map_.end()) {
      // Remove mapping first.
      return false;
    }
    api->id_to_file_map_.insert(std::make_pair(
        id, std::make_unique<VivaldiDataSourceItem>(file_path)));
  }

  api->ScheduleSaveMappings();
  return true;
}

// static
bool VivaldiDataSourcesAPI::RemoveMapping(
    content::BrowserContext* browser_context,
    const std::string& id) {
  VivaldiDataSourcesAPI* api = FromBrowserContext(browser_context);
  DCHECK(api);
  if (!api)
    return false;

  {
    base::AutoLock lock(api->lock_);

    auto it = api->id_to_file_map_.find(id);
    if (it == api->id_to_file_map_.end())
      return false;
    api->id_to_file_map_.erase(it);
  }

  api->ScheduleSaveMappings();
  return true;
}

// This method can be called on any thread and will call the callback on the
// same thread.
// static
void VivaldiDataSourcesAPI::GetDataForId(
    const std::string& id,
    const content::URLDataSource::GotDataCallback& callback) {

  scoped_refptr<base::RefCountedMemory> data;
  bool should_read_data = false;

  // This must be a value, not a reference, as we store the value outside the
  // lock.
  base::FilePath file_path;

  // Accessed from multiple threads.
  {
    base::AutoLock lock(lock_);
    auto it = id_to_file_map_.find(id);
    if (it != id_to_file_map_.end()) {
      if (it->second->cached_image_data) {
        data = it->second->cached_image_data;
      } else {
        should_read_data = true;
        file_path = it->second->file_path;
      }
    }
  }

  if (!should_read_data) {
    callback.Run(std::move(data));
    return;
  }

  base::FilePath full_path =
      file_path.IsAbsolute() ? file_path : user_data_dir_.Append(file_path);

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&VivaldiDataSourcesAPI::ReadFileOnBlockingThread,
                     full_path),
      base::BindOnce(&VivaldiDataSourcesAPI::FinishGetDataForId, this, id,
                     file_path, callback));
}

void VivaldiDataSourcesAPI::FinishGetDataForId(
    const std::string& id,
    const base::FilePath& file_path,
    const content::URLDataSource::GotDataCallback& callback,
    scoped_refptr<base::RefCountedMemory> data) {
  // Cache the data unless the mapping was already removed or modified.
  bool valid = false;
  if (data) {
    base::AutoLock lock(lock_);
    auto it = id_to_file_map_.find(id);
    if (it != id_to_file_map_.end() && it->second->file_path == file_path) {
      it->second->cached_image_data = data;
      valid = true;
    }
  }
  if (!valid) {
    data.reset();
  }
  callback.Run(std::move(data));
}

// static
void VivaldiDataSourcesAPI::AddImageDataForBookmark(
    content::BrowserContext* browser_context,
    int bookmark_id,
    std::unique_ptr<SkBitmap> bitmap,
    AddBookmarkImageCallback callback) {
  VivaldiDataSourcesAPI* api = FromBrowserContext(browser_context);
  DCHECK(api);
  if (!api)
    return;

  base::PostTaskWithTraits(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          &VivaldiDataSourcesAPI::AddImageDataForBookmarkOnFileThread,
          api, bookmark_id, std::move(bitmap),
          std::move(callback), content::BrowserThread::UI));
}

// static
void VivaldiDataSourcesAPI::AddImageDataForBookmark(
    content::BrowserContext* browser_context,
    int bookmark_id,
    scoped_refptr<base::RefCountedMemory> png_data,
    AddBookmarkImageCallback callback) {
  VivaldiDataSourcesAPI* api = FromBrowserContext(browser_context);
  DCHECK(api);
  if (!api)
    return;

  base::PostTaskWithTraits(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          &VivaldiDataSourcesAPI::AddRawImageDataForBookmarkOnFileThread,
          api, bookmark_id, std::move(png_data),
          std::move(callback), content::BrowserThread::UI));
}

void VivaldiDataSourcesAPI::AddRawImageDataForBookmarkOnFileThread(
    int bookmark_id,
    scoped_refptr<base::RefCountedMemory> png_data,
    AddBookmarkImageCallback callback,
    content::BrowserThread::ID thread_id) {
  base::FilePath path = user_data_dir_.Append(kThumbnailDirectory);

  if (!base::DirectoryExists(path)) {
    LOG(INFO) << "Creating thumbnail directory: " << path.value();
    base::CreateDirectory(path);
  }
  std::string filename = base::StringPrintf("%d.png", bookmark_id);
#if defined(OS_POSIX)
  path = path.Append(filename);
#elif defined(OS_WIN)
  path = path.Append(base::UTF8ToUTF16(filename));
#endif
  size_t bytes = base::WriteFile(
      path, reinterpret_cast<const char*>(png_data->front()), png_data->size());
  if (bytes < 0 || bytes != png_data->size()) {
    LOG(ERROR) << "Error writing to file: " << path.value();
  }
  // We use the relative path in the mapping file.
  base::FilePath relative_path = base::FilePath(kThumbnailDirectory);
#if defined(OS_POSIX)
  relative_path = relative_path.Append(filename);
#elif defined(OS_WIN)
  relative_path = relative_path.Append(base::UTF8ToUTF16(filename));
#endif

  std::string id = base::StringPrintf("%d", bookmark_id);
  auto item = std::make_unique<VivaldiDataSourceItem>(relative_path);
  // TODO(igor@vivaldi.com): consider initializing item->cached_image_data
  // with written data.
  {
    base::AutoLock lock(lock_);

    // We silently overwrite any old mapping for this bookmark.
    id_to_file_map_[id] = std::move(item);
  }

  base::PostTaskWithTraits(
      FROM_HERE, {thread_id},
      base::BindOnce(
          &VivaldiDataSourcesAPI::PostAddBookmarkImageResultsOnThread,
          std::move(callback), bookmark_id));

  SaveMappingsOnFileThread();
}

void VivaldiDataSourcesAPI::AddImageDataForBookmarkOnFileThread(
    int bookmark_id,
    std::unique_ptr<SkBitmap> bitmap,
    AddBookmarkImageCallback callback,
    content::BrowserThread::ID thread_id) {
  std::vector<unsigned char> data;
  std::string mime_type;

  bool encoded = EncodeBitmap(
      *bitmap, &data, &mime_type, ImageFormat::IMAGE_FORMAT_PNG,
      gfx::Size(bitmap->width(), bitmap->height()), 100, 100, false);
  if (!encoded) {
    LOG(ERROR) << "Error encoding image data to png";
    return;
  }
  scoped_refptr<base::RefCountedMemory> thumbnail =
      base::RefCountedBytes::TakeVector(&data);

  AddRawImageDataForBookmarkOnFileThread(bookmark_id, std::move(thumbnail),
                                         std::move(callback), thread_id);
}

// static
void VivaldiDataSourcesAPI::PostAddBookmarkImageResultsOnThread(
    AddBookmarkImageCallback callback,
    int bookmark_id) {
  std::string image_url =
      base::StringPrintf("chrome://vivaldi-data/local-image/%d", bookmark_id);

  std::move(callback).Run(bookmark_id, image_url);
}

// static
bool VivaldiDataSourcesAPI::HasBookmarkThumbnail(
    content::BrowserContext* browser_context,
    int bookmark_id) {
  VivaldiDataSourcesAPI* api = FromBrowserContext(browser_context);
  DCHECK(api);
  if (!api)
    return false;

  if (bookmark_id) {
    std::string id = base::StringPrintf("%d", bookmark_id);

    // We only check the mapping, not if the actual file is on disk.
    base::AutoLock lock(api->lock_);
    return api->id_to_file_map_.find(id) != api->id_to_file_map_.end();
  }
  return false;
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
  api_ = base::MakeRefCounted<VivaldiDataSourcesAPI>(profile->GetPath());
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

}  // namespace extensions
