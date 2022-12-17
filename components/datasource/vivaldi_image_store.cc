// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "components/datasource/vivaldi_image_store.h"

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
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
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/base32/base32.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"
#include "ui/base/models/tree_node_iterator.h"

#include "app/vivaldi_constants.h"
#include "components/bookmarks/vivaldi_bookmark_kit.h"
#include "components/capture/thumbnail_capture_contents.h"
#include "components/datasource/vivaldi_data_url_utils.h"
#include "components/datasource/vivaldi_theme_io.h"
#include "prefs/vivaldi_gen_prefs.h"

namespace {

constexpr const char* const kCanonicalExtensions[] = {
    // clang-format off
    "bmp",
    "gif",
    "jpg",
    "png",
    "webp",
    // clang-format on
};

static_assert(std::size(kCanonicalExtensions) ==
                  VivaldiImageStore::kImageFormatCount,
              "array must be in sync with ImageFormat");

constexpr const char* GetCanonicalExtension(
    VivaldiImageStore::ImageFormat format) {
  return kCanonicalExtensions[static_cast<int>(format)];
}

const char* const kAllowedMimeTypes[] = {
    // clang-format off
    "image/bmp",
    "image/gif",
    "image/jpeg",
    "image/png",
    "image/webp",
    // clang-format on
};

static_assert(std::size(kAllowedMimeTypes) ==
                  VivaldiImageStore::kImageFormatCount,
              "array must be in sync with ImageFormat");

const char kDatasourceFilemappingFilename[] = "file_mapping.json";
const char kDatasourceFilemappingTmpFilename[] = "file_mapping.tmp";

// The name is thumbnails as originally the directory stored only bookmark
// thumbnails.
const base::FilePath::StringPieceType kImageDirectory =
    FILE_PATH_LITERAL("VivaldiThumbnails");

// Size of bookmark thumbnails. This must stay in sync with ThumbnailService.js.
constexpr int kBookmarkThumbnailWidth = 440;
constexpr int kBookmarkThumbnailHeight = 360;

// Size of offscreen window for bookmark thumbnail capture.
constexpr int kOffscreenWindowWidth = 1024;
constexpr int kOffscreenWindowHeight = 838;

// Delay to check for no longer used data url after initialization when the
// browser is likely idle.
constexpr base::TimeDelta kDataUrlGCStartupDelay = base::Seconds(60);

}  // namespace

// Helper to store ref-counted VivaldiImageStore in BrowserContext.
class VivaldiImageStoreHolder : public KeyedService {
 public:
  explicit VivaldiImageStoreHolder(content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    api_ = base::MakeRefCounted<VivaldiImageStore>(profile);
    api_->Start();
  }

  ~VivaldiImageStoreHolder() override = default;

 private:
  // KeyedService
  void Shutdown() override {
    // Prevent further access to api_ from UI thread. Note that it can still
    // be used on worker threads.
    api_->profile_ = nullptr;
    api_.reset();
  }

 public:
  scoped_refptr<VivaldiImageStore> api_;
};

namespace {

class VivaldiImageStoreFactory : public BrowserContextKeyedServiceFactory {
 public:
  static VivaldiImageStoreHolder* GetForBrowserContext(
      content::BrowserContext* context) {
    return static_cast<VivaldiImageStoreHolder*>(
        GetInstance()->GetServiceForBrowserContext(context, true));
  }

  static VivaldiImageStoreFactory* GetInstance() {
    return base::Singleton<VivaldiImageStoreFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<VivaldiImageStoreFactory>;

  VivaldiImageStoreFactory()
      : BrowserContextKeyedServiceFactory(
            "VivaldiImageStore",
            BrowserContextDependencyManager::GetInstance()) {}
  ~VivaldiImageStoreFactory() override = default;

  // BrowserContextKeyedServiceFactory:

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* browser_context) const override {
    return chrome::GetBrowserContextRedirectedInIncognito(browser_context);
  }

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const override {
    return new VivaldiImageStoreHolder(browser_context);
  }
};

}  // namespace

