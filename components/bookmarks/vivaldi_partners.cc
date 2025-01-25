// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/bookmarks/vivaldi_partners.h"

#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"

#include "components/datasource/resource_reader.h"
#include "components/datasource/vivaldi_data_url_utils.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/system/sys_info.h"
#if defined(OEM_RENAULT_BUILD)
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/CarDataProvider_jni.h"
#endif
#endif

namespace vivaldi_partners {

const std::string GetBookmarkResourceDir() {
  std::string bookmark_resource_dir = "default-bookmarks";
#if BUILDFLAG(IS_ANDROID) && defined(OEM_RENAULT_BUILD)
  // Append subfolder for XDD (LCV/Master) bookmarks
  const std::string lcv_master_vehicle("XDD");
  JNIEnv* env = base::android::AttachCurrentThread();
  if (Java_CarDataProvider_isModel(env,
      base::android::ConvertUTF8ToJavaString(env, lcv_master_vehicle))) {
    bookmark_resource_dir += "/" + lcv_master_vehicle;
  }
#endif
  return bookmark_resource_dir;
}

namespace {

const char kPartnerDBFile[] = "partners.json";
const char kPartnerLocaleMapFile[] = "partners-locale-map.json";

// JSON keys
const char kBookmarksKey[] = "bookmarks";
const char kFaviconKey[] = "favicon";
const char kFaviconUrlKey[] = "favicon_url";
const char kFoldersKey[] = "folders";
const char kGuidKey[] = "guid";
const char kGuid2Key[] = "guid2";
const char kNameKey[] = "name";
const char kSpeeddialKey[] = "speeddial";
const char kThumbnailKey[] = "thumbnail";
const char kTitleKey[] = "title";

bool IsValidBookmarkName(bool folder, std::string_view name) {
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
    base::Value::List& list,
    std::vector<PartnerDetails>& details_list) {
  for (size_t i = 0; i < list.size(); ++i) {
    auto error = [&](std::string_view message) -> bool {
      LOG(ERROR) << "Partner database JSON error: bad format of "
                 << (is_folder ? kFoldersKey : kBookmarksKey) << "[" << i
                 << "] - " << message;
      return false;
    };
    base::Value::Dict* dict = list[i].GetIfDict();
    if (!dict)
      return error("entry is not an object");
    PartnerDetails details;
    details.folder = is_folder;
    for (auto property_key_value : *dict) {
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
        base::Uuid uuid_value = base::Uuid::ParseCaseInsensitive(v.GetString());
        if (!uuid_value.is_valid())
          return error(property + " is not a valid Uuid - " + v.GetString());
        base::Uuid* uuid =
            (property == kGuidKey ? &details.uuid : &details.uuid2);
        *uuid = std::move(uuid_value);
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
        // For convenience of partners.json maintainence allow but ignore an
        // empty thumbnail.
        if (!v.GetString().empty()) {
          if (!vivaldi_data_url_utils::IsResourceURL(v.GetString()))
            return error(property + " value is not a browser resource URL.");
          details.thumbnail = std::move(v.GetString());
        }
        bookmark_only = true;
      } else if (property == kFaviconKey) {
        if (!v.is_string())
          return error(property + " is not a string");
        details.favicon = std::move(v.GetString());
      } else if (property == kFaviconUrlKey) {
        if (!v.is_string())
          return error(property + " is not a string");
        details.favicon_url = std::move(v.GetString());
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
    if (!details.uuid.is_valid())
      return error(std::string("missing ") + kGuidKey + " property");
    if (is_folder) {
      if (details.title.empty()) {
        details.title = details.name;
      }
    } else {
      if (!details.uuid2.is_valid())
        return error(std::string("missing ") + kGuid2Key + " property");
    }
    details_list.push_back(std::move(details));
  }
  return true;
}

}  // namespace

PartnerDetails::PartnerDetails() = default;
PartnerDetails::PartnerDetails(PartnerDetails&&) = default;
PartnerDetails::~PartnerDetails() = default;
PartnerDetails& PartnerDetails::operator=(PartnerDetails&&) = default;

namespace {

class PartnerDatabase {
 public:
  PartnerDatabase() = default;
  ~PartnerDatabase() = default;

  // The maps below contain references, so prevent copy/assignment for safety.
  PartnerDatabase(const PartnerDatabase&) = delete;
  PartnerDatabase& operator=(const PartnerDatabase&) = delete;

  static std::unique_ptr<PartnerDatabase> Read();

  const PartnerDetails* FindDetailsByName(std::string_view name) const {
    auto i = name_index_.find(name);
    if (i == name_index_.end())
      return nullptr;
    return i->second;
  }

  const PartnerDetails* FindDetailsByPartner(
      const base::Uuid& partner_id) const {
    auto i = locale_id_uuid_map_.find(partner_id);
    const base::Uuid* id = &partner_id;
    if (i != locale_id_uuid_map_.end()) {
      id = &i->second;
    }
    auto i2 = uuid_index_.find(*id);
    if (i2 == uuid_index_.end())
      return nullptr;
    return i2->second;
  }

  bool MapLocaleIdToUuid(base::Uuid& id) const {
    auto i = locale_id_uuid_map_.find(id);
    if (i == locale_id_uuid_map_.end())
      return false;
    id = i->second;
    return true;
  }

 private:
  bool ParseJson(base::Value root, base::Value partners_locale_value);

  std::vector<PartnerDetails> details_list_;

  // Map partner details name to its details.
  base::flat_map<std::string_view, const PartnerDetails*> name_index_;

  // Map locale-independent uuid or uuid2 to its details.
  base::flat_map<base::Uuid, const PartnerDetails*> uuid_index_;

