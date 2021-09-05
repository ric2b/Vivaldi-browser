// Copyright 2019 Vivaldi Technologies. All rights reserved.

#include "browser/vivaldi_default_bookmarks.h"

#if defined(OS_ANDROID)
#include "base/android/apk_assets.h"
#endif
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "ui/base/models/tree_node_iterator.h"

#include "components/bookmarks/vivaldi_bookmark_kit.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

namespace vivaldi_default_bookmarks {

bool g_bookmark_update_actve = false;

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {

// This will enable code that, on an update, will remove any installed partner
// bookmarks that are no longer specified in the default collection. Currently
// we do not want this functionality.
constexpr bool kAllowPartnerRemoval = false;

#if defined(OS_ANDROID)
const char kAssetsDir[] = "assets/default-bookmarks/";
#else
#if !defined(OS_MACOSX)
const base::FilePath::CharType kResources[] = FILE_PATH_LITERAL("resources");
#endif
const base::FilePath::CharType kVivaldi[] = FILE_PATH_LITERAL("vivaldi");
const base::FilePath::CharType kDefBookmarks[] =
    FILE_PATH_LITERAL("default-bookmarks");
#endif  // !OS_ANDROID

const char kFallbackLocaleFile[] = "en-US.json";
const char kPartnerDBFile[] = "partners.json";
const char kPartnerLocaleMapFile[] = "partners-locale-map.json";

const char kFoldersKey[] = "folders";
const char kGuidKey[] = "guid";
const char kSpeeddialKey[] = "speeddial";

const char kChildrenKey[] = "children";
const char kNameKey[] = "name";
const char kDescriptionKey[] = "description";
const char kNicknameKey[] = "nickname";
const char kPartnerKey[] = "partner";
const char kThumbNailKey[] = "thumbnail";
const char kTitleKey[] = "title";
const char kURLKey[] = "url";
const char kVersionKey[] = "version";

const char kPartnerThumbnailUrlPrefix[] = "/resources/";

struct PartnerDetails {
  std::string name;
  std::string title;
  std::string guid;
  bool speeddial = false;
  std::vector<std::string> old_partner_ids;
};

struct PartnerDatabase {

  PartnerDetails* FindDetailsByName(base::StringPiece name) {
    auto i = name_index.find(name);
    if (i == name_index.end())
      return nullptr;
    return &details[i->second];
  }

  std::vector<PartnerDetails> details;

  // Map partner details name to its index in the details array.
  std::map<base::StringPiece, size_t> name_index;
};

struct DefaultBookmarkItem {
  std::string title;
  std::string nickname;
  std::string partner_id;
  std::string thumbnail;
  std::string description;
  GURL url;
  bool speeddial = false;
  std::vector<DefaultBookmarkItem> children;
};

struct DefaultBookmarkTree {
  PartnerDatabase partner_db;
  bool valid = false;
  std::string version;
  std::vector<DefaultBookmarkItem> top_items;
  std::set<std::string> partner_ids;
};

class BookmarkUpdater {
 public:
  struct Stats {
    int added_folders = 0;
    int added_urls = 0;
    int updated_folders = 0;
    int updated_urls = 0;
    int removed = 0;
    int failed_updates = 0;
  };

  BookmarkUpdater(const DefaultBookmarkTree* default_bookmark_tree,
                  BookmarkModel* model);

  void SetDeletedPartners(const base::Value* deleted_partners);

  void RunCleanUpdate();

  const Stats& stats() const { return stats_; }

 private:
  void CleanUpdateRecursively(
      const std::vector<DefaultBookmarkItem>& default_items,
      const BookmarkNode* parent_node);
  void FindExistingPartners(const BookmarkNode* top_node);
  void UpdatePartnerNode(const DefaultBookmarkItem& item,
                         const BookmarkNode* node);
  const BookmarkNode* AddPartnerNode(const DefaultBookmarkItem& item,
                                     const BookmarkNode* parent_node);

  const DefaultBookmarkTree* default_bookmark_tree_;
  BookmarkModel* model_;
  base::flat_set<std::string> deleted_partner_ids_;

  // For named partners map its old locale-based partner id to the guid.
  base::flat_map<base::StringPiece, base::StringPiece> locale_id_guid_map_;
  std::map<std::string, const BookmarkNode*> guid_node_map_;
  std::map<std::string, const BookmarkNode*> existing_partner_bookmarks_;
  Stats stats_;
};

bool HasPartnerThumbnailPrefix(base::StringPiece url) {
  return url.starts_with(kPartnerThumbnailUrlPrefix);
}

bool IsValidPartnerName(base::StringPiece name) {
  if (name.empty())
    return false;

  // The name must use only latin letters, digits and underscores and must not
  // start with a digit.
  for (size_t i = 0; i < name.size(); ++i) {
    char c = name[i];
    if (base::IsAsciiAlpha(c) || c == '_')
      continue;
    if (i > 0 && base::IsAsciiDigit(c))
      continue;
    return false;
  }

  return true;
}

bool ParsePartnerDatabaseDetailsList(base::Value* list_value,
                                     PartnerDatabase* db) {
  DCHECK(list_value->is_list());
  auto list = list_value->GetList();
  for (size_t i = 0; i < list.size(); ++i) {
    auto error = [&](base::StringPiece message) -> bool {
      LOG(ERROR) << "Partner database JSON error: bad format of " << kFoldersKey
                 << "[" << i << "] - " << message;
      return false;
    };
    base::Value& dict = list[i];
    if (!dict.is_dict())
      return error("entry is not an object");
    PartnerDetails details;
    for (auto property_key_value : dict.DictItems()) {
      const std::string& property = property_key_value.first;
      base::Value& v = property_key_value.second;
      if (property == kNameKey) {
        if (!v.is_string())
          return error(property + " is not a string");
        details.name = std::move(v.GetString());
        if (!IsValidPartnerName(details.name))
          return error(property + " is not a valid partner name - " +
                       details.name);
      } else if (property == kTitleKey) {
        if (!v.is_string())
          return error(property + " is not a string");
        details.title = std::move(v.GetString());
      } else if (property == kGuidKey) {
        if (!v.is_string())
          return error(property + " is not a string");
        details.guid = std::move(v.GetString());
        if (!base::IsValidGUID(details.guid))
          return error(property + " is not a valid GUID - " + details.guid);
      } else if (property == kSpeeddialKey) {
        if (!v.is_bool())
          return error(property + " is not a boolean");
        details.speeddial = v.GetBool();
      } else {
        return error("unknown property '" + property + "'");
      }
    }
    if (details.name.empty())
      return error(std::string("missing ") + kNameKey + " property");
    if (details.title.empty()) {
      details.title = details.name;
    }
    if (details.guid.empty())
      return error(std::string("missing ") + kGuidKey + " property");
    db->details.push_back(std::move(details));
  }
  return true;
}

bool ParsePartnerDatabaseJson(base::Value root,
                              base::Value partners_locale_value,
                              PartnerDatabase* db) {
  auto error = [](base::StringPiece message) -> bool {
    LOG(ERROR) << "Partner database JSON error: " << message;
    return false;
  };
  if (!root.is_dict())
    return error("partner db json is not an object");

  base::Value* folders_value = root.FindListKey(kFoldersKey);
  if (!folders_value)
    return error(std::string("missing ") + kFoldersKey + " key");

  db->details.reserve(folders_value->GetList().size());
  if (!ParsePartnerDatabaseDetailsList(folders_value, db))
    return false;

  // Establish index now that we no longer mutate details and check
  // for unique values.
  std::set<base::StringPiece> guid_set;
  for (size_t i = 0; i < db->details.size(); ++i) {
    const PartnerDetails& details = db->details[i];
    if (!db->name_index.emplace(details.name, i).second)
      return error(std::string("duplicated partner name - ") + details.name);
    if (!guid_set.emplace(details.guid).second)
      return error(std::string("duplicated GUID - ") + details.guid);
  }

  if (!partners_locale_value.is_dict())
    return error("partner local map json is not an object");
  for (auto name_key_value : partners_locale_value.DictItems()) {
    const std::string& name = name_key_value.first;
    PartnerDetails* details = db->FindDetailsByName(name);
    if (!details)
      return error(std::string("'") + name + "' from " + kPartnerLocaleMapFile +
                   " is not defined in " + kGuidKey);
    base::Value& locale_dict = name_key_value.second;
    if (!locale_dict.is_dict())
      return error(std::string(kPartnerLocaleMapFile) + "." + name +
                   " is not a dictionary");
    std::vector<std::string>& partner_ids = details->old_partner_ids;
    partner_ids.resize(locale_dict.DictSize());
    for (auto locale_key_value : locale_dict.DictItems()) {
      const std::string& partner_id = locale_key_value.first;
      if (!base::IsValidGUID(partner_id))
        return error(std::string("Partner id in ") +
                     std::string(kPartnerLocaleMapFile) + "." + name +
                     " is not a valid GUID - " + partner_id);
      base::Value& partner_id_value = locale_key_value.second;
      if (!partner_id_value.is_string())
        return error(std::string(kPartnerLocaleMapFile) + "." + name + "." +
                     partner_id + " is not a string");
      const std::string& locale = partner_id_value.GetString();
      bool valid_locale = false;
      if (locale.size() >= 2) {
        if (base::IsAsciiLower(locale[0]) && base::IsAsciiLower(locale[1])) {
          if (locale.size() == 2) {
            valid_locale = true;
          } else if (locale.size() == 5) {
            valid_locale = (locale[2] == '-' && base::IsAsciiUpper(locale[3]) &&
                            base::IsAsciiUpper(locale[4]));
          }
        }
      }
      if (!valid_locale)
        return error(std::string("The value of ") + kPartnerLocaleMapFile + "." + partner_id +
                     " is not a valid locale name - " + locale);
      partner_ids.push_back(std::move(partner_id));
    }
  }
  return true;
}

void ParseDefaultJsonBookmarkList(
    int level,
    base::Value* value_list,
    std::vector<DefaultBookmarkItem>* default_items,
    DefaultBookmarkTree* tree) {
  if (!value_list->is_list()) {
    tree->valid = false;
    LOG(ERROR) << kChildrenKey << " is not an array.";
    return;
  }
  if (level > 5) {
    tree->valid = false;
    LOG(ERROR) << "too deaply nested default bookmarks";
    return;
  }

  for (base::Value& dict : value_list->GetList()) {
    if (!dict.is_dict()) {
      tree->valid = false;
      LOG(ERROR) << "a child of " << kChildrenKey << " is not a dictionary";
      continue;
    }

    // The flag becomes false on unrecoverable errors.
    bool skip_node = false;
    DefaultBookmarkItem item;

    base::Value* children = dict.FindKey(kChildrenKey);
    if (children) {
      // Folder
      const PartnerDetails* details = nullptr;
      if (std::string* value = dict.FindStringKey(kNameKey)) {
        details = tree->partner_db.FindDetailsByName(*value);
      }
      if (!details) {
        tree->valid = false;
        skip_node = true;
        LOG(ERROR) << "bookmark folder with missing or unknown name";
      } else {
        item.partner_id = details->guid;
        item.title = details->title;
        item.speeddial = details->speeddial;
      }
      ParseDefaultJsonBookmarkList(level + 1, children, &item.children, tree);
    } else {
      // Bookmark URL
      if (level == 0) {
        LOG(ERROR) << "top-level bookmark " << item.title << "is not a folder";
        tree->valid = false;
      }
      if (std::string* value = dict.FindStringKey(kTitleKey)) {
        item.title = std::move(*value);
      }
      if (item.title.empty()) {
        // Node must have a title.
        skip_node = true;
        tree->valid = false;
        LOG(ERROR) << "bookmark without title";
      }

      if (std::string* value = dict.FindStringKey(kNicknameKey)) {
        item.nickname = std::move(*value);
      }

      if (std::string* value = dict.FindStringKey(kPartnerKey)) {
        item.partner_id = std::move(*value);
      }
      if (item.partner_id.empty()) {
        tree->valid = false;
        LOG(ERROR) << "no partner id for bookmark " << item.title;
      } else if (!base::IsValidGUID(item.partner_id)) {
        tree->valid = false;
        LOG(ERROR) << "the partner for bookmark " << item.title
                   << " is not a valid GUID: " << item.partner_id;
      }

      if (std::string* value = dict.FindStringKey(kThumbNailKey)) {
        if (!value->empty()) {
          if (HasPartnerThumbnailPrefix(*value)) {
            item.thumbnail = std::move(*value);
          } else {
            tree->valid = false;
            LOG(ERROR) << "bookmark " << item.title << " thumbnail URL "
                       << *value << " does not starts with "
                       << kPartnerThumbnailUrlPrefix;
          }
        }
      }

      if (std::string* value = dict.FindStringKey(kDescriptionKey)) {
        item.description = std::move(*value);
      }

      if (std::string* value = dict.FindStringKey(kURLKey)) {
        if (!value->empty()) {
          GURL url(*value);
          if (url.is_valid()) {
            item.url = std::move(url);
          } else {
            tree->valid = false;
            LOG(ERROR) << kURLKey << " for bookmark " << item.title
                       << " is not a valid URL: " << *value;
          }
        }
      } else {
        skip_node = true;
        LOG(ERROR) << "bookmark " << item.title << " without " << kURLKey;
        tree->valid = false;
      }
    }

    if (!item.partner_id.empty()) {
      auto inserted = tree->partner_ids.insert(item.partner_id);
      if (!inserted.second) {
        tree->valid = false;
        LOG(ERROR) << "the partner for bookmark " << item.title
                    << " is duplicated: " << item.partner_id;
      }
    }

    if (!skip_node) {
      default_items->push_back(std::move(item));
    }
  }
}

DefaultBookmarkTree ParseDefaultBookmarkJson(
    base::Value partner_db_value,
    base::Value partners_locale_value,
    base::Value default_bookmarks_value) {
  DefaultBookmarkTree tree;
  do {
    if (!ParsePartnerDatabaseJson(std::move(partner_db_value),
                                  std::move(partners_locale_value),
                                  &tree.partner_db))
      break;

    if (!default_bookmarks_value.is_dict()) {
      LOG(ERROR) << "default bookmark json is not an object";
      break;
    }
    std::string* version = default_bookmarks_value.FindStringKey(kVersionKey);
    if (!version || version->empty()) {
      LOG(ERROR) << "no " << kVersionKey << " in json";
      break;
    }
    tree.version = std::move(*version);

    base::Value* bookmark_list = default_bookmarks_value.FindKey(kChildrenKey);
    if (!bookmark_list) {
      LOG(ERROR) << "no " << kChildrenKey << " array.";
      break;
    }

    tree.valid = true;
    ParseDefaultJsonBookmarkList(0, bookmark_list, &tree.top_items, &tree);
  } while (false);

  return tree;
}

struct AssetReadState {
#if defined(OS_ANDROID)
  std::string path;
#else
  base::FilePath asset_dir;
  base::FilePath path;
#endif  // OS_ANDROID
  bool not_found = false;
};

base::Optional<base::Value> ReadJsonAsset(AssetReadState* state,
                                          base::StringPiece name) {
  state->not_found = false;
  base::StringPiece json_text;
#if defined(OS_ANDROID)
  state->path = kAssetsDir;
  state->path.append(name.data(), name.length());

  base::MemoryMappedFile::Region region;
  int json_fd = base::android::OpenApkAsset(state->path, &region);
  if (json_fd < 0) {
    state->not_found = true;
    return base::nullopt;
  }
  base::MemoryMappedFile mapped_file;
  if (!mapped_file.Initialize(base::File(json_fd), region)) {
    LOG(ERROR) << "failed to initialize memory mapping for " << state->path;
    return base::nullopt;
  }
  json_text = base::StringPiece(reinterpret_cast<char*>(mapped_file.data()),
                                mapped_file.length());
#else
  if (state->asset_dir.empty()) {
    base::PathService::Get(base::DIR_ASSETS, &state->asset_dir);
#if !defined(OS_MACOSX)
    state->asset_dir = state->asset_dir.Append(kResources);
#endif  // if !defined(OS_MACOSX)
    state->asset_dir = state->asset_dir.Append(kVivaldi).Append(kDefBookmarks);
  }
  state->path = state->asset_dir.AppendASCII(name);
  if (!base::PathExists(state->path)) {
    state->not_found = true;
    return base::nullopt;
  }
  std::string json_buffer;
  if (!base::ReadFileToString(state->path, &json_buffer)) {
    LOG(ERROR) << "failed to read " << state->path;
    return base::nullopt;
  }
  json_text = base::StringPiece(json_buffer);
#endif  //  OS_ANDROID

  return base::JSONReader::Read(json_text);
}

DefaultBookmarkTree ReadDefaultBookmarks(std::string locale) {
  AssetReadState state;
  base::Optional<base::Value> partner_db_value =
      ReadJsonAsset(&state, kPartnerDBFile);
  if (!partner_db_value && state.not_found) {
    LOG(ERROR) << "missing partner DB file " << state.path;
  }

  // TODO(igor@vivaldi.com): Read this only if we are going to update bookmarks.
  // This will be actual when the partner database will be expanded to cover
  // all bookmarks, not only bookmark folders.
  base::Optional<base::Value> partners_locale_value =
      ReadJsonAsset(&state, kPartnerLocaleMapFile);
  if (!partners_locale_value && state.not_found) {
    LOG(ERROR) << "missing partner locale file " << state.path;
  }

  base::Optional<base::Value> default_bookmarks_value =
      ReadJsonAsset(&state, locale + ".json");
  if (!default_bookmarks_value) {
    if (state.not_found) {
      LOG(WARNING) << "missing default bookmarks file " << state.path;
      default_bookmarks_value = ReadJsonAsset(&state, kFallbackLocaleFile);
      if (!default_bookmarks_value) {
        LOG(ERROR) << "missing fallback bookmark file " << state.path;
      }
    }
  }

  if (!partner_db_value || !partners_locale_value || !default_bookmarks_value) {
    return DefaultBookmarkTree();
  }

  return ParseDefaultBookmarkJson(std::move(*partner_db_value),
                                  std::move(*partners_locale_value),
                                  std::move(*default_bookmarks_value));
}

BookmarkUpdater::BookmarkUpdater(
    const DefaultBookmarkTree* default_bookmark_tree,
    BookmarkModel* model)
    : default_bookmark_tree_(default_bookmark_tree), model_(model) {
  std::vector<std::pair<base::StringPiece, base::StringPiece>>
      locale_id_guid_list;
  for (const PartnerDetails& details :
       default_bookmark_tree_->partner_db.details) {
    for (const std::string& locale_id : details.old_partner_ids) {
      locale_id_guid_list.emplace_back(locale_id, details.guid);
    }
  }
  locale_id_guid_map_ = base::flat_map<base::StringPiece, base::StringPiece>(
      std::move(locale_id_guid_list));
}

void BookmarkUpdater::SetDeletedPartners(const base::Value* deleted_partners) {
  if (!deleted_partners->is_list()) {
    LOG(ERROR) << vivaldiprefs::kBookmarksDeletedPartners
               << " is no a list value";
    return;
  }

  std::vector<std::string> ids;
  ids.reserve(deleted_partners->GetList().size());
  for (const base::Value& value : deleted_partners->GetList()) {
    if (!value.is_string()) {
      LOG(ERROR) << vivaldiprefs::kBookmarksDeletedPartners
                 << " contains non-string entry";
      continue;
    }
    const std::string& partner_id = value.GetString();
    auto i = locale_id_guid_map_.find(partner_id);
    if (i != locale_id_guid_map_.end()) {
      // This is an old locale-based id for a named partner. Push its GUID.
      ids.push_back(i->second.as_string());
    } else {
      ids.push_back(partner_id);
    }
  }

  deleted_partner_ids_ = base::flat_set<std::string>(std::move(ids));
}

void BookmarkUpdater::RunCleanUpdate() {
  DCHECK(!g_bookmark_update_actve);
  g_bookmark_update_actve = true;

  FindExistingPartners(model_->bookmark_bar_node());
  FindExistingPartners(model_->trash_node());

  CleanUpdateRecursively(default_bookmark_tree_->top_items,
                         model_->bookmark_bar_node());

  if (kAllowPartnerRemoval) {
    // A range loop cannot be used as we remove elements from the map not to
    // keep pointers to deleted nodes.
    for (auto i = existing_partner_bookmarks_.begin();
         i != existing_partner_bookmarks_.end();) {
      const std::string& partner_id = i->first;
      auto found = default_bookmark_tree_->partner_ids.find(partner_id);
      if (found == default_bookmark_tree_->partner_ids.end()) {
        const BookmarkNode* node = i->second;
        if (!model_->is_permanent_node(node)) {
          VLOG(2) << "Removing non-existing partner title=" << node->GetTitle()
                  << " guid=" << node->guid();
          i = existing_partner_bookmarks_.erase(i);
          model_->Remove(node);
          continue;
        }
      }
      i++;
    }
  }

  DCHECK(g_bookmark_update_actve);
  g_bookmark_update_actve = false;
}

void BookmarkUpdater::FindExistingPartners(const BookmarkNode* top_node) {
  ui::TreeNodeIterator<const BookmarkNode> iterator(top_node);
  while (iterator.has_next()) {
    const BookmarkNode* node = iterator.Next();
    auto inserted_guid = guid_node_map_.emplace(node->guid(), node);
    if (!inserted_guid.second) {
      // TODO(igor@vavaldi.com): Chromium in debug builds isists that all GUIDs
      // are unique. Find out if they absolutely rules them out and do not
      // bother with this check.
      LOG(ERROR) << "Duplicated GUID " << node->guid();
      DCHECK(false);
      continue;
    }
    std::string partner_id = vivaldi_bookmark_kit::GetPartner(node);
    if (partner_id.empty())
      continue;
    auto guid = locale_id_guid_map_.find(partner_id);
    if (guid != locale_id_guid_map_.end()) {
      partner_id = guid->second.as_string();
    }
    if (!existing_partner_bookmarks_.emplace(partner_id, node).second) {
      LOG(WARNING) << "Duplicated partner " << partner_id;
    }
  }
}

void BookmarkUpdater::CleanUpdateRecursively(
    const std::vector<DefaultBookmarkItem>& default_items,
    const BookmarkNode* parent_node) {
  for (const DefaultBookmarkItem& item : default_items) {
    const BookmarkNode* recursion_parent;
    auto iter = existing_partner_bookmarks_.find(item.partner_id);
    if (iter != existing_partner_bookmarks_.end()) {
      const BookmarkNode* node = iter->second;
      // The partner still exists in bookmarks.
      UpdatePartnerNode(item, node);
      recursion_parent = node;
    } else {
      // The partner item did not match a corresponding node in the tree. For
      // leaf items we check if the corresponding node was deleted. If so,
      // we skip it. If not, we create a node for it.
      //
      // For folder items we also need to deal with their children as a folder
      // may have loosen its partner status after the user editted it without
      // affecting the partner status of children. If we have a match by guid,
      // this should be the folder. Otherwise to deal with older locale-specific
      // partners we check if any of folder's child items exists in the bookmark
      // tree. If we find such item, we guess that its parent was the original
      // partner folder and update children there.
      const BookmarkNode* guessed_old_folder = nullptr;
      auto iter = guid_node_map_.find(item.partner_id);
      if (iter != guid_node_map_.end()) {
        // Exact match by GUID.
        guessed_old_folder = iter->second;
        VLOG(1) << "Guessed parent by GUID match, title="
                << guessed_old_folder->GetTitle()
                << " guid=" << guessed_old_folder->guid();
      }
      if (!guessed_old_folder) {
        for (const DefaultBookmarkItem& child_item : item.children) {
          auto iter = existing_partner_bookmarks_.find(child_item.partner_id);
          if (iter != existing_partner_bookmarks_.end()) {
            guessed_old_folder = iter->second->parent();
            VLOG(1) << "Guessed parent from a child, title="
                    << guessed_old_folder->GetTitle()
                    << " guid=" << guessed_old_folder->guid();
            break;
          }
        }
      }

      if (guessed_old_folder) {
        recursion_parent = guessed_old_folder;
      } else if (deleted_partner_ids_.contains(item.partner_id)) {
        VLOG(2) << "Skipping deleted partner name=" << item.title
                << " partner_id=" << item.partner_id;
        continue;
      } else {
        recursion_parent = AddPartnerNode(item, parent_node);
      }
    }
    if (!item.children.empty()) {
      CleanUpdateRecursively(item.children, recursion_parent);
    }
  }
}

const BookmarkNode* BookmarkUpdater::AddPartnerNode(
    const DefaultBookmarkItem& item,
    const BookmarkNode* parent_node) {
  vivaldi_bookmark_kit::CustomMetaInfo custom_meta;
  custom_meta.SetNickname(item.nickname);
  custom_meta.SetPartner(item.partner_id);
  custom_meta.SetThumbnail(item.thumbnail);
  custom_meta.SetDescription(item.description);
  custom_meta.SetSpeeddial(item.speeddial);

  base::string16 title = base::UTF8ToUTF16(item.title);
  size_t index = parent_node->children().size();
  const BookmarkNode* node;
  if (item.url.is_empty()) {
    VLOG(2) << "Adding folder " << item.title
            << " guid=" << item.partner_id;
    node = model_->AddFolder(parent_node, index, title, custom_meta.map(),
                             item.partner_id);
    stats_.added_folders++;
  } else {
    VLOG(2) << "Adding url " << item.title << " guid=" << item.partner_id;
    node = model_->AddURL(parent_node, index, title, item.url,
                          custom_meta.map(), base::nullopt, item.partner_id);
    stats_.added_urls++;
  }
  return node;
}

void BookmarkUpdater::UpdatePartnerNode(const DefaultBookmarkItem& item,
                                        const BookmarkNode* node) {
  auto error = [&](base::StringPiece message) -> void {
    LOG(ERROR) << message << " - " << item.title;
    stats_.failed_updates++;
  };

  if (model_->is_permanent_node(node))
    return error("Partner became a permamnent node");

  if (item.url.is_empty()) {
    if (!node->is_folder())
      return error("Partner folder became a bookmark url");
  } else {
    if (node->is_folder())
      return error("Partner url became a bookmark folder");
  }

  VLOG(2) << "Updatning " << item.title << " guid=" << node->guid();
  model_->SetTitle(node, base::UTF8ToUTF16(item.title));
  if (!item.url.is_empty()) {
    model_->SetURL(node, item.url);
  }

  vivaldi_bookmark_kit::CustomMetaInfo custom_meta;
  const BookmarkNode::MetaInfoMap *old_meta_info = node->GetMetaInfoMap();
  if (old_meta_info) {
    custom_meta.SetMap(*old_meta_info);
  }

  // Make sure the node uses the stable partner guid, not one of older
  // locale-specific partner ids.
  custom_meta.SetPartner(item.partner_id);

  // If nick is taken by other node do nothing. But ensure that it is cleared
  // if the nick in defaults is empty.
  if (item.nickname.empty() ||
      !::vivaldi_bookmark_kit::DoesNickExists(model_, item.nickname, node)) {
    custom_meta.SetNickname(item.nickname);
  }
  // We do not clear the partner status when the user selects a custom
  // thumbnail or uses a page snapshot as a thumbnail. So update the
  // thumbnail only if still points to the partner image.
  if (HasPartnerThumbnailPrefix(::vivaldi_bookmark_kit::GetThumbnail(node))) {
    custom_meta.SetThumbnail(item.thumbnail);
  }
  custom_meta.SetDescription(item.description);
  custom_meta.SetSpeeddial(item.speeddial);

  model_->SetNodeMetaInfoMap(node, *custom_meta.map());
  if (node->is_url()) {
    stats_.updated_urls++;
  } else {
    stats_.updated_folders++;
  }
}

void UpdatePartnersInModel(Profile* profile,
                           const std::string& locale,
                           DefaultBookmarkTree default_tree,
                           UpdateCallback callback,
                           BookmarkModel* model) {
  bool ok = false;
  bool no_version = false;
  do {
    if (!model)
      break;

    if (!model->bookmark_bar_node() || !model->trash_node()) {
      LOG(ERROR)
          << "the top node for bookmarks or trash nodes are not available";
      break;
    }

    if (!default_tree.valid) {
      LOG(ERROR) << "default bookmarks cannot be updated as their definition "
                    "is not valid.";
      break;
    }

    PrefService* prefs = profile->GetPrefs();
    std::string prev_update_version =
        prefs->GetString(vivaldiprefs::kBookmarksVersion);
    if (prev_update_version.empty()) {
      // If the bookmark model has any nodes including those in trash this is
      // probably an older setup where we did not have a version. Delegate to JS
      // to update based on url matches.
      if (!model->bookmark_bar_node()->children().empty() ||
          !model->trash_node()->children().empty()) {
        // This is probably an older setup where we did not have a version.
        // Delegate to JS to update based on url matches.
        LOG(WARNING) << "bookmarks without the partner version information";
        no_version = true;
        break;
      }
    } else {
      if (default_tree.version == prev_update_version) {
        bool force_update =
            prefs->GetBoolean(vivaldiprefs::kBiscuitLoadBookmarks);
        if (!force_update) {
          ok = true;
          break;
        }
      }
    }

    BookmarkUpdater upgrader(&default_tree, model);
    upgrader.SetDeletedPartners(
        prefs->GetList(vivaldiprefs::kBookmarksDeletedPartners));
    upgrader.RunCleanUpdate();

    const BookmarkUpdater::Stats& stats = upgrader.stats();
    int changed = stats.added_folders + stats.added_urls +
                  stats.updated_folders + stats.updated_urls + stats.removed;
    int skipped = static_cast<int>(default_tree.partner_ids.size()) - changed -
                  stats.failed_updates;
    LOG(INFO) << " added_folders=" << stats.added_folders
              << " added_urls=" << stats.added_urls
              << " updated_folders=" << stats.updated_folders
              << " updated_urls=" << stats.updated_urls
              << " removed=" << stats.removed
              << " skipped=" << skipped;

    if (stats.failed_updates) {
      LOG(ERROR) << " failed_updates=" << stats.failed_updates;
      break;
    }

    prefs->SetString(vivaldiprefs::kBookmarksVersion, default_tree.version);
    ok = true;

  } while (false);

  if (callback) {
    std::move(callback).Run(ok, no_version);
  }
}

void UpdatePartnersFromDefaults(Profile* profile,
                                const std::string& locale,
                                UpdateCallback callback,
                                DefaultBookmarkTree default_tree) {
  // Unretained is safe as recording profiles are never removed.
  vivaldi_bookmark_kit::RunAfterModelLoad(
      profile ? BookmarkModelFactory::GetForBrowserContext(profile) : nullptr,
      base::BindOnce(&UpdatePartnersInModel, base::Unretained(profile), locale,
                     std::move(default_tree), std::move(callback)));
}

}  // namespace

void UpdatePartners(Profile* profile,
                    const std::string& locale,
                    UpdateCallback callback) {
  if (locale.length() != 2 && locale.length() != 5) {
    LOG(WARNING) << "unexpected locale format: " << locale;
  }

  // Should not be used for incognito profile. That can be closed when
  // various callbacks still runs.
  if (profile->GetOriginalProfile() != profile) {
    profile = nullptr;
    LOG(ERROR) << "Attempt to update bookmarks from incognito or guest window";
  }

  // Unretained() is safe as recording profiles are not deleted until shutdown.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ReadDefaultBookmarks, locale),
      base::BindOnce(&UpdatePartnersFromDefaults, base::Unretained(profile),
                     locale, std::move(callback)));
}

}  // namespace vivaldi_default_bookmarks
