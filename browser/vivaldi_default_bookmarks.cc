// Copyright 2019 Vivaldi Technologies. All rights reserved.

#include "browser/vivaldi_default_bookmarks.h"

#include "base/bind.h"
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
#include "components/bookmarks/vivaldi_partners.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

namespace vivaldi_default_bookmarks {

bool g_bookmark_update_actve = false;

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

using vivaldi_partners::PartnerDetails;

namespace {

// This will enable code that, on an update, will remove any installed partner
// bookmarks that are no longer specified in the default collection. Currently
// we do not want this functionality.
constexpr bool kAllowPartnerRemoval = false;

// If this is true, older locale-specific partner GUIDs will be updated to newer
// locale-independent ones. This is false for now to avoid suprises when syncing
// between profiles using newer version of Vivaldi and older one that knows only
// about locale-based partners.
constexpr bool kUpdateLocaleBasedPartners = false;

const char kFallbackLocaleFile[] = "en-US.json";

const char kChildrenKey[] = "children";
const char kNameKey[] = "name";
const char kDescriptionKey[] = "description";
const char kNicknameKey[] = "nickname";
const char kTitleKey[] = "title";
const char kURLKey[] = "url";
const char kVersionKey[] = "version";

const char kBookmarksFolderName[] = "Bookmarks";

struct DefaultBookmarkItem {
  std::string title;
  std::string nickname;
  std::string guid;
  std::string thumbnail;
  std::string description;
  GURL url;
  bool speeddial = false;
  std::vector<DefaultBookmarkItem> children;
};

struct DefaultBookmarkTree {
  bool valid = false;
  std::string version;
  std::vector<DefaultBookmarkItem> top_items;
  size_t item_count = 0;
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

  void SetDeletedPartners(PrefService* prefs);

  void RunCleanUpdate();

  const Stats& stats() const { return stats_; }

 private:
  void UpdateRecursively(
      const std::vector<DefaultBookmarkItem>& default_items,
      const BookmarkNode* parent_node);

  void FindExistingPartners(const BookmarkNode* top_node);

  // Find existing bookmark node and update it or add a new one. Return null
  // if item's children should be skipped.
  const BookmarkNode* UpdateOrAdd(
      const DefaultBookmarkItem& default_item,
      const BookmarkNode* parent_node);
  void UpdatePartnerNode(const DefaultBookmarkItem& item,
                         const BookmarkNode* node);
  const BookmarkNode* AddPartnerNode(const DefaultBookmarkItem& item,
                                     const BookmarkNode* parent_node);

  const DefaultBookmarkTree* default_bookmark_tree_;
  BookmarkModel* model_;
  base::flat_set<std::string> deleted_partner_guids_;

  std::map<std::string, const BookmarkNode*> guid_node_map_;
  std::map<std::string, const BookmarkNode*> existing_partner_bookmarks_;
  Stats stats_;
};

struct DefaultBookmarkParser {
  DefaultBookmarkParser(DefaultBookmarkTree& tree) : tree(tree) {}

  void ParseJson(base::Value default_bookmarks_value);
  void ParseBookmarkList(int level,
                         base::Value* value_list,
                         std::vector<DefaultBookmarkItem>* default_items,
                         bool inside_bookmarks_folder);

  DefaultBookmarkTree& tree;

  // The set of partner folders and bookmarks outside Bokmarks folder that are
  // used by a particular bookmark file.
  std::set<const PartnerDetails*> used_details;