std::vector<base::FilePath::StringType>
VivaldiImageStore::GetAllowedImageExtensions() {
  std::vector<base::FilePath::StringType> extensions;
  for (int i = 0; i < kImageFormatCount; ++i) {
    extensions.push_back(
        base::FilePath::FromASCII(kCanonicalExtensions[i]).value());
    if (static_cast<ImageFormat>(i) == ImageFormat::kJPEG) {
      extensions.push_back(base::FilePath::FromASCII("jpeg").value());
    }
  }
  return extensions;
}

/* static */
absl::optional<VivaldiImageStore::ImageFormat>
VivaldiImageStore::FindFormatForMimeType(base::StringPiece mime_type) {
  for (int i = 0; i < kImageFormatCount; ++i) {
    if (mime_type == kAllowedMimeTypes[i]) {
      return static_cast<ImageFormat>(i);
    }
  }
  return absl::nullopt;
}

/* static */
absl::optional<VivaldiImageStore::ImageFormat>
VivaldiImageStore::FindFormatForExtension(base::StringPiece file_extension) {
  if (file_extension.empty())
    return absl::nullopt;
  if (file_extension[0] == '.') {
    file_extension.remove_prefix(1);
  }
  for (int i = 0; i < kImageFormatCount; ++i) {
    if (base::EqualsCaseInsensitiveASCII(file_extension,
                                         kCanonicalExtensions[i])) {
      return static_cast<ImageFormat>(i);
    }
  }
  if (base::EqualsCaseInsensitiveASCII(file_extension, "jpeg")) {
    return ImageFormat::kJPEG;
  }
  return absl::nullopt;
}

/* static */
absl::optional<VivaldiImageStore::ImageFormat>
VivaldiImageStore::FindFormatForPath(const base::FilePath& path) {
#if BUILDFLAG(IS_WIN)
  return FindFormatForExtension(base::WideToUTF8(path.FinalExtension()));
#else
  return FindFormatForExtension(path.FinalExtension());
#endif
}

/* static */
bool VivaldiImageStore::ParseDataUrl(base::StringPiece url,
                                     VivaldiImageStore::UrlKind& url_kind,
                                     std::string& id) {
  using PathType = vivaldi_data_url_utils::PathType;

  absl::optional<PathType> type = vivaldi_data_url_utils::ParseUrl(url, &id);
  if (type == PathType::kImage) {
    url_kind = VivaldiImageStore::kImageUrl;
    return true;
  }
  if (type == PathType::kLocalPath) {
    url_kind = VivaldiImageStore::kPathMappingUrl;
    return true;
  }
  return false;
}

namespace {
// Hash the image data and produce a string that can be used as a file name. The
// strings should contain all uppercase letters.
std::string HashDataToFileName(const uint8_t* data, size_t size) {
  std::array<uint8_t, crypto::kSHA256Length> hash =
      crypto::SHA256Hash(base::span<const uint8_t>(data, size));

  // Use base32 as that is case-insensitive and uses only letters and digits so
  // it works nicely as a file name. Use just 160 bits of hash that in base32
  // gives 32 character string rather than 53 as it will be with full 256-bit
  // string.
  constexpr size_t kHashBytesToUse = 20;
  static_assert(kHashBytesToUse <= crypto::kSHA256Length,
                "cannot use more than hash length");
  return base32::Base32Encode(
      base::StringPiece(reinterpret_cast<const char*>(hash.data()),
                        kHashBytesToUse),
      base32::Base32EncodePolicy::OMIT_PADDING);
}

}  // namespace

VivaldiImageStore::VivaldiImageStore(Profile* profile)
    : profile_(profile),
      user_data_dir_(profile->GetPath()),
      ui_thread_runner_(content::GetUIThreadTaskRunner(
          {base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      sequence_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_VISIBLE, base::MayBlock()})) {}

VivaldiImageStore::~VivaldiImageStore() {}

void VivaldiImageStore::Start() {
  sequence_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VivaldiImageStore::LoadMappingsOnFileThread, this));

  // Inline ScheduleRemovalOfUnusedUrlData here as it uses FromBrowserContext()
  // but that can not be used when the factory initializes the instance.
  ui_thread_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&VivaldiImageStore::FindUsedUrlsOnUIThread, this),
      kDataUrlGCStartupDelay);
}

void VivaldiImageStore::LoadMappingsOnFileThread() {
  DCHECK(sequence_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(path_id_map_.empty());

  base::FilePath file_path = GetFileMappingFilePath();
  scoped_refptr<base::RefCountedMemory> data =
      vivaldi_data_url_utils::ReadFileOnBlockingThread(file_path,
                                                       /*log_not_found=*/false);
  if (!data)
    return;

  auto root = base::JSONReader::ReadAndReturnValueWithError(
      base::StringPiece(data->front_as<char>(), data->size()));
  if (!root.has_value()) {
    LOG(ERROR) << file_path.value() << " is not a valid JSON - "
               << root.error().message;
    return;
  }

  if (root.value().is_dict()) {
    if (base::Value::Dict* mappings =
            root.value().GetDict().FindDict("mappings")) {
      InitMappingsOnFileThread(*mappings);
    }
  }
}

void VivaldiImageStore::InitMappingsOnFileThread(base::Value::Dict& mappings) {
  DCHECK(sequence_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(path_id_map_.empty());

  for (auto i : mappings) {
    const std::string& id = i.first;
    if (vivaldi_data_url_utils::isOldFormatThumbnailId(id)) {
      // Older mapping entry that we just skip as we know the path statically.
      continue;
    }
    if (base::Value::Dict* dict = i.second.GetIfDict()) {
      std::string* path_string = dict->FindString("local_path");
      if (!path_string) {
        // Older format support.
        path_string = dict->FindString("relative_path");
      }
      if (path_string) {
#if BUILDFLAG(IS_POSIX)
        base::FilePath path(*path_string);
#elif BUILDFLAG(IS_WIN)
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

std::string VivaldiImageStore::GetMappingJSONOnFileThread() {
  DCHECK(sequence_task_runner_->RunsTasksInCurrentSequence());

  // TODO(igor@vivaldi.com): Write the mapping file even if there are no
  // entries. This allows in future to write a URL format converter for
  // bookamrks and a add a version field to the file. Then presence of the file
  // without the version string will indicate the need for converssion.

  base::Value::Dict items;
  for (const auto& it : path_id_map_) {
    const base::FilePath& path = it.second;
    base::Value::Dict item;
    item.Set("local_path", path.AsUTF16Unsafe());
    items.Set(it.first, std::move(item));
  }

  base::Value::Dict root;
  root.Set("mappings", std::move(items));

  std::string json;
  base::JSONWriter::WriteWithOptions(
      root, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  return json;
}

void VivaldiImageStore::SaveMappingsOnFileThread() {
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
  // on OS crashes or power loss, but loosing the map is not the end of
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

base::FilePath VivaldiImageStore::GetFileMappingFilePath() {
  return user_data_dir_.AppendASCII(kDatasourceFilemappingFilename);
}

base::FilePath VivaldiImageStore::GetImagePath(base::StringPiece image_id) {
  base::FilePath path = user_data_dir_.Append(kImageDirectory);
#if BUILDFLAG(IS_POSIX)
  path = path.Append(image_id);
#elif BUILDFLAG(IS_WIN)
  path = path.Append(base::UTF8ToWide(image_id));
#endif
  return path;
}

// static
void VivaldiImageStore::ScheduleRemovalOfUnusedUrlData(
    content::BrowserContext* browser_context,
    base::TimeDelta when) {
  VivaldiImageStore* api = FromBrowserContext(browser_context);
  DCHECK(api);
  if (!api) {
    return;
  }

  api->ui_thread_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&VivaldiImageStore::FindUsedUrlsOnUIThread, api), when);
}

void VivaldiImageStore::FindUsedUrlsOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  vivaldi_bookmark_kit::RunAfterModelLoad(
      GetBookmarkModel(),
      base::BindOnce(
          &VivaldiImageStore::FindUsedUrlsOnUIThreadWithLoadedBookmaks, this));
}

void VivaldiImageStore::FindUsedUrlsOnUIThreadWithLoadedBookmaks(
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
    std::string thumbnail_url = vivaldi_bookmark_kit::GetThumbnail(node);
    if (ParseDataUrl(thumbnail_url, url_kind, id)) {
      used_ids[url_kind].push_back(std::move(id));
    }
  }

  auto check_url = [](UsedIds* used_ids, base::StringPiece url) {
    UrlKind url_kind;
    std::string id;
    if (ParseDataUrl(url, url_kind, id)) {
      used_ids->at(url_kind).push_back(std::move(id));
    }
  };

  // Find data url ids in preferences.
  PrefService* prefs = profile_->GetPrefs();
  check_url(&used_ids,
            prefs->GetString(vivaldiprefs::kThemeBackgroundUserImage));

  // base::Unretained() is OK as the callback is only called inside the callee.
  vivaldi_theme_io::EnumerateUserThemeUrls(
      prefs, base::BindRepeating(check_url, base::Unretained(&used_ids)));

  sequence_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VivaldiImageStore::RemoveUnusedUrlDataOnFileThread, this,
                     std::move(used_ids)));
}

void VivaldiImageStore::RemoveUnusedUrlDataOnFileThread(UsedIds used_ids) {
  static_assert(kUrlKindCount == 2, "The code supports 2 url kinds");
  DCHECK(sequence_task_runner_->RunsTasksInCurrentSequence());

  // Add newly allocated ids that have not been stored in bookmarks or
  // preferences yet.
  UrlKind url_kind;
  std::string url_id;
  for (const std::string& data_url : file_thread_newborn_urls_) {
    if (ParseDataUrl(data_url, url_kind, url_id)) {
      used_ids[url_kind].push_back(url_id);
    }
  }

  base::flat_set<std::string> used_path_mapping_set(
      std::move(used_ids[kPathMappingUrl]));

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

  base::flat_set<std::string> used_image_set(std::move(used_ids[kImageUrl]));
  base::FileEnumerator files(user_data_dir_.Append(kImageDirectory), false,
                             base::FileEnumerator::FILES);
  size_t removed_images = 0;
  for (base::FilePath path = files.Next(); !path.empty(); path = files.Next()) {
    base::FilePath base_name = path.BaseName();
    std::string id = base_name.AsUTF8Unsafe();
    if (!used_image_set.contains(id)) {
      if (!base::DeleteFile(path)) {
        LOG(WARNING) << "Failed to remove the image file " << path;
      }
      removed_images++;
    }
  }
  if (removed_images) {
    LOG(INFO) << removed_images << " unreferenced image files were removed";
  }
}

void VivaldiImageStore::AddNewbornUrlOnFileThread(base::StringPiece data_url) {
  DCHECK(sequence_task_runner_->RunsTasksInCurrentSequence());
  file_thread_newborn_urls_.push_back(
      std::string(data_url.data(), data_url.size()));
}

void VivaldiImageStore::ForgetNewbornUrl(std::string data_url) {
  if (data_url.empty())
    return;
  sequence_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VivaldiImageStore::ForgetNewbornUrlOnFileThread, this,
                     std::move(data_url)));
}

