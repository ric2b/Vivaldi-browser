// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "components/datasource/vivaldi_data_source_api.h"

#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/prefs/pref_filter.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/api/extension_types.h"
#include "ui/vivaldi_ui_utils.h"

const char kDatasourceFilemappingFilename[] = "file_mapping.json";
const base::FilePath::StringPieceType kThumbnailDirectory =
    FILE_PATH_LITERAL("VivaldiThumbnails");

using extensions::api::extension_types::ImageFormat;
using vivaldi::ui_tools::EncodeBitmap;

namespace extensions {

VivaldiDataSourceItem::VivaldiDataSourceItem(const std::string& id,
                                             const base::FilePath& path)
    : file_path_(path), mapping_id_(id) {
}

VivaldiDataSourceItem::VivaldiDataSourceItem(int bookmark_id,
                                             const base::FilePath& path)
    : file_path_(path), bookmark_id_(bookmark_id) {
}

VivaldiDataSourceItem::VivaldiDataSourceItem(const std::string& id)
  : mapping_id_(id) {
}

VivaldiDataSourceItem::~VivaldiDataSourceItem() {
}

bool VivaldiDataSourceItem::HasCachedData() {
  return cached_image_data_.get() != nullptr;
}

void VivaldiDataSourceItem::SetCachedData(
    scoped_refptr<base::RefCountedMemory> data) {
  cached_image_data_ = data;
}

VivaldiDataSourcesAPI::VivaldiDataSourcesAPI(content::BrowserContext* context)
    : browser_context_(context) {
  LoadMappings();
}

VivaldiDataSourcesAPI::~VivaldiDataSourcesAPI() {
}

void VivaldiDataSourcesAPI::Shutdown() {
  for (auto it : id_to_file_map_) {
    // Get rid of the allocated items
    delete it.second;
  }
}

bool VivaldiDataSourcesAPI::LoadMappings() {
  // This might be called outside the startup, eg. during creation of a guest
  // window, so need to allow IO.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::FilePath path;

  Profile* profile = Profile::FromBrowserContext(browser_context_);
  path = profile->GetPath();
  path = path.AppendASCII(kDatasourceFilemappingFilename);

  store_ = new JsonPrefStore(
      path, std::unique_ptr<PrefFilter>(),
      base::CreateSequencedTaskRunnerWithTraits({base::MayBlock()}).get());
  store_->ReadPrefs();

  if (!GetMappings()) {
    return false;
  }
  return true;
}

bool VivaldiDataSourcesAPI::GetMappings() {
  DCHECK(store_.get());

  base::AutoLock lock(map_lock_);

  const base::Value* mappings_value;
  if (!store_->GetValue("mappings", &mappings_value))
    return false;

  const base::DictionaryValue* dict;
  if (!mappings_value->GetAsDictionary(&dict))
    return false;

  for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    const base::Value* root = NULL;
    if (!dict->GetWithoutPathExpansion(it.key(), &root)) {
      LOG(WARNING) << "Invalid entry in \"" << kDatasourceFilemappingFilename
        << "\" file.";
      continue;
    }
    std::string id(it.key());
    VivaldiDataSourceItem* entry = new VivaldiDataSourceItem(id);
    const base::DictionaryValue* value = NULL;
    if (root->GetAsDictionary(&value)) {
      for (base::DictionaryValue::Iterator values_it(*value);
           !values_it.IsAtEnd(); values_it.Advance()) {
        const base::Value* sub_value = NULL;
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
              entry->SetPath(path);
            }
            id_to_file_map_.insert(std::make_pair(id, entry));
          }
        }
      }
    }
  }
  return true;
}

