// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "components/datasource/vivaldi_data_source_api.h"

#include "app/vivaldi_constants.h"
#include "base/containers/flat_set.h"
#include "base/guid.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread_restrictions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/vivaldi_bookmark_kit.h"
#include "components/capture/thumbnail_capture_contents.h"
#include "components/datasource/vivaldi_data_url_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/json_pref_store.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "prefs/vivaldi_gen_prefs.h"
#include "ui/base/models/tree_node_iterator.h"

const char kDatasourceFilemappingFilename[] = "file_mapping.json";
const char kDatasourceFilemappingTmpFilename[] = "file_mapping.tmp";
const base::FilePath::StringPieceType kThumbnailDirectory =
    FILE_PATH_LITERAL("VivaldiThumbnails");

namespace {

// Size of bookmark thumbnails. This must stay in sync with ThumbnailService.js.
constexpr int kBookmarkThumbnailWidth = 440;
constexpr int kBookmarkThumbnailHeight = 360;

// Size of offscreen window for bookmark thumbnail capture.
constexpr int kOffscreenWindowWidth  = 1024;
constexpr int kOffscreenWindowHeight = 838;

// Delay to check for no longer used data url after initialization when the
// browser is likely idle.
constexpr base::TimeDelta kDataUrlGCStartupDelay =
    base::TimeDelta::FromSeconds(60);

}  // namespace

// Helper to store ref-counted VivaldiDataSourcesAPI in BrowserContext.
class VivaldiDataSourcesAPIHolder : public KeyedService {
 public:
  explicit VivaldiDataSourcesAPIHolder(content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    api_ = base::MakeRefCounted<VivaldiDataSourcesAPI>(profile);
    api_->Start();
  }

  ~VivaldiDataSourcesAPIHolder() override = default;

 private:
  // KeyedService
  void Shutdown() override {
    // Prevent further access to api_ from UI thread. Note that it can still
    // be used on worker threads.
    api_->profile_ = nullptr;
    api_.reset();
  }

 public:
  scoped_refptr<VivaldiDataSourcesAPI> api_;
};

namespace {

class VivaldiDataSourcesAPIFactory : public BrowserContextKeyedServiceFactory {
 public:
  static VivaldiDataSourcesAPIHolder* GetForBrowserContext(
      content::BrowserContext* context) {
    return static_cast<VivaldiDataSourcesAPIHolder*>(
        GetInstance()->GetServiceForBrowserContext(context, true));
  }

  static VivaldiDataSourcesAPIFactory* GetInstance() {
    return base::Singleton<VivaldiDataSourcesAPIFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<VivaldiDataSourcesAPIFactory>;

  VivaldiDataSourcesAPIFactory()
      : BrowserContextKeyedServiceFactory(
            "VivaldiDataSourcesAPI",
            BrowserContextDependencyManager::GetInstance()) {}
  ~VivaldiDataSourcesAPIFactory() override = default;

  // BrowserContextKeyedServiceFactory:

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* browser_context) const override {
    return chrome::GetBrowserContextRedirectedInIncognito(browser_context);
  }

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const override {
    return new VivaldiDataSourcesAPIHolder(browser_context);
  }
};

}  //namespace

const char* VivaldiDataSourcesAPI::kDataMappingPrefs[] = {
    vivaldiprefs::kThemeBackgroundUserImage,
};

namespace {

bool ParseDataUrl(base::StringPiece url,
                  VivaldiDataSourcesAPI::UrlKind* url_kind,
                  std::string* id) {
  using PathType = vivaldi_data_url_utils::PathType;

  absl::optional<PathType> type = vivaldi_data_url_utils::ParseUrl(url, id);
  if (type == PathType::kThumbnail) {
    *url_kind = VivaldiDataSourcesAPI::THUMBNAIL_URL;
    return true;
  }
  if (type == PathType::kLocalPath) {
    *url_kind = VivaldiDataSourcesAPI::PATH_MAPPING_URL;
    return true;
  }
  return false;
}

}  // namespace

VivaldiDataSourcesAPI::VivaldiDataSourcesAPI(Profile* profile)
    : profile_(profile),
      user_data_dir_(profile->GetPath()),
      ui_thread_runner_(content::GetUIThreadTaskRunner(
          {base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      sequence_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_VISIBLE,
           base::MayBlock()})) {}

VivaldiDataSourcesAPI::~VivaldiDataSourcesAPI() {}