void VivaldiImageStore::ForgetNewbornUrlOnFileThread(std::string data_url) {
  DCHECK(sequence_task_runner_->RunsTasksInCurrentSequence());
  std::vector<std::string>& v = file_thread_newborn_urls_;
  auto i = std::find(v.begin(), v.end(), data_url);
  if (i != v.end()) {
    v.erase(i);
  } else {
    // This should only be called for active ids.
    NOTREACHED() << data_url;
  }
}

// static
void VivaldiImageStore::UpdateMapping(content::BrowserContext* browser_context,
                                      ImagePlace place,
                                      ImageFormat format,
                                      base::FilePath file_path,
                                      StoreImageCallback callback) {
  DCHECK(!place.IsEmpty());
  VivaldiImageStore* api = FromBrowserContext(browser_context);
  DCHECK(api);
  if (!api) {
    LOG(ERROR) << "No API";
    std::move(callback).Run(false);
    return;
  }

  api->sequence_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VivaldiImageStore::UpdateMappingOnFileThread,
                                api, std::move(place), format,
                                std::move(file_path), std::move(callback)));
}

void VivaldiImageStore::UpdateMappingOnFileThread(ImagePlace place,
                                                  ImageFormat format,
                                                  base::FilePath file_path,
                                                  StoreImageCallback callback) {
  DCHECK(sequence_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(format == FindFormatForPath(file_path));

  std::string path_str =
      file_path.NormalizePathSeparatorsTo('/').AsUTF8Unsafe();
  std::string path_id = HashDataToFileName(
      reinterpret_cast<const uint8_t*>(path_str.data()), path_str.size());

  // Add the extension so we can deduce mime type just from URL.
  path_id += '.';
  path_id += GetCanonicalExtension(format);

  std::string data_url = vivaldi_data_url_utils::MakeUrl(
      vivaldi_data_url_utils::PathType::kLocalPath, path_id);
  AddNewbornUrlOnFileThread(data_url);

  // inserted is false when file_path points to an already existing mapping.
  bool inserted =
      path_id_map_.emplace(std::move(path_id), std::move(file_path)).second;

  ui_thread_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VivaldiImageStore::FinishStoreImageOnUIThread,
                                this, std::move(callback), std::move(place),
                                std::move(data_url)));
  if (inserted) {
    SaveMappingsOnFileThread();
  }
}