  // Map old locale-based partner id to the uuid or uuid2 if the old id is for
  // an url under Bookmarks folder.
  base::flat_map<base::Uuid, base::Uuid> locale_id_uuid_map_;
};

// Global singleton.
const PartnerDatabase* g_partner_db = nullptr;

// static
std::unique_ptr<PartnerDatabase> PartnerDatabase::Read() {
  std::optional<base::Value> partner_db_value =
      ResourceReader::ReadJSON(GetBookmarkResourceDir(), kPartnerDBFile);
  if (!partner_db_value)
    return nullptr;

  std::optional<base::Value> partners_locale_value =
      ResourceReader::ReadJSON(GetBookmarkResourceDir(), kPartnerLocaleMapFile);
  if (!partners_locale_value)
    return nullptr;

  auto db = std::make_unique<PartnerDatabase>();
  if (!db->ParseJson(std::move(*partner_db_value),
                     std::move(*partners_locale_value)))
    return nullptr;

  return db;
}

bool PartnerDatabase::ParseJson(base::Value root_value,
                                base::Value partners_locale_value) {
  auto error = [](std::string_view message) -> bool {
    LOG(ERROR) << "Partner database JSON error: " << message;
    return false;
  };

  base::Value::Dict* root_dict = root_value.GetIfDict();
  if (!root_dict)
    return error("partner db json is not an object");

  base::Value::List* folders = root_dict->FindList(kFoldersKey);
  if (!folders)
    return error(std::string("missing ") + kFoldersKey + " key");

  base::Value::List* bookmarks = root_dict->FindList(kBookmarksKey);
  if (!bookmarks)
    return error(std::string("missing ") + kBookmarksKey + " key");

  details_list_.reserve(folders->size() + bookmarks->size());
  if (!ParsePartnerDatabaseDetailsList(true, *folders, details_list_))
    return false;
  if (!ParsePartnerDatabaseDetailsList(false, *bookmarks, details_list_))
    return false;

  // Establish the index now that we no longer mutate details and check
  // for unique values.
  std::vector<std::pair<std::string_view, const PartnerDetails*>> names;
  names.reserve(details_list_.size());
  std::vector<std::pair<base::Uuid, const PartnerDetails*>> uuids;
  uuids.reserve(details_list_.size() * 2);
  for (const PartnerDetails& details : details_list_) {
    names.emplace_back(details.name, &details);
    uuids.emplace_back(details.uuid, &details);
    if (details.uuid2.is_valid()) {
      uuids.emplace_back(details.uuid2, &details);
    }
  }
  name_index_ =
      base::flat_map<std::string_view, const PartnerDetails*>(
      std::move(names));
  if (name_index_.size() != details_list_.size())
    return error("duplicated names");
  size_t added_uuids = uuids.size();
  uuid_index_ =
      base::flat_map<base::Uuid, const PartnerDetails*>(std::move(uuids));
  if (uuid_index_.size() != added_uuids)
    return error("duplicated Uuids");

  // Parse mapping from old locale-based ids to the new universal ids.
  std::vector<std::pair<base::Uuid, base::Uuid>> locale_id_uuid_list;
  if (!partners_locale_value.is_dict())
    return error("partner local map json is not an object");
  for (auto name_key_value : partners_locale_value.GetDict()) {
    const std::string& name = name_key_value.first;
    const PartnerDetails* details = FindDetailsByName(name);
    if (!details)
      return error(std::string("'") + name + "' from " + kPartnerLocaleMapFile +
                   " is not defined in " + kGuidKey);
    base::Value& locale_dict = name_key_value.second;
    if (!locale_dict.is_dict())
      return error(std::string(kPartnerLocaleMapFile) + "." + name +
                   " is not a dictionary");

    for (auto uuid_key_value : locale_dict.GetDict()) {
      const std::string& guid_key = uuid_key_value.first;
      base::Value& v = uuid_key_value.second;
      base::Uuid uuid;
      if (guid_key == kGuidKey) {
        uuid = details->uuid;
      } else if (guid_key == kGuid2Key) {
        uuid = details->uuid2;
      } else {
        return error(std::string("unknown key ") + guid_key + " in " +
                     kPartnerLocaleMapFile + "." + name);
      }
      if (!v.is_list())
        return error(std::string(kPartnerLocaleMapFile) + "." + name + "." +
                     guid_key + " is not a list");
      for (base::Value& id_value : v.GetList()) {
        if (!id_value.is_string())
          return error(std::string("Partner id in ") +
                       std::string(kPartnerLocaleMapFile) + "." + name + "." +
                       guid_key + " is not a string");
        base::Uuid locale_id =
            base::Uuid::ParseCaseInsensitive(id_value.GetString());
        if (!locale_id.is_valid()) {
          return error(std::string("Partner id in ") +
                       std::string(kPartnerLocaleMapFile) + "." + name + "." +
                       guid_key + " is not a valid Uuid - " +
                       id_value.GetString());
        }
        locale_id_uuid_list.emplace_back(std::move(locale_id), std::move(uuid));
      }
    }
  }
  locale_id_uuid_map_ =
      base::flat_map<base::Uuid, base::Uuid>(std::move(locale_id_uuid_list));
  return true;
}

}  // namespace

const PartnerDetails* FindDetailsByName(std::string_view name) {
  if (!g_partner_db)
    return nullptr;
  return g_partner_db->FindDetailsByName(name);
}

bool MapLocaleIdToUuid(base::Uuid& id) {
  if (!g_partner_db)
    return false;
  return g_partner_db->MapLocaleIdToUuid(id);
}

const std::string& GetThumbnailUrl(const base::Uuid& partner_id) {
  DCHECK(partner_id.is_valid());
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