// static
int VivaldiDataSourcesAPI::FindMappingPreference(base::StringPiece preference) {
  const char* const* i = std::find(std::cbegin(kDataMappingPrefs),
                                   std::cend(kDataMappingPrefs), preference);
  if (i == std::cend(kDataMappingPrefs))
    return -1;
  return static_cast<int>(i - kDataMappingPrefs);
}

void VivaldiDataSourcesAPI::Start() {
  sequence_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VivaldiDataSourcesAPI::LoadMappingsOnFileThread, this));

  // Inline ScheduleRemovalOfUnusedUrlData here as it uses FromBrowserContext()
  // but that can not be used when the factory initializes the instance.
  ui_thread_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&VivaldiDataSourcesAPI::FindUsedUrlsOnUIThread, this),
      kDataUrlGCStartupDelay);
}

void VivaldiDataSourcesAPI::LoadMappingsOnFileThread() {
  DCHECK(sequence_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(path_id_map_.empty());

  base::FilePath file_path = GetFileMappingFilePath();
  std::vector<unsigned char> data;
  if (!vivaldi_data_url_utils::ReadFileOnBlockingThread(file_path, &data))
    return;

  base::JSONReader::ValueWithError root =
      base::JSONReader::ReadAndReturnValueWithError(
      base::StringPiece(reinterpret_cast<char*>(data.data()), data.size()));
  if (!root.value) {
    LOG(ERROR) << file_path.value() << " is not a valid JSON - "
               << root.error_message;
    return;
  }

  if (root.value->is_dict()) {
    if (const base::Value* mappings_value = root.value->FindDictKey("mappings")) {
      InitMappingsOnFileThread(
          static_cast<const base::DictionaryValue*>(mappings_value));
    }
  }
}

void VivaldiDataSourcesAPI::InitMappingsOnFileThread(
    const base::DictionaryValue* dict) {
  DCHECK(sequence_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(path_id_map_.empty());

  for (const auto& i : dict->DictItems()) {
    const std::string& id = i.first;
    if (vivaldi_data_url_utils::isOldFormatThumbnailId(id)) {
      // Older mapping entry that we just skip as we know the path statically.
      continue;
    }
    const base::Value& value = i.second;
    if (value.is_dict()) {
      const std::string* path_string = value.FindStringKey("local_path");
      if (!path_string) {
        // Older format support.
        path_string = value.FindStringKey("relative_path");
      }
      if (path_string) {
#if defined(OS_POSIX)
        base::FilePath path(*path_string);
#elif defined(OS_WIN)
        base::FilePath path(base::UTF8ToWide(*path_string));
#endif
        path_id_map_.emplace(id, std::move(path));
        continue;
      }
    }
    LOG(WARNING) << "Invalid entry " << id << " in \""
                 << kDatasourceFilemappingFilename << "\" file.";
  }
}

std::string VivaldiDataSourcesAPI::GetMappingJSONOnFileThread() {
  DCHECK(sequence_task_runner_->RunsTasksInCurrentSequence());

  // TODO(igor@vivaldi.com): Write the mapping file even if there are no
  // entries. This allows in future to write a URL format converter for
  // bookamrks and a add a version field to the file. Then presence of the file
  // without the version string will indicate the need for converssion.

  std::vector<base::Value::DictStorage::value_type> items;
  for (const auto& it : path_id_map_) {
    const base::FilePath& path = it.second;
    base::Value item(base::Value::Type::DICTIONARY);
    item.SetStringKey("local_path", path.AsUTF16Unsafe());
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

  std::string json = GetMappingJSONOnFileThread();
  base::FilePath path = GetFileMappingFilePath();
  if (json.empty()) {
    if (!base::DeleteFile(path)) {
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

base::FilePath VivaldiDataSourcesAPI::GetFileMappingFilePath() {
  return user_data_dir_.AppendASCII(kDatasourceFilemappingFilename);
}

base::FilePath VivaldiDataSourcesAPI::GetThumbnailPath(
    base::StringPiece thumbnail_id) {
  base::FilePath path = user_data_dir_.Append(kThumbnailDirectory);
#if defined(OS_POSIX)
  path = path.Append(thumbnail_id);
#elif defined(OS_WIN)
  path = path.Append(base::UTF8ToWide(thumbnail_id));
#endif
  return path;
}

// static
void VivaldiDataSourcesAPI::ScheduleRemovalOfUnusedUrlData(
    content::BrowserContext* browser_context,
    base::TimeDelta when) {
  VivaldiDataSourcesAPI* api = FromBrowserContext(browser_context);
  DCHECK(api);
  if (!api) {
    return;
  }

  api->ui_thread_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&VivaldiDataSourcesAPI::FindUsedUrlsOnUIThread, api),
      when);
}

void VivaldiDataSourcesAPI::FindUsedUrlsOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  vivaldi_bookmark_kit::RunAfterModelLoad(
      GetBookmarkModel(),
      base::BindOnce(
          &VivaldiDataSourcesAPI::FindUsedUrlsOnUIThreadWithLoadedBookmaks,
          this));
}

void VivaldiDataSourcesAPI::FindUsedUrlsOnUIThreadWithLoadedBookmaks(
    bookmarks::BookmarkModel* bookmark_model) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!profile_ || !bookmark_model)
    return;

  UrlKind url_kind;
  std::string id;
  UsedIds used_ids;

  // Find all data url ids in bookmarks.
  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      bookmark_model->root_node());
  while (iterator.has_next()) {
    const bookmarks::BookmarkNode* node = iterator.Next();
    std::string thumbnail = vivaldi_bookmark_kit::GetThumbnail(node);
    if (ParseDataUrl(thumbnail, &url_kind, &id)) {
      used_ids[url_kind].push_back(std::move(id));
    }
  }

  // Find data url ids in preferences.
  PrefService* pref_service = profile_->GetPrefs();
  for (int i = 0; i < VivaldiDataSourcesAPI::kDataMappingPrefsCount; ++i) {
    std::string preference_value =
        pref_service->GetString(VivaldiDataSourcesAPI::kDataMappingPrefs[i]);
    if (ParseDataUrl(preference_value, &url_kind, &id)) {
      used_ids[url_kind].push_back(std::move(id));
    }
  }

  sequence_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VivaldiDataSourcesAPI::RemoveUnusedUrlDataOnFileThread,
                     this, std::move(used_ids)));
}