  // The set of partner bookmarks inside Bokmarks folder that are used by a
  // particular bookmark file.
  std::set<const PartnerDetails*> used_details2;
};

void DefaultBookmarkParser::ParseBookmarkList(
    int level,
    base::Value* value_list,
    std::vector<DefaultBookmarkItem>* default_items,
    bool inside_bookmarks_folder) {
  if (!value_list->is_list()) {
    tree.valid = false;
    LOG(ERROR) << kChildrenKey << " is not an array.";
    return;
  }
  if (level > 5) {
    tree.valid = false;
    LOG(ERROR) << "too deaply nested default bookmarks";
    return;
  }

  for (base::Value& dict : value_list->GetList()) {
    if (!dict.is_dict()) {
      tree.valid = false;
      LOG(ERROR) << "a child of " << kChildrenKey << " is not a dictionary";
      continue;
    }

    DefaultBookmarkItem item;

    const PartnerDetails* details = nullptr;
    if (std::string* value = dict.FindStringKey(kNameKey)) {
      details = vivaldi_partners::FindDetailsByName(*value);
    }
    if (!details) {
      tree.valid = false;
      LOG(ERROR) << "bookmark node with missing or unknown name";
      continue;
    }
    if (base::Value* children = dict.FindKey(kChildrenKey)) {
      // Folder
      item.guid = details->guid;
      item.title = details->title;
      item.speeddial = details->speeddial;
      if (!used_details.insert(details).second) {
        tree.valid = false;
        LOG(ERROR) << "duplicated folder " << details->name;
      }
      bool bookmarks_folder =
          (level == 0 && details->name == kBookmarksFolderName);
      ParseBookmarkList(level + 1, children, &item.children,
                        inside_bookmarks_folder || bookmarks_folder);
    } else {
      // Bookmark URL
      if (level == 0) {
        tree.valid = false;
        LOG(ERROR) << "top-level bookmark " << item.title << "is not a folder";
      }
      if (std::string* value = dict.FindStringKey(kTitleKey)) {
        item.title = std::move(*value);
      }
      if (item.title.empty()) {
        tree.valid = false;
        LOG(ERROR) << "bookmark without title";
      }

      if (std::string* value = dict.FindStringKey(kNicknameKey)) {
        item.nickname = std::move(*value);
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
            tree.valid = false;
            LOG(ERROR) << kURLKey << " for bookmark " << item.title
                       << " is not a valid URL: " << *value;
          }
        }
      } else {
        tree.valid = false;
        LOG(ERROR) << "bookmark " << item.title << " without " << kURLKey;
      }

      if (inside_bookmarks_folder) {
        if (!used_details2.insert(details).second) {
          tree.valid = false;
          LOG(ERROR) << "bookmark is defined twice inside bookmarks folder - "
                     << item.title;
        }
        item.guid = details->guid2;
      } else {
        if (!used_details.insert(details).second) {
          tree.valid = false;
          LOG(ERROR) << "bookmark is defined twice - " << item.title;
        }
        item.guid = details->guid;
      }

      item.thumbnail = details->thumbnail;
    }

    default_items->push_back(std::move(item));
    tree.item_count++;
  }
}

void DefaultBookmarkParser::ParseJson(base::Value default_bookmarks_value) {
  if (!default_bookmarks_value.is_dict()) {
    LOG(ERROR) << "default bookmark json is not an object";
    return;
  }
  std::string* version = default_bookmarks_value.FindStringKey(kVersionKey);
  if (!version || version->empty()) {
    LOG(ERROR) << "no " << kVersionKey << " in json";
    return;
  }
  tree.version = std::move(*version);

  base::Value* bookmark_list = default_bookmarks_value.FindKey(kChildrenKey);
  if (!bookmark_list) {
    LOG(ERROR) << "no " << kChildrenKey << " array.";
    return;
  }

  tree.valid = true;
  ParseBookmarkList(0, bookmark_list, &tree.top_items, false);
}

base::Optional<base::Value> ReadDefaultBookmarks(std::string locale) {

  vivaldi_partners::AssetReader reader;
  base::Optional<base::Value> default_bookmarks_value =
      reader.ReadJson(locale + ".json");
  if (!default_bookmarks_value) {
    if (reader.is_not_found()) {
      LOG(WARNING) << "missing default bookmarks file " << reader.GetPath();
      default_bookmarks_value = reader.ReadJson(kFallbackLocaleFile);
      if (!default_bookmarks_value) {
        if (reader.is_not_found()) {
          LOG(ERROR) << "missing fallback bookmark file " << reader.GetPath();
        }
      }
    }
  }
  return default_bookmarks_value;
}

BookmarkUpdater::BookmarkUpdater(
    const DefaultBookmarkTree* default_bookmark_tree,
    BookmarkModel* model)
    : default_bookmark_tree_(default_bookmark_tree), model_(model) {
}

void BookmarkUpdater::SetDeletedPartners(PrefService* prefs) {
  const base::Value* deleted_partners =
      prefs->GetList(vivaldiprefs::kBookmarksDeletedPartners);
  if (!deleted_partners->is_list()) {
    LOG(ERROR) << vivaldiprefs::kBookmarksDeletedPartners
               << " is no a list value";
    return;
  }

  std::vector<std::string> ids;
  ids.reserve(deleted_partners->GetList().size());
  bool has_locale_based_ids = false;
  for (const base::Value& value : deleted_partners->GetList()) {
    if (!value.is_string()) {
      LOG(ERROR) << vivaldiprefs::kBookmarksDeletedPartners
                 << " contains non-string entry";
      continue;
    }
    std::string guid = value.GetString();
    if (vivaldi_partners::MapLocaleIdToGUID(guid)) {
      has_locale_based_ids = true;
    }
    ids.push_back(std::move(guid));
  }
  if (has_locale_based_ids && kUpdateLocaleBasedPartners) {
    base::Value new_list(base::Value::Type::LIST);
    for (const std::string& guid : ids) {
      new_list.Append(base::Value(guid));
    }
    prefs->Set(vivaldiprefs::kBookmarksDeletedPartners, new_list);
  }

  deleted_partner_guids_ = base::flat_set<std::string>(std::move(ids));
}

