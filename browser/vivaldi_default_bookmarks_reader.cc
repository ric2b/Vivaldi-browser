// Copyright 2018 Vivaldi Technologies. All rights reserved.

#include "browser/vivaldi_default_bookmarks_reader.h"

#include "app/vivaldi_version_constants.h"
#if defined(OS_ANDROID)
#include "base/android/apk_assets.h"
#endif
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/vivaldi_bookmark_kit.h"
#include "content/public/browser/browser_task_traits.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace vivaldi {

namespace first_run {
static const base::FilePath::CharType kVivaldiExe[] =
    FILE_PATH_LITERAL("vivaldi.exe");
static const base::FilePath::CharType kResources[] =
    FILE_PATH_LITERAL("resources");
static const base::FilePath::CharType kVivaldi[] =
    FILE_PATH_LITERAL("vivaldi");
static const base::FilePath::CharType kDefBookmarks[] =
    FILE_PATH_LITERAL("default-bookmarks");
#if defined(OS_ANDROID)
static const base::FilePath::CharType kFallbackLocaleFile[] =
    FILE_PATH_LITERAL("assets/default-bookmarks/en-US.json");
#else
static const base::FilePath::CharType kFallbackLocaleFile[] =
    FILE_PATH_LITERAL("en-US.json");
#endif  // OS_ANDROID

static const char kAddDateKey[] = "add_date";
static const char kChildrenKey[] = "children";
static const char kDescriptionKey[] = "description";
static const char kFaviconURLKey[] = "faviconurl";
static const char kSpeeddialKey[] = "speeddial";
static const char kThumbNailKey[] = "thumbnail";
static const char kTitleKey[] = "title";
static const char kURLKey[] = "url";
static const char kPartnerKey[] = "partner";

static const int64_t kRootId = 1;


VivaldiDefaultBookmarksReader::VivaldiDefaultBookmarksReader() {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  DCHECK(profile);
  model_ = BookmarkModelFactory::GetForBrowserContext(profile);
  DCHECK(model_);
}

VivaldiDefaultBookmarksReader::~VivaldiDefaultBookmarksReader() {
  if (model_ && added_bookmark_observer_)
    model_->RemoveObserver(this);
}

// static
VivaldiDefaultBookmarksReader* VivaldiDefaultBookmarksReader::GetInstance() {
  return base::Singleton<VivaldiDefaultBookmarksReader,
      base::DefaultSingletonTraits<VivaldiDefaultBookmarksReader>>::get();
}

void VivaldiDefaultBookmarksReader::ReadBookmarks() {
  if (waiting_for_bookmark_model_)
    return;

  DCHECK(model_);
  // Model should not be loaded at this point.
  DCHECK(!model_->loaded());

  if (!ReadJson())
    return;

  // Wait for model to load.
  model_->AddObserver(this);
  waiting_for_bookmark_model_ = true;
  added_bookmark_observer_ = true;
}

void VivaldiDefaultBookmarksReader::BookmarkModelLoaded(
    bookmarks::BookmarkModel* model,
    bool ids_reassigned) {
  model->RemoveObserver(this);
  waiting_for_bookmark_model_ = false;
  added_bookmark_observer_ = false;

  // If model is populated, don't read default bookmarks.
  if (model->HasBookmarks()) {
    LOG(INFO) << "Bookmarks model is populated.";
    return;
  }

  base::PostTask(FROM_HERE,
      { content::BrowserThread::UI, base::TaskPriority::USER_VISIBLE },
      base::Bind(&VivaldiDefaultBookmarksReader::Task::CreateBookmarks,
          scoped_refptr<VivaldiDefaultBookmarksReader::Task>(
              new VivaldiDefaultBookmarksReader::Task),
              &root_.value(), model));
}

base::FilePath VivaldiDefaultBookmarksReader::GetDefaultBookmarksFilePath() {
  base::FilePath path;
  const std::string file_name =
      g_browser_process->GetApplicationLocale() + ".json";
#if defined(OS_ANDROID)
  // For Android, get the default bookmarks from assets within the apk.
  path = base::FilePath(FILE_PATH_LITERAL("assets"));
  path = path.Append(kDefBookmarks).AppendASCII(file_name);
#else
  base::PathService::Get(base::DIR_EXE, &path);
#if !defined(_DEBUG)
  path = path.AppendASCII(vivaldi::kVivaldiVersion);
#endif  // _DEBUG
  path = path.Append(kResources).Append(kVivaldi)
      .Append(kDefBookmarks).AppendASCII(file_name);
  if (!base::PathExists(path)) {
    base::PathService::Get(base::DIR_EXE, &path);
#if !defined(_DEBUG)
    path = path.AppendASCII(vivaldi::kVivaldiVersion);
#endif
    path = path.Append(kResources).Append(kVivaldi)
        .Append(kDefBookmarks).Append(kFallbackLocaleFile);
    if (!base::PathExists(path))
      return base::FilePath();
  }
#endif //  OS_ANDROID
  return path;
}

bool VivaldiDefaultBookmarksReader::ReadJson() {
  base::FilePath path = GetDefaultBookmarksFilePath();
  DCHECK(!path.empty());
  std::string default_bookmarks_json_content;
#if defined(OS_ANDROID)
  base::MemoryMappedFile::Region region;
  int json_fd = base::android::OpenApkAsset(path.value(), &region);
  if (json_fd < 0) {
    LOG(WARNING) << "Missing default bookmarks in APK: " << path.value();
    // Fallback to default locale.
    path = base::FilePath(kFallbackLocaleFile);
    json_fd = base::android::OpenApkAsset(path.value(), &region);
    if (json_fd < 0) {
      LOG(ERROR) << "Missing fallback bookmarks in APK: " << path.value();
      return false;
    }
  }
  std::unique_ptr<base::MemoryMappedFile>
      mapped_file(new base::MemoryMappedFile());
  if (mapped_file->Initialize(base::File(json_fd), region)) {
    default_bookmarks_json_content.assign(
        reinterpret_cast<char*>(mapped_file->data()),
        mapped_file->length());
  } else {
    return false;
  }
#else
  if (!base::ReadFileToString(path, &default_bookmarks_json_content))
    return false;
#endif //  OS_ANDROID
  root_ = base::JSONReader::Read(default_bookmarks_json_content);
  return bool(root_);
}

VivaldiDefaultBookmarksReader::Task::Task() {
}

VivaldiDefaultBookmarksReader::Task::~Task() {
}

void VivaldiDefaultBookmarksReader::Task::CreateBookmarks(
    base::Value* root,
    bookmarks::BookmarkModel* model) {
  DCHECK(root);
  DCHECK(model);
  const base::Value* bookmarks_list =
      root->FindKeyOfType(kChildrenKey, base::Value::Type::LIST);
  if (bookmarks_list) {
    for (const auto& value : bookmarks_list->GetList()) {
      DecodeFolder(value, model, kRootId);
    }
  } else {
    LOG(ERROR) << __FUNCTION__ << " No bookmarks in list.";
  }
}

bool VivaldiDefaultBookmarksReader::Task::DecodeFolder(
    const base::Value& value,
    bookmarks::BookmarkModel* model,
    const int64_t parent_id) {
  DCHECK(model);
  bool success = false;

  if (value.is_dict()) {
    const base::DictionaryValue* folder = nullptr;
    if (value.GetAsDictionary(&folder)) {
      std::string folder_name;
      bool is_speeddial = false;
      std::string folder_partner_string;

      const base::Value* title =
          folder->FindKeyOfType(kTitleKey, base::Value::Type::STRING);
      if (title) {
        folder_name = title->GetString();
      }
      else {
        NOTREACHED();
        return false;
      }
      const base::Value* speeddial =
          folder->FindKeyOfType(kSpeeddialKey, base::Value::Type::BOOLEAN);
      if (speeddial)
        is_speeddial = speeddial->GetBool();

      const base::Value* folder_partner =
          folder->FindKeyOfType(kPartnerKey, base::Value::Type::STRING);
      if (folder_partner)
        folder_partner_string = folder_partner->GetString();

      const base::Value* bookmarks_list =
          folder->FindKeyOfType(kChildrenKey, base::Value::Type::LIST);
      if (!bookmarks_list) {
        NOTREACHED();
        return false;
      }

      const BookmarkNode* parent_node =
          bookmarks::GetBookmarkNodeByID(model, parent_id);
      if (!parent_node)
        return false;

      size_t index = parent_node->children().size();
      if (index < 0)
        return false;

      vivaldi_bookmark_kit::CustomMetaInfo custom_meta;
      custom_meta.SetPartner(folder_partner_string);
      custom_meta.SetSpeeddial(is_speeddial);

      const BookmarkNode* folder_node =
          model->AddFolder(parent_node, index, base::UTF8ToUTF16(folder_name),
                           custom_meta.map());

      for (const auto& bookmark : bookmarks_list->GetList()) {
        const base::Value* children =
            bookmark.FindKeyOfType(kChildrenKey, base::Value::Type::LIST);
        if (children)
          // We have a folder within the folder, now recurse.
          DecodeFolder(bookmark, model, folder_node->id());

        custom_meta.Clear();

        const std::string* url = bookmark.FindStringKey(kURLKey);
        if (!url)
          continue;

        const std::string* add_date = bookmark.FindStringKey(kAddDateKey);
        if (!add_date)
          continue;

        const std::string* title = bookmark.FindStringKey(kTitleKey);
        if (!title)
          continue;

        const std::string* description =
            bookmark.FindStringKey(kDescriptionKey);
        if (description)
          custom_meta.SetDescription(*description);

        const std::string* thumbnail = bookmark.FindStringKey(kThumbNailKey);
        if (thumbnail)
          custom_meta.SetThumbnail(*thumbnail);

        const std::string* faviconurl = bookmark.FindStringKey(kFaviconURLKey);
        if (faviconurl)
          custom_meta.SetDefaultFaviconUri(*faviconurl);

        const std::string* partner = bookmark.FindStringKey(kPartnerKey);
        if (partner)
          custom_meta.SetPartner(*partner);

        size_t ix = folder_node->children().size();
        model->AddURL(folder_node, ix, base::UTF8ToUTF16(*title),
                      GURL(*url), custom_meta.map());
      }
      success = true;
    }
  }
  return success;
}

}  // namepace first_run
}  // namespace vivaldi