void VivaldiDataSourcesAPI::RemoveUnusedUrlDataOnFileThread(UsedIds used_ids) {
  static_assert(kUrlKindCount == 2, "The code supports 2 url kinds");
  DCHECK(sequence_task_runner_->RunsTasksInCurrentSequence());

  // Add newly allocated ids that have not been stored in bookmarks or
  // preferences yet.
  for (int i = 0; i < kUrlKindCount; ++i) {
    const std::vector<std::string>* v = &file_thread_newborn_ids_[i];
    used_ids[i].insert(used_ids[i].end(), v->cbegin(), v->cend());
  }

  base::flat_set<std::string> used_path_mapping_set(
      std::move(used_ids[PATH_MAPPING_URL]));

  size_t removed_path_mappings = 0;
  for (auto i = path_id_map_.begin(); i != path_id_map_.end();) {
    // Update the iterator before the following erase call.
    auto current = i;
    i++;
    if (!used_path_mapping_set.contains(current->first)) {
      path_id_map_.erase(current);
      removed_path_mappings++;
    }
  }
  if (removed_path_mappings) {
    LOG(INFO) << removed_path_mappings
              << " unused local path mappings were removed";
    SaveMappingsOnFileThread();
  }

  base::flat_set<std::string> used_thumbnail_set(
      std::move(used_ids[THUMBNAIL_URL]));
  base::FileEnumerator files(user_data_dir_.Append(kThumbnailDirectory), false,
                             base::FileEnumerator::FILES);
  size_t removed_thumbnails = 0;
  for (base::FilePath path = files.Next(); !path.empty(); path = files.Next()) {
    base::FilePath base_name = path.BaseName();
    std::string id = base_name.AsUTF8Unsafe();
    if (!used_thumbnail_set.contains(id)) {
      if (!base::DeleteFile(path)) {
        LOG(WARNING) << "Failed to remove thumbnail file " << path;
      }
      removed_thumbnails++;
    }
  }
  if (removed_thumbnails) {
    LOG(INFO) << removed_thumbnails << " unused thumbnail files were removed";
  }
}

void VivaldiDataSourcesAPI::AddNewbornIdOnFileThread(UrlKind url_kind,
                                                     const std::string& id) {
  DCHECK(sequence_task_runner_->RunsTasksInCurrentSequence());
  file_thread_newborn_ids_[url_kind].push_back(id);
}