void VivaldiDataSourcesAPI::SaveMappings() {
  std::unique_ptr<base::DictionaryValue> root(new base::DictionaryValue);
  std::unique_ptr<base::DictionaryValue> item(new base::DictionaryValue);

  base::AutoLock lock(map_lock_);
  for (auto it : id_to_file_map_) {
    std::unique_ptr<base::DictionaryValue> subitems(new base::DictionaryValue);
    base::FilePath path = it.second->GetPath();

    if (path.IsAbsolute()) {
      subitems->SetString("local_path", path.value());
    } else {
      subitems->SetString("relative_path", path.value());
    }
    if (it.second->bookmark_id()) {
      subitems->SetInteger("bookmark_id", it.second->bookmark_id());
    }

    item->Set(it.first, std::move(subitems));
  }
  root->Set("mappings", std::move(item));

  Profile* profile = Profile::FromBrowserContext(browser_context_);
  base::FilePath path = profile->GetPath();
  path = path.AppendASCII(kDatasourceFilemappingFilename);
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

bool VivaldiDataSourcesAPI::AddMapping(const std::string& id,
                                       const base::FilePath& file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  {
    // Accessed from multiple threads.
    base::AutoLock lock(map_lock_);

    // Should not be previously registered.
    auto it = id_to_file_map_.find(id);
    DCHECK(it == id_to_file_map_.end());
    if (it != id_to_file_map_.end()) {
      // Use UpdateMapping.
      return false;
    }
    id_to_file_map_.insert(
      std::make_pair(id, new VivaldiDataSourceItem(id, file_path)));
  }
  base::PostTaskWithTraits(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::Bind(&VivaldiDataSourcesAPI::SaveMappings, base::Unretained(this)));

  return true;
}

bool VivaldiDataSourcesAPI::AddMapping(int bookmark_id,
                                       const base::FilePath& file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  {
    // Accessed from multiple threads.
    base::AutoLock lock(map_lock_);

    std::string id = base::StringPrintf("%d", bookmark_id);

    // Should not be previously registered.
    auto it = id_to_file_map_.find(id);
    DCHECK(it == id_to_file_map_.end());
    if (it != id_to_file_map_.end() &&
        (*it).second->bookmark_id() == bookmark_id) {
      // Use UpdateMapping.
      return false;
    }
    id_to_file_map_.insert(
      std::make_pair(id, new VivaldiDataSourceItem(bookmark_id, file_path)));
  }
  base::PostTaskWithTraits(
    FROM_HERE, { base::TaskPriority::USER_VISIBLE, base::MayBlock() },
    base::Bind(&VivaldiDataSourcesAPI::SaveMappings, base::Unretained(this)));

  return true;
}

bool VivaldiDataSourcesAPI::RemoveMapping(const std::string& id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Accessed from multiple threads.
  base::AutoLock lock(map_lock_);

  auto it = id_to_file_map_.find(id);
  if (it != id_to_file_map_.end()) {
    delete it->second;
    id_to_file_map_.erase(it);

    base::PostTaskWithTraits(
      FROM_HERE, { base::TaskPriority::USER_VISIBLE, base::MayBlock() },
      base::Bind(&VivaldiDataSourcesAPI::SaveMappings, base::Unretained(this)));

    return true;
  }
  return false;
}

bool VivaldiDataSourcesAPI::RemoveMapping(int bookmark_id) {
  DCHECK_GT(bookmark_id, 0);
  std::string id = base::StringPrintf("%d", bookmark_id);

  return RemoveMapping(id);
}

// This method can be called on any thread and will call the callback on the
// same thread.
void VivaldiDataSourcesAPI::GetDataForId(
    const std::string& id,
    const content::URLDataSource::GotDataCallback& callback) {
  content::BrowserThread::ID thread_id;
  if (!content::BrowserThread::GetCurrentThreadIdentifier(&thread_id)) {
    thread_id = content::BrowserThread::IO;
  }
  base::PostTaskWithTraits(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::Bind(&VivaldiDataSourcesAPI::GetDataForIdOnFileThread,
                 base::Unretained(this), id, callback, thread_id));
}

void VivaldiDataSourcesAPI::GetDataForIdOnFileThread(
    const std::string& id,
    const content::URLDataSource::GotDataCallback& callback,
    content::BrowserThread::ID thread_id) {

  // Accessed from multiple threads.
  base::AutoLock lock(map_lock_);
  scoped_refptr<base::RefCountedMemory> data;

  auto it = id_to_file_map_.find(id);
  if (it != id_to_file_map_.end()) {
    if (it->second->HasCachedData()) {
      data = it->second->cached_data();
    } else {
      base::FilePath f(
          base::FilePath::FromUTF8Unsafe(it->second->GetPathString()));
      if (!f.IsAbsolute()) {
        Profile* profile = Profile::FromBrowserContext(browser_context_);
        base::FilePath user_data_dir = profile->GetPath();

        f = user_data_dir.Append(f);
      }
      base::File file(f, base::File::FLAG_READ | base::File::FLAG_OPEN);
      int64_t len = file.GetLength();
      if (len > 0) {
        std::unique_ptr<char[]> buffer(new char[len]);
        int read_len = file.Read(0, buffer.get(), len);
        if (read_len == len) {
          data = base::MakeRefCounted<base::RefCountedBytes>(
              reinterpret_cast<unsigned char*>(buffer.get()), (size_t)len);
          it->second->SetCachedData(data);
        }
      }
    }
  }
  base::PostTaskWithTraits(
      FROM_HERE, {thread_id},
      base::Bind(&VivaldiDataSourcesAPI::PostResultsOnThread,
                 base::Unretained(this), std::move(callback), data));
}

void VivaldiDataSourcesAPI::PostResultsOnThread(
    const content::URLDataSource::GotDataCallback& callback,
    scoped_refptr<base::RefCountedMemory> data) {
  callback.Run(data);
}

void VivaldiDataSourcesAPI::AddImageDataForBookmark(
    int bookmark_id,
    std::unique_ptr<SkBitmap> bitmap,
    AddBookmarkImageCallback callback) {
  content::BrowserThread::ID thread_id;
  if (!content::BrowserThread::GetCurrentThreadIdentifier(&thread_id)) {
    thread_id = content::BrowserThread::UI;
  }
  base::PostTaskWithTraits(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          &VivaldiDataSourcesAPI::AddImageDataForBookmarkOnFileThread,
          base::Unretained(this), bookmark_id, std::move(bitmap),
          std::move(callback), thread_id));
}

