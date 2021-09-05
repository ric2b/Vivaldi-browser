// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/bookmarks/vivaldi_partners.h"

#if defined(OS_ANDROID)
#include "base/android/apk_assets.h"
#endif
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace vivaldi_partners {

namespace {

#if defined(OS_ANDROID)
const char kAssetsDir[] = "assets/default-bookmarks/";
#else
#if !defined(OS_MAC)
const base::FilePath::CharType kResources[] = FILE_PATH_LITERAL("resources");
#endif
const base::FilePath::CharType kVivaldi[] = FILE_PATH_LITERAL("vivaldi");
const base::FilePath::CharType kDefBookmarks[] =
    FILE_PATH_LITERAL("default-bookmarks");
#endif  // !OS_ANDROID

const char kPartnerDBFile[] = "partners.json";
const char kPartnerLocaleMapFile[] = "partners-locale-map.json";

const char kPartnerThumbnailUrlPrefix[] = "/resources/";

// JSON keys
const char kBookmarksKey[] = "bookmarks";
const char kFoldersKey[] = "folders";
const char kGuidKey[] = "guid";
const char kGuid2Key[] = "guid2";
const char kNameKey[] = "name";
const char kSpeeddialKey[] = "speeddial";
const char kThumbnailKey[] = "thumbnail";
const char kTitleKey[] = "title";

bool IsValidBookmarkName(bool folder, base::StringPiece name) {
  if (name.empty())
    return false;

  // The folder name must use only latin letters, digits and must start with a
  // capital letter. Bookmark name must use only latin letters, digits, dash,
  // dot and must start with a small letter or digit.
  for (size_t i = 0; i < name.size(); ++i) {
    char c = name[i];
    if (base::IsAsciiAlpha(c)) {
      if (i == 0) {
        if (folder != base::IsAsciiUpper(c))
          return false;
      }
      continue;
    }
    if (base::IsAsciiDigit(c)) {
      if (i == 0 && folder)
        return false;
      continue;
    }
    if (!folder) {
      if (c == '.' || c == '-')
        continue;
    }
    return false;
  }

  return true;
}

bool ParsePartnerDatabaseDetailsList(
    bool is_folder,
    base::Value* list_value,
    std::vector<PartnerDetails>& details_list) {
  DCHECK(list_value->is_list());
  auto list = list_value->GetList();
  for (size_t i = 0; i < list.size(); ++i) {
    auto error = [&](base::StringPiece message) -> bool {
      LOG(ERROR) << "Partner database JSON error: bad format of "
                 << (is_folder ? kFoldersKey : kBookmarksKey) << "[" << i
                 << "] - " << message;
      return false;
    };
    base::Value& dict = list[i];
    if (!dict.is_dict())
      return error("entry is not an object");
    PartnerDetails details;
    details.folder = is_folder;
    for (auto property_key_value : dict.DictItems()) {
      const std::string& property = property_key_value.first;
      base::Value& v = property_key_value.second;
      bool folder_only = false;
      bool bookmark_only = false;
      if (property == kNameKey) {
        if (!v.is_string())
          return error(property + " is not a string");
        details.name = std::move(v.GetString());
        if (!IsValidBookmarkName(is_folder, details.name))
          return error(property + " is not a valid bookmark name - " +
                       details.name);
      } else if (property == kTitleKey) {
        if (!v.is_string())
          return error(property + " is not a string");
        details.title = std::move(v.GetString());
      } else if (property == kGuidKey || property == kGuid2Key) {
        if (!v.is_string())
          return error(property + " is not a string");
        std::string* guid =
            (property == kGuidKey ? &details.guid : &details.guid2);
        *guid = std::move(v.GetString());
        if (!base::IsValidGUID(*guid))
          return error(property + " is not a valid GUID - " + *guid);
        if (property == kGuid2Key) {
          bookmark_only = true;
        }
      } else if (property == kSpeeddialKey) {
        if (!v.is_bool())
          return error(property + " is not a boolean");
        details.speeddial = v.GetBool();
        folder_only = true;
      } else if (property == kThumbnailKey) {
        if (!v.is_string())
          return error(property + " is not a string");
        if (!HasPartnerThumbnailPrefix(v.GetString()))
          return error(property + " value does not start with " +
                       kPartnerThumbnailUrlPrefix);
        details.thumbnail = std::move(v.GetString());
        bookmark_only = true;
      } else {
        return error("unsupported or unknown property '" + property + "'");
      }
      if (is_folder) {
        if (bookmark_only)
          return error("property '" + property +
                       "' cannnot present in a folder");
      } else {
        if (folder_only)
          return error("property '" + property +
                       "' cannnot present in a bookmark");
      }
    }
    if (details.name.empty())
      return error(std::string("missing ") + kNameKey + " property");
    if (details.guid.empty())
      return error(std::string("missing ") + kGuidKey + " property");
    if (is_folder) {
      if (details.title.empty()) {
        details.title = details.name;
      }
    } else {
      if (details.guid2.empty())
        return error(std::string("missing ") + kGuid2Key + " property");
    }
    details_list.push_back(std::move(details));
  }
  return true;
}

}  // namespace

bool HasPartnerThumbnailPrefix(base::StringPiece url) {
  return base::StartsWith(url, kPartnerThumbnailUrlPrefix);
}

AssetReader::AssetReader() = default;
AssetReader::~AssetReader() = default;

std::string AssetReader::GetPath() const {
#if defined(OS_ANDROID)
  return path_;
#else
  return path_.AsUTF8Unsafe();
#endif  //  OS_ANDROID
}

base::Optional<base::Value> AssetReader::ReadJson(base::StringPiece name) {
  not_found_ = false;
  base::StringPiece json_text;
#if defined(OS_ANDROID)
  path_ = kAssetsDir;
  path_.append(name.data(), name.length());

  base::MemoryMappedFile::Region region;
  base::MemoryMappedFile mapped_file;
  int json_fd = base::android::OpenApkAsset(path_, &region);
  if (json_fd < 0) {
    not_found_ = true;
  } else {
    if (!mapped_file.Initialize(base::File(json_fd), region)) {
      LOG(ERROR) << "failed to initialize memory mapping for " << path_;
      return base::nullopt;
    }
    json_text = base::StringPiece(reinterpret_cast<char*>(mapped_file.data()),
                                  mapped_file.length());
  }
#else
  if (asset_dir_.empty()) {
    base::PathService::Get(base::DIR_ASSETS, &asset_dir_);
#if !defined(OS_MAC)
    asset_dir_ = asset_dir_.Append(kResources);
#endif  // if !defined(OS_MAC)
    asset_dir_ = asset_dir_.Append(kVivaldi).Append(kDefBookmarks);
  }
  path_ = asset_dir_.AppendASCII(name);
  std::string json_buffer;
  if (!base::PathExists(path_)) {
    not_found_ = true;
  } else {
    if (!base::ReadFileToString(path_, &json_buffer)) {
      LOG(ERROR) << "failed to read " << path_;
      return base::nullopt;
    }
    json_text = base::StringPiece(json_buffer);
  }
#endif  //  OS_ANDROID
  if (not_found_) {
    if (log_not_found_) {
      LOG(ERROR) << GetPath() << ": path not found";
    }
    return base::nullopt;
  }

  base::JSONReader::ValueWithError v =
      base::JSONReader::ReadAndReturnValueWithError(json_text);
  if (!v.value) {
    LOG(ERROR) << GetPath() << ":" << v.error_line << ":" << v.error_column << ": JSON error - " << v.error_message;
  }
  return std::move(v.value);
}

PartnerDetails::PartnerDetails() = default;
PartnerDetails::PartnerDetails(PartnerDetails&&) = default;
PartnerDetails::~PartnerDetails() = default;
PartnerDetails& PartnerDetails::operator=(PartnerDetails&&) = default;

namespace {

class PartnerDatabase {
public:
  PartnerDatabase() = default;
  ~PartnerDatabase()= default;

  static std::unique_ptr<PartnerDatabase> Read();

  const PartnerDetails* FindDetailsByName(base::StringPiece name) const {
    auto i = name_index_.find(name);
    if (i == name_index_.end())
      return nullptr;
    return i->second;
  }

  const PartnerDetails* FindDetailsByPartner(
      base::StringPiece partner_id) const {
    auto i = locale_id_guid_map_.find(partner_id);
    if (i != locale_id_guid_map_.end()) {
      partner_id = i->second;
    }
    auto i2 = guid_index_.find(partner_id);
    if (i2 == guid_index_.end())
      return nullptr;
    return i2->second;
  }

  bool MapLocaleIdToGUID(std::string& id) const {
    auto i = locale_id_guid_map_.find(id);
    if (i == locale_id_guid_map_.end())
      return false;
    base::StringPiece guid = i->second;
    id.assign(guid.data(), guid.length());
    return true;
  }

private:
 bool ParseJson(base::Value root, base::Value partners_locale_value);

 std::vector<PartnerDetails> details_list_;

 // Map partner details name to its details.
 base::flat_map<base::StringPiece, const PartnerDetails*> name_index_;

 // Map locale-independent guid or guid2 to its details.
 base::flat_map<base::StringPiece, const PartnerDetails*> guid_index_;

 // Map old locale-based partner id to the guid or guid2 if the old id is for
 // an url under Bookmarks folder.
 base::flat_map<std::string, base::StringPiece> locale_id_guid_map_;

 // The maps above contains references, so prevent copy/assignment for safety.
 DISALLOW_COPY_AND_ASSIGN(PartnerDatabase);
};

// Global singleton.
const PartnerDatabase* g_partner_db = nullptr;

// static
std::unique_ptr<PartnerDatabase> PartnerDatabase::Read() {
  AssetReader reader;
  reader.set_log_not_found();
  base::Optional<base::Value> partner_db_value =
      reader.ReadJson(kPartnerDBFile);
  if (!partner_db_value)
    return nullptr;

  base::Optional<base::Value> partners_locale_value =
      reader.ReadJson(kPartnerLocaleMapFile);
  if (!partners_locale_value)
    return nullptr;

  auto db = std::make_unique<PartnerDatabase>();
  if (!db->ParseJson(std::move(*partner_db_value),
                     std::move(*partners_locale_value)))
    return nullptr;

  return db;
}

bool PartnerDatabase::ParseJson(
    base::Value root,
    base::Value partners_locale_value) {
  auto error = [](base::StringPiece message) -> bool {
    LOG(ERROR) << "Partner database JSON error: " << message;
    return false;
  };
  if (!root.is_dict())
    return error("partner db json is not an object");

  base::Value* folders_value = root.FindListKey(kFoldersKey);
  if (!folders_value)
    return error(std::string("missing ") + kFoldersKey + " key");

  base::Value* bookmarks_value = root.FindListKey(kBookmarksKey);
  if (!bookmarks_value)
    return error(std::string("missing ") + kBookmarksKey + " key");

  details_list_.reserve(folders_value->GetList().size() +
                        bookmarks_value->GetList().size());
  if (!ParsePartnerDatabaseDetailsList(true, folders_value, details_list_))
    return false;
  if (!ParsePartnerDatabaseDetailsList(false, bookmarks_value, details_list_))
    return false;

  // Establish the index now that we no longer mutate details and check
  // for unique values.
  std::vector<std::pair<base::StringPiece, const PartnerDetails*>> names;
  names.reserve(details_list_.size());
  std::vector<std::pair<base::StringPiece, const PartnerDetails*>> guids;
  guids.reserve(details_list_.size() * 2);
  for (const PartnerDetails& details : details_list_) {
    names.emplace_back(details.name, &details);
    guids.emplace_back(details.guid, &details);
    if (!details.guid2.empty()) {
      guids.emplace_back(details.guid2, &details);
    }
  }
  name_index_ = base::flat_map<base::StringPiece, const PartnerDetails*>(
      std::move(names));
  if (name_index_.size() != details_list_.size())
    return error("duplicatd names");
  size_t added_guids = guids.size();
  guid_index_ = base::flat_map<base::StringPiece, const PartnerDetails*>(
      std::move(guids));
  if (guid_index_.size() != added_guids)
    return error("duplicatd GUIDs");

  // Parse mapping from old locale-based ids to the new universal ids.
  std::vector<std::pair<std::string, base::StringPiece>> locale_id_guid_list;
  if (!partners_locale_value.is_dict())
    return error("partner local map json is not an object");
  for (auto name_key_value : partners_locale_value.DictItems()) {
    const std::string& name = name_key_value.first;
    const PartnerDetails* details = FindDetailsByName(name);
    if (!details)
      return error(std::string("'") + name + "' from " + kPartnerLocaleMapFile +
                   " is not defined in " + kGuidKey);
    base::Value& locale_dict = name_key_value.second;
    if (!locale_dict.is_dict())
      return error(std::string(kPartnerLocaleMapFile) + "." + name +
                   " is not a dictionary");

    for (auto guid_key_value : locale_dict.DictItems()) {
      const std::string& guid_key = guid_key_value.first;
      base::Value& v = guid_key_value.second;
      base::StringPiece guid;
      if (guid_key == kGuidKey) {
        guid = details->guid;
      } else if (guid_key == kGuid2Key) {
        guid = details->guid2;
      } else {
        return error(std::string("unknown key ") + guid_key + " in " +
                     kPartnerLocaleMapFile + "." + name);
      }
      if (!v.is_list())
        return error(std::string(kPartnerLocaleMapFile) + "." + name + "." +
                     guid_key + " is not a list");
      base::Value::ListView id_list = v.GetList();
      for (base::Value& id_value : id_list) {
        if (!id_value.is_string())
          return error(std::string("Partner id in ") +
                       std::string(kPartnerLocaleMapFile) + "." + name + "." +
                       guid_key + " is not a string");
        std::string locale_id = std::move(id_value.GetString());
        if (!base::IsValidGUID(locale_id))
          return error(std::string("Partner id in ") +
                       std::string(kPartnerLocaleMapFile) + "." + name + "." +
                       guid_key + " is not a valid GUID - " + locale_id);
        locale_id_guid_list.emplace_back(std::move(locale_id), guid);
      }
    }
  }
  locale_id_guid_map_ = base::flat_map<std::string, base::StringPiece>(
      std::move(locale_id_guid_list));
  return true;
}

}  // namespace

const PartnerDetails* FindDetailsByName(base::StringPiece name) {
  if (!g_partner_db)
    return nullptr;
  return g_partner_db->FindDetailsByName(name);
}

bool MapLocaleIdToGUID(std::string& id) {
  if (!g_partner_db)
    return false;
  return g_partner_db->MapLocaleIdToGUID(id);
}

const std::string& GetThumbnailUrl(const std::string& partner_id) {
  if (!g_partner_db)
    return base::EmptyString();
  const PartnerDetails* details =
      g_partner_db->FindDetailsByPartner(partner_id);
  if (!details)
    return base::EmptyString();
  return details->thumbnail;
}

void LoadOnWorkerThread(
    const scoped_refptr<base::SequencedTaskRunner>& main_thread_task_runner) {
  DCHECK(main_thread_task_runner);
  if (g_partner_db)
    return;
  std::unique_ptr<PartnerDatabase> db = PartnerDatabase::Read();
  if (!db)
    return;
  auto task = [](std::unique_ptr<PartnerDatabase> db) {
    // When loading several profiles g_partner_db can be initialized on the main
    // thread from another profile even after the above g_partner_db check.
    if (g_partner_db)
      return;
    g_partner_db = db.release();
  };
  main_thread_task_runner->PostTask(FROM_HERE,
                                    base::BindOnce(task, std::move(db)));
}

}  // namespace vivaldi_partners