void VivaldiDataSourcesAPI::ForgetNewbornIdOnFileThread(UrlKind url_kind,
                                                        const std::string& id) {
  DCHECK(sequence_task_runner_->RunsTasksInCurrentSequence());
  std::vector<std::string>* v = &file_thread_newborn_ids_[url_kind];
  auto i = std::find(v->begin(), v->end(), id);
  if (i != v->end()) {
    v->erase(i);
  } else {
    // This should only be called for active ids.
    NOTREACHED() << url_kind << " " << id;
  }
}

// static
void VivaldiDataSourcesAPI::UpdateMapping(
    content::BrowserContext* browser_context,
    int64_t bookmark_id,
    int preference_index,
    base::FilePath file_path,
    UpdateMappingCallback callback) {
  // Exactly one of bookmarkId, preference_index must be given.
  DCHECK_NE(bookmark_id == 0, preference_index == -1);
  DCHECK(0 <= bookmark_id);
  DCHECK(-1 <= preference_index || preference_index < kDataMappingPrefsCount);
  VivaldiDataSourcesAPI* api = FromBrowserContext(browser_context);
  DCHECK(api);
  if (!api) {
    std::move(callback).Run(false);
    return;
  }

  api->sequence_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VivaldiDataSourcesAPI::UpdateMappingOnFileThread, api,
                     bookmark_id, preference_index, std::move(file_path),
                     std::move(callback)));
}

void VivaldiDataSourcesAPI::UpdateMappingOnFileThread(
    int64_t bookmark_id,
    int preference_index,
    base::FilePath file_path,
    UpdateMappingCallback callback) {
  DCHECK(sequence_task_runner_->RunsTasksInCurrentSequence());

  std::string path_id = base::GenerateGUID();
  AddNewbornIdOnFileThread(PATH_MAPPING_URL, path_id);

  path_id_map_.emplace(path_id, std::move(file_path));

  ui_thread_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VivaldiDataSourcesAPI::FinishUpdateMappingOnUIThread,
                     this, bookmark_id, preference_index, std::move(path_id),
                     std::move(callback)));
  SaveMappingsOnFileThread();
}

void VivaldiDataSourcesAPI::FinishUpdateMappingOnUIThread(
    int64_t bookmark_id,
    int preference_index,
    std::string path_id,
    UpdateMappingCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // bookmark model or profile are null on shutdown.
  bool success = false;
  std::string url = vivaldi_data_url_utils::MakeUrl(
      vivaldi_data_url_utils::PathType::kLocalPath, path_id);
  if (bookmark_id > 0) {
    if (bookmarks::BookmarkModel* bookmark_model = GetBookmarkModel()) {
      success = vivaldi_bookmark_kit::SetBookmarkThumbnail(bookmark_model,
                                                           bookmark_id, url);
    }
  } else {
    if (profile_) {
      profile_->GetPrefs()->SetString(kDataMappingPrefs[preference_index], url);
      success = true;
    }
  }
  sequence_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VivaldiDataSourcesAPI::ForgetNewbornIdOnFileThread, this,
                     PATH_MAPPING_URL, path_id));

  std::move(callback).Run(success);
}

// static
void VivaldiDataSourcesAPI::GetDataForId(
    content::BrowserContext* browser_context,
    UrlKind url_kind,
    std::string id,
    content::URLDataSource::GotDataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  VivaldiDataSourcesAPI* api = FromBrowserContext(browser_context);
  DCHECK(api);
  if (!api) {
    std::move(callback).Run(nullptr);
    return;
  }
  api->sequence_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VivaldiDataSourcesAPI::GetDataForIdOnFileThread, api,
                     url_kind, std::move(id), std::move(callback)));
}

void VivaldiDataSourcesAPI::GetDataForIdOnFileThread(
    UrlKind url_kind,
    std::string id,
    content::URLDataSource::GotDataCallback callback) {
  DCHECK(sequence_task_runner_->RunsTasksInCurrentSequence());

  base::FilePath file_path;
  if (url_kind == THUMBNAIL_URL) {
    file_path = GetThumbnailPath(id);
  } else {
    DCHECK(url_kind == PATH_MAPPING_URL);

    // It is not an error if id is not in the map. The IO thread may not
    // be aware yet that the id was removed when it called this.
    auto it = path_id_map_.find(id);
    if (it != path_id_map_.end()) {
      file_path = it->second;
      if (!file_path.IsAbsolute()) {
        file_path = user_data_dir_.Append(file_path);
      }
    }
  }

  scoped_refptr<base::RefCountedMemory> data;
  if (!file_path.empty()) {
    data = vivaldi_data_url_utils::ReadFileOnBlockingThread(file_path);
  }

  ui_thread_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(data)));
}

