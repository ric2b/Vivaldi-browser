// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "components/datasource/vivaldi_data_source_api.h"

#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_filter.h"
#include "content/public/browser/browser_thread.h"

const char kDatasourceFilemappingFilename[] = "file_mapping.json";

namespace extensions {

VivaldiDataSourceItem::VivaldiDataSourceItem(const std::string& id,
                                             const base::FilePath& path)
    : file_path_(path), mapping_id_(id) {
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
  base::FilePath path;

  Profile* profile = Profile::FromBrowserContext(browser_context_);
  path = profile->GetPath();
  path = path.AppendASCII(kDatasourceFilemappingFilename);

  store_ = new JsonPrefStore(path,
                            base::CreateSequencedTaskRunnerWithTraits(
                              { base::MayBlock() }).get(),
                             std::unique_ptr<PrefFilter>());
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
          if (values_it.key() == "local_path") {
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
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  std::unique_ptr<base::DictionaryValue> root(new base::DictionaryValue);
  std::unique_ptr<base::DictionaryValue> item(new base::DictionaryValue);

  base::AutoLock lock(map_lock_);
  for (auto it : id_to_file_map_) {
    std::unique_ptr<base::DictionaryValue> subitems(new base::DictionaryValue);
    base::FilePath path = it.second->GetPath();

    subitems->SetString("local_path", path.value());

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
  content::BrowserThread::PostTask(
    content::BrowserThread::IO, FROM_HERE,
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

    content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&VivaldiDataSourcesAPI::SaveMappings, base::Unretained(this)));

    return true;
  }
  return false;
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
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&VivaldiDataSourcesAPI::GetDataForIdOnFileThread,
                 base::Unretained(this), id, callback, thread_id));
}

void VivaldiDataSourcesAPI::GetDataForIdOnFileThread(
    const std::string& id,
    const content::URLDataSource::GotDataCallback& callback,
    content::BrowserThread::ID thread_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

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
      base::File file(f, base::File::FLAG_READ | base::File::FLAG_OPEN);
      int64_t len = file.GetLength();
      if (len > 0) {
        std::unique_ptr<char[]> buffer(new char[len]);
        int read_len = file.Read(0, buffer.get(), len);
        if (read_len == len) {
          data = new base::RefCountedBytes(
              reinterpret_cast<unsigned char*>(buffer.get()), (size_t)len);
          it->second->SetCachedData(data);
        }
      }
    }
  }
  content::BrowserThread::PostTask(
      thread_id, FROM_HERE,
      base::Bind(&VivaldiDataSourcesAPI::PostResultsOnThread,
                 base::Unretained(this), callback, data));
}

void VivaldiDataSourcesAPI::PostResultsOnThread(
    const content::URLDataSource::GotDataCallback& callback,
    scoped_refptr<base::RefCountedMemory> data) {
  callback.Run(data);
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