void VivaldiDataSourcesAPI::AddImageDataForBookmark(
    int bookmark_id,
    scoped_refptr<base::RefCountedMemory> png_data,
    AddBookmarkImageCallback callback) {
  content::BrowserThread::ID thread_id;
  if (!content::BrowserThread::GetCurrentThreadIdentifier(&thread_id)) {
    thread_id = content::BrowserThread::UI;
  }
  base::PostTaskWithTraits(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          &VivaldiDataSourcesAPI::AddRawImageDataForBookmarkOnFileThread,
          base::Unretained(this), bookmark_id, std::move(png_data),
          std::move(callback), thread_id));
}

void VivaldiDataSourcesAPI::AddRawImageDataForBookmarkOnFileThread(
    int bookmark_id,
    scoped_refptr<base::RefCountedMemory> png_data,
    AddBookmarkImageCallback callback,
    content::BrowserThread::ID thread_id) {
  base::FilePath path;

  Profile* profile = Profile::FromBrowserContext(browser_context_);
  path = profile->GetPath();
  path = path.Append(kThumbnailDirectory);

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

  base::PostTaskWithTraits(
    FROM_HERE, { thread_id },
    base::Bind(&VivaldiDataSourcesAPI::PostAddBookmarkImageResultsOnThread,
      base::Unretained(this), std::move(callback),
      std::move(relative_path), bookmark_id));

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

  AddRawImageDataForBookmarkOnFileThread(bookmark_id, thumbnail, callback,
                                         thread_id);
}

void VivaldiDataSourcesAPI::PostAddBookmarkImageResultsOnThread(
    AddBookmarkImageCallback callback,
    base::FilePath image_path,
    int bookmark_id) {
  // We silently remove any mapping for this bookmark.
  RemoveMapping(bookmark_id);
  if (!AddMapping(bookmark_id, image_path)) {
    LOG(ERROR) << "Error adding mapping for bookmark id: " << bookmark_id;
  }
  std::string image_url =
      base::StringPrintf("chrome://vivaldi-data/local-image/%d", bookmark_id);

  callback.Run(bookmark_id, image_url);
}

bool VivaldiDataSourcesAPI::HasBookmarkThumbnail(int bookmark_id) {
  // We only check the mapping, not if the actual file is on disk.
  base::AutoLock lock(map_lock_);

  if (bookmark_id) {
    auto it = id_to_file_map_.find(base::StringPrintf("%d", bookmark_id));
    if (it != id_to_file_map_.end()) {
      return true;
    }
  }
  return false;
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<VivaldiDataSourcesAPI>>::DestructorAtExit
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<VivaldiDataSourcesAPI>*
VivaldiDataSourcesAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

}  // namespace extensions