// static
void VivaldiDataSourcesAPI::CaptureBookmarkThumbnail(
    content::BrowserContext* browser_context,
    int64_t bookmark_id,
    const GURL& url,
    AddBookmarkImageCallback ui_thread_callback) {
  VivaldiDataSourcesAPI* api = FromBrowserContext(browser_context);
  DCHECK(api);
  if (!api) {
    std::move(ui_thread_callback).Run(false);
    return;
  }
  ::vivaldi::ThumbnailCaptureContents::Capture(
      browser_context, url,
      gfx::Size(kOffscreenWindowWidth, kOffscreenWindowHeight),
      gfx::Size(kBookmarkThumbnailWidth, kBookmarkThumbnailHeight),
      base::BindOnce(&VivaldiDataSourcesAPI::AddImageDataForBookmarkUIThread,
                     api, bookmark_id, std::move(ui_thread_callback)));
}

// static
void VivaldiDataSourcesAPI::AddImageDataForBookmark(
    content::BrowserContext* browser_context,
    int64_t bookmark_id,
    scoped_refptr<base::RefCountedMemory> png_data,
    AddBookmarkImageCallback callback) {
  VivaldiDataSourcesAPI* api = FromBrowserContext(browser_context);
  DCHECK(api);
  if (!api) {
    std::move(callback).Run(false);
    return;
  }

  api->AddImageDataForBookmarkUIThread(bookmark_id, std::move(callback),
                                       std::move(png_data));
}

void VivaldiDataSourcesAPI::AddImageDataForBookmarkUIThread(
    int64_t bookmark_id,
    AddBookmarkImageCallback ui_thread_callback,
    scoped_refptr<base::RefCountedMemory> png_data) {
  if (!png_data || !png_data->size()) {
    // Propage a capture or encoding error to the callback.
    std::move(ui_thread_callback).Run(false);
    return;
  }
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
  std::string thumbnail_id = base::GenerateGUID() + ".png";
  AddNewbornIdOnFileThread(THUMBNAIL_URL, thumbnail_id);

  base::FilePath path = GetThumbnailPath(thumbnail_id);
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
    success = true;
  }

  ui_thread_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VivaldiDataSourcesAPI::FinishAddImageDataForBookmarkOnUIThread, this,
          std::move(ui_thread_callback), success, bookmark_id,
          std::move(thumbnail_id)));
}

void VivaldiDataSourcesAPI::FinishAddImageDataForBookmarkOnUIThread(
    AddBookmarkImageCallback ui_thread_callback,
    bool success,
    int64_t bookmark_id,
    std::string thumbnail_id) {
  if (success) {
    bookmarks::BookmarkModel* bookmark_model = GetBookmarkModel();
    if (!bookmark_model) {
      // We are at shutdown.
      success = false;
    } else {
      std::string url = vivaldi_data_url_utils::MakeUrl(
          vivaldi_data_url_utils::PathType::kThumbnail, thumbnail_id);
      success = vivaldi_bookmark_kit::SetBookmarkThumbnail(bookmark_model,
                                                           bookmark_id, url);
    }
  }
  sequence_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VivaldiDataSourcesAPI::ForgetNewbornIdOnFileThread, this,
                     THUMBNAIL_URL, thumbnail_id));
  std::move(ui_thread_callback).Run(success);
}

bookmarks::BookmarkModel* VivaldiDataSourcesAPI::GetBookmarkModel() {
  if (!profile_)
    return nullptr;
  return BookmarkModelFactory::GetForBrowserContext(profile_);
}

// static
void VivaldiDataSourcesAPI::InitFactory() {
  VivaldiDataSourcesAPIFactory::GetInstance();
}

VivaldiDataSourcesAPI* VivaldiDataSourcesAPI::FromBrowserContext(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  VivaldiDataSourcesAPIHolder* holder =
      VivaldiDataSourcesAPIFactory::GetForBrowserContext(browser_context);
  if (!holder)
    return nullptr;
  return holder->api_.get();
}