void VivaldiImageStore::FinishStoreImageOnUIThread(StoreImageCallback callback,
                                                   ImagePlace place,
                                                   std::string data_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // profile are null on shutdown.
  bool success = false;
  if (!profile_) {
    LOG(ERROR) << "shutdown started";
  } else if (place.IsBookmarkId()) {
    if (bookmarks::BookmarkModel* bookmark_model = GetBookmarkModel()) {
      success = vivaldi_bookmark_kit::SetBookmarkThumbnail(
          bookmark_model, place.GetBookmarkId(), data_url);
    }
  } else if (place.IsBackgroundUserImage()) {
    profile_->GetPrefs()->SetString(vivaldiprefs::kThemeBackgroundUserImage,
                                    data_url);
    success = true;
  } else if (place.IsThemeId()) {
    success = vivaldi_theme_io::StoreImageUrl(profile_->GetPrefs(),
                                              place.GetThemeId(), data_url);
  } else {
    NOTREACHED();
  }
  ForgetNewbornUrl(std::move(data_url));
  std::move(callback).Run(success);
}

// static
void VivaldiImageStore::GetDataForId(
    content::BrowserContext* browser_context,
    UrlKind url_kind,
    std::string id,
    content::URLDataSource::GotDataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  VivaldiImageStore* api = FromBrowserContext(browser_context);
  DCHECK(api);
  if (!api) {
    std::move(callback).Run(nullptr);
    return;
  }
  api->GetDataForId(url_kind, std::move(id), std::move(callback));
}

void VivaldiImageStore::GetDataForId(
    UrlKind url_kind,
    std::string id,
    content::URLDataSource::GotDataCallback callback) {
  sequence_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&VivaldiImageStore::GetDataForIdOnFileThread, this,
                     url_kind, std::move(id)),
      std::move(callback));
}