void AddBookmarkGuids(const std::vector<DefaultBookmarkItem>& default_items,std::vector<std::string>* guids) {
   for (const DefaultBookmarkItem& item : default_items) {
     guids->push_back(item.guid);
     AddBookmarkGuids(item.children, guids);
   }
}

void BookmarkUpdater::RunCleanUpdate() {
  DCHECK(!g_bookmark_update_actve);
  g_bookmark_update_actve = true;

  FindExistingPartners(model_->bookmark_bar_node());
  FindExistingPartners(model_->trash_node());

  UpdateRecursively(default_bookmark_tree_->top_items,
                    model_->bookmark_bar_node());

  if (kAllowPartnerRemoval) {
    std::vector<std::string> defined_guid_vector;
    AddBookmarkGuids(default_bookmark_tree_->top_items, &defined_guid_vector);
    base::flat_set<std::string> defined_guid_set(
        std::move(defined_guid_vector));
    // A range loop cannot be used as we remove elements from the map not to
    // keep pointers to deleted nodes.
    for (auto i = existing_partner_bookmarks_.begin();
         i != existing_partner_bookmarks_.end();) {
      const std::string& partner_id = i->first;
      if (!defined_guid_set.contains(partner_id)) {
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
    std::string guid = node->guid();

    // If the guid was for a former locale-specific partner id, adjust it to
    // locale-independent one as guid_node_map_ is used to check for presence of
    // nodes that lost its partner status due to changes by the user.
    vivaldi_partners::MapLocaleIdToGUID(guid);
    auto inserted_guid = guid_node_map_.emplace(std::move(guid), node);
    if (!inserted_guid.second) {
      // This happens after sync mixed older locale-based partners from several
      // locales.
      VLOG(1) << "Duplicated GUID node_guid=" << node->guid()
              << " adjusted_guid=" << guid;
    }

    std::string partner_id = vivaldi_bookmark_kit::GetPartner(node);
    if (partner_id.empty())
      continue;
    if (vivaldi_partners::MapLocaleIdToGUID(partner_id)) {
      VLOG(2) << "Old locale-based partner id "
              << vivaldi_bookmark_kit::GetPartner(node) << " "
              << node->GetTitle();
    }
    auto inserted =
        existing_partner_bookmarks_.emplace(std::move(partner_id), node);
    if (!inserted.second) {
      // As with guid, this is a normal situation after a sync accross profiles
      // with older locale-specific partner ids from different locales. Ignore
      // the second copy.
      //
      // TODO(igor@vivaldi.com): Consider merging all such duplicated partner
      // nodes into one.
      VLOG(1) << "Duplicated partner partner_id=" << partner_id;
    }
  }
}

void BookmarkUpdater::UpdateRecursively(
    const std::vector<DefaultBookmarkItem>& default_items,
    const BookmarkNode* parent_node) {
  for (const DefaultBookmarkItem& item : default_items) {
    const BookmarkNode* node = UpdateOrAdd(item, parent_node);
    if (node && !item.children.empty()) {
      UpdateRecursively(item.children, node);
    }
  }
}

const BookmarkNode* BookmarkUpdater::UpdateOrAdd(
    const DefaultBookmarkItem& item,
    const BookmarkNode* parent_node) {
  auto iter = existing_partner_bookmarks_.find(item.guid);
  if (iter != existing_partner_bookmarks_.end()) {
    const BookmarkNode* node = iter->second;
    // The partner still exists in bookmarks.
    UpdatePartnerNode(item, node);
    return node;
  }

  iter = guid_node_map_.find(item.guid);
  if (iter != guid_node_map_.end()) {
    // The node was a partner. Do not touch the node, but still return it. If
    // the item is a folder, we want to update its children in case the user
    // renamed the folder that lead to a loss of its partner status.
    //
    // Note that normally a former partner should be also on the deleted partner
    // list, but we cannot rely on that. A sync against an older version of
    // Vivaldi that does not sync the deleted bookmarks list can bring a former
    // partner that is not on the local delete list.
    const BookmarkNode* node = iter->second;
    VLOG_IF(1, !item.children.empty())
        << "Found former partner folder by GUID match, title="
        << node->GetTitle() << " guid=" << node->guid();
    VLOG_IF(2, item.children.empty())
        << "Skipping former partner bookmark, title=" << node->GetTitle()
        << " guid=" << node->guid();
    return node;
  }

  // The partner item did not match a corresponding node in the tree. We check
  // if the if the corresponding node was deleted. If so, we skip it. If not,
  // we create a node for the item.
  //
  // But first for folders we need to deal with a corner case of an older
  // locale-specific folder with a randomly generated GUID. We check if any of
  // folder's child items exists in the bookmark tree. If we find such item,
  // we guess that its parent was the original partner folder and update
  // children there.
  for (const DefaultBookmarkItem& child_item : item.children) {
    auto iter = existing_partner_bookmarks_.find(child_item.guid);
    if (iter != existing_partner_bookmarks_.end()) {
      const BookmarkNode* node = iter->second->parent();
      VLOG(1) << "Guessed a folder from a child, title=" << node->GetTitle()
              << " guid=" << node->guid();
      return node;
    }
  }

  if (deleted_partner_guids_.contains(item.guid)) {
    VLOG(2) << "Skipping deleted partner name=" << item.title
            << " guid=" << item.guid;
    return nullptr;
  }

  return AddPartnerNode(item, parent_node);
}

const BookmarkNode* BookmarkUpdater::AddPartnerNode(
    const DefaultBookmarkItem& item,
    const BookmarkNode* parent_node) {
  vivaldi_bookmark_kit::CustomMetaInfo custom_meta;
  custom_meta.SetNickname(item.nickname);
  custom_meta.SetPartner(item.guid);
  custom_meta.SetThumbnail(item.thumbnail);
  custom_meta.SetDescription(item.description);
  custom_meta.SetSpeeddial(item.speeddial);

  base::string16 title = base::UTF8ToUTF16(item.title);
  size_t index = parent_node->children().size();
  const BookmarkNode* node;
  if (item.url.is_empty()) {
    VLOG(2) << "Adding folder " << item.title << " guid=" << item.guid;
    node = model_->AddFolder(parent_node, index, title, custom_meta.map(),
                             item.guid);
    stats_.added_folders++;
  } else {
    VLOG(2) << "Adding url " << item.title << " guid=" << item.guid;
    node = model_->AddURL(parent_node, index, title, item.url,
                          custom_meta.map(), base::nullopt, item.guid);
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

  if (kUpdateLocaleBasedPartners) {
    // Make sure the node uses locale-independent partner guid, not one of older
    // locale-specific partner ids.
    custom_meta.SetPartner(item.guid);
  }

  // If nick is taken by other node do nothing. But ensure that it is cleared
  // if the nick in defaults is empty.
  if (item.nickname.empty() ||
      !::vivaldi_bookmark_kit::DoesNickExists(model_, item.nickname, node)) {
    custom_meta.SetNickname(item.nickname);
  }
  // We do not clear the partner status when the user selects a custom
  // thumbnail or uses a page snapshot as a thumbnail. So update the
  // thumbnail only if still points to the partner image.
  if (vivaldi_partners::HasPartnerThumbnailPrefix(
          ::vivaldi_bookmark_kit::GetThumbnail(node))) {
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
                           base::Optional<base::Value> default_bookmarks_value,
                           UpdateCallback callback,
                           BookmarkModel* model) {
  bool ok = false;
  bool no_version = false;
  do {
    if (!model || !default_bookmarks_value)
      break;

    if (!model->bookmark_bar_node() || !model->trash_node()) {
      LOG(ERROR)
          << "the top node for bookmarks or trash nodes are not available";
      break;
    }

    // We parse default_bookmarks_value here after the bookmark model and
    // vivaldi_partners database are loaded, not on a worker thread immediatelly
    // after we read JSON in ReadDefaultBookmarks. The latter may run before
    // vivaldi_partners are ready especially on Android.
    DefaultBookmarkTree default_tree;
    DefaultBookmarkParser bookmark_parser(default_tree);
    bookmark_parser.ParseJson(std::move(*default_bookmarks_value));
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
    upgrader.SetDeletedPartners(prefs);
    upgrader.RunCleanUpdate();

    const BookmarkUpdater::Stats& stats = upgrader.stats();
    int changed = stats.added_folders + stats.added_urls +
                  stats.updated_folders + stats.updated_urls + stats.removed;
    int skipped = static_cast<int>(default_tree.item_count) - changed -
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

void UpdatePartnersFromDefaults(
    Profile* profile,
    const std::string& locale,
    UpdateCallback callback,
    base::Optional<base::Value> default_bookmarks_value) {
  // Unretained is safe as recording profiles are never removed.
  vivaldi_bookmark_kit::RunAfterModelLoad(
      profile ? BookmarkModelFactory::GetForBrowserContext(profile) : nullptr,
      base::BindOnce(&UpdatePartnersInModel, base::Unretained(profile), locale,
                     std::move(default_bookmarks_value), std::move(callback)));
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