scoped_refptr<base::RefCountedMemory>
VivaldiImageStore::GetDataForIdOnFileThread(UrlKind url_kind, std::string id) {
  DCHECK(sequence_task_runner_->RunsTasksInCurrentSequence());

  base::FilePath file_path;
  if (url_kind == kImageUrl) {
    file_path = GetImagePath(id);
  } else {
    DCHECK(url_kind == kPathMappingUrl);

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

  return data;
}

// static
void VivaldiImageStore::CaptureBookmarkThumbnail(
    content::BrowserContext* browser_context,
    int64_t bookmark_id,
    const GURL& url,
    StoreImageCallback ui_thread_callback) {
  VivaldiImageStore* api = FromBrowserContext(browser_context);
  DCHECK(api);
  if (!api) {
    std::move(ui_thread_callback).Run(false);
    return;
  }
  ImagePlace place;
  place.SetBookmarkId(bookmark_id);
  ::vivaldi::ThumbnailCaptureContents::Capture(
      browser_context, url,
      gfx::Size(kOffscreenWindowWidth, kOffscreenWindowHeight),
      gfx::Size(kBookmarkThumbnailWidth, kBookmarkThumbnailHeight),
      base::BindOnce(&VivaldiImageStore::StoreImageUIThread, api,
                     std::move(place), std::move(ui_thread_callback),
                     ImageFormat::kPNG));
}

// static
void VivaldiImageStore::StoreImage(
    content::BrowserContext* browser_context,
    ImagePlace place,
    ImageFormat format,
    scoped_refptr<base::RefCountedMemory> image_data,
    StoreImageCallback callback) {
  VivaldiImageStore* api = FromBrowserContext(browser_context);
  DCHECK(api);
  if (!api) {
    std::move(callback).Run(false);
    return;
  }

  api->StoreImageUIThread(std::move(place), std::move(callback), format,
                          std::move(image_data));
}

void VivaldiImageStore::StoreImageUIThread(
    ImagePlace place,
    StoreImageCallback ui_thread_callback,
    ImageFormat format,
    scoped_refptr<base::RefCountedMemory> image_data) {
  StoreImageData(
      format, std::move(image_data),
      base::BindOnce(&VivaldiImageStore::FinishStoreImageOnUIThread, this,
                     std::move(ui_thread_callback), std::move(place)));
}

void VivaldiImageStore::StoreImageData(
    ImageFormat format,
    scoped_refptr<base::RefCountedMemory> image_data,
    StoreImageDataResult callback) {
  sequence_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&VivaldiImageStore::StoreImageDataOnFileThread, this,
                     format, std::move(image_data)),
      std::move(callback));
}

std::string VivaldiImageStore::StoreImageDataOnFileThread(
    ImageFormat format,
    scoped_refptr<base::RefCountedMemory> image_data) {
  DCHECK(sequence_task_runner_->RunsTasksInCurrentSequence());

  if (!image_data || !image_data->size())
    return std::string();

  std::string image_id =
      HashDataToFileName(image_data->data(), image_data->size());
  image_id += '.';
  image_id += GetCanonicalExtension(format);
  std::string data_url = vivaldi_data_url_utils::MakeUrl(
      vivaldi_data_url_utils::PathType::kImage, image_id);
  AddNewbornUrlOnFileThread(data_url);

  base::FilePath path = GetImagePath(image_id);
  base::FilePath dir = path.DirName();
  if (!base::DirectoryExists(dir)) {
    LOG(INFO) << "Creating image directory: " << dir.value();
    base::CreateDirectory(dir);
  }
  if (base::PathExists(path)) {
    // We already have such image.
    return data_url;
  }

  // The caller must ensure that data fit 2G.
  int bytes = base::WriteFile(path, image_data->front_as<char>(),
                              static_cast<int>(image_data->size()));
  if (bytes < 0 || static_cast<size_t>(bytes) != image_data->size()) {
    LOG(ERROR) << "Error writing to file: " << path.value();
    return std::string();
  }
  return data_url;
}

bookmarks::BookmarkModel* VivaldiImageStore::GetBookmarkModel() {
  if (!profile_)
    return nullptr;
  return BookmarkModelFactory::GetForBrowserContext(profile_);
}

// static
void VivaldiImageStore::InitFactory() {
  VivaldiImageStoreFactory::GetInstance();
}

VivaldiImageStore* VivaldiImageStore::FromBrowserContext(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  VivaldiImageStoreHolder* holder =
      VivaldiImageStoreFactory::GetForBrowserContext(browser_context);
  if (!holder)
    return nullptr;
  return holder->api_.get();
}
