// Copyright 2019 Vivaldi Technologies. All rights reserved.

#include "browser/vivaldi_default_bookmarks.h"

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ref.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "browser/removed_partners_tracker.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/vivaldi_bookmark_kit.h"
#include "components/bookmarks/vivaldi_partners.h"
#include "components/datasource/resource_reader.h"
#include "components/datasource/vivaldi_data_url_utils.h"
#include "components/favicon/core/favicon_service.h"
#include "components/locale/locale_kit.h"
#include "components/prefs/pref_service.h"
#include "ui/base/models/tree_node_iterator.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

namespace vivaldi_default_bookmarks {

bool g_bookmark_update_active = false;

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

using favicon::FaviconService;

using vivaldi_partners::PartnerDetails;

namespace {

// This will enable code that, on an update, will remove any installed partner
// bookmarks that are no longer specified in the default collection. Currently
// we do not want this functionality.
constexpr bool kAllowPartnerRemoval = false;

const char kChildrenKey[] = "children";
const char kNameKey[] = "name";
const char kDescriptionKey[] = "description";
const char kNicknameKey[] = "nickname";
const char kTitleKey[] = "title";
const char kURLKey[] = "url";
const char kVersionKey[] = "version";

const char kBookmarksFolderName[] = "Bookmarks";

std::string_view bookmark_locales[] = {
#include "components/bookmarks/bookmark_locales.inc"
};

struct DefaultBookmarkItem {
  std::string title;
  std::string nickname;
  base::Uuid uuid;

  // UUID of the item in the alternative location, like if this is a speed dial,
  // then this is a UUID of the bookmark in the bookmarks location.
  base::Uuid alternative_uuid;
  std::string thumbnail;
  std::string description;
  GURL url;
  std::string favicon;
  GURL favicon_url;
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
    int moved = 0;
    int removed = 0;
    int failed_updates = 0;
  };

  BookmarkUpdater(FaviconServiceGetter favicons_getter,
                  const DefaultBookmarkTree* default_bookmark_tree,
                  bookmarks::BookmarkModel* model);

  void SetDeletedPartners(PrefService* prefs);

  void RunCleanUpdate();

  const Stats& stats() const { return stats_; }

 private:
  // Recursively update the existing partners including checking for moves but
  // without adding anything.
  void UpdateRecursively(const DefaultBookmarkItem* parent_item,
                         const std::vector<DefaultBookmarkItem>& default_items);

  // Recursively add missing items.
  void AddRecursively(const std::vector<DefaultBookmarkItem>& default_items,
                      const BookmarkNode* parent_node);

  void FindExistingPartners(const BookmarkNode* top_node);

  // Try to add the new partner at the given folder in the bookmark tree. Return
  // the added node or null if the item should be ignored because it was
  // deleted.
  const BookmarkNode* TryToAdd(const DefaultBookmarkItem& item,
                               const BookmarkNode* parent_node);

  // Update the node content like title or URL. It does not move the node in the
  // tree.
  void UpdatePartnerNode(const DefaultBookmarkItem& item,
                         const BookmarkNode* node);

  void MovePartnerIfRequired(const DefaultBookmarkItem* parent_item,
                             size_t item_index,
                             const DefaultBookmarkItem& item,
                             const BookmarkNode* node);

  // Add a new partner node to the given bookmark folder.
  const BookmarkNode* AddPartnerNode(const DefaultBookmarkItem& item,
                                     const BookmarkNode* parent_node);

  void SetFavicon(const GURL& page_url,
                  const GURL& icon_url,
                  std::string icon_path);

  FaviconServiceGetter favicons_getter_;
  const raw_ptr<const DefaultBookmarkTree> default_bookmark_tree_;
  const raw_ptr<bookmarks::BookmarkModel> model_;
  std::set<base::Uuid> deleted_partner_uuids_;

  std::map<base::Uuid, const BookmarkNode*> uuid_node_map_;
  std::map<base::Uuid, const BookmarkNode*> existing_partner_bookmarks_;
  Stats stats_;
};

struct DefaultBookmarkParser {
  DefaultBookmarkParser(DefaultBookmarkTree& tree) : tree(tree) {}

  void ParseJson(base::Value default_bookmarks_value);
  void ParseBookmarkList(int level,
                         base::Value* value_list,
                         std::vector<DefaultBookmarkItem>* default_items,
                         bool inside_bookmarks_folder);

  const raw_ref<DefaultBookmarkTree> tree;

  // The set of partner folders and bookmarks outside Bookmarks folder that are
  // used by a particular bookmark file.
  std::set<const PartnerDetails*> used_details;

  // The set of partner bookmarks inside Bookmarks folder that are used by a
  // particular bookmark file.
  std::set<const PartnerDetails*> used_details2;
};

void DefaultBookmarkParser::ParseBookmarkList(
    int level,
    base::Value* value_list,
    std::vector<DefaultBookmarkItem>* default_items,
    bool inside_bookmarks_folder) {
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

    DefaultBookmarkItem item;

    const PartnerDetails* details = nullptr;
    if (std::string* value = dict.GetDict().FindString(kNameKey)) {
      details = vivaldi_partners::FindDetailsByName(*value);
#if BUILDFLAG(IS_IOS)
      if (*value == "vivaldigame")
        continue;
#endif
    }
    if (!details) {
      tree->valid = false;
      LOG(ERROR) << "bookmark node with missing or unknown name: "
                 << *dict.GetDict().FindString(kNameKey);
      continue;
    }
    if (base::Value* children = dict.GetDict().Find(kChildrenKey)) {
      // Folder
      item.uuid = details->uuid;
      // Support for a localized name from the bookmark file. If not set we
      // fall back to partner specified name (not localized).
      if (std::string* value = dict.GetDict().FindString(kTitleKey)) {
        item.title = std::move(*value);
      } else {
        item.title = details->title;
      }
      if (item.title.empty()) {
        tree->valid = false;
        LOG(ERROR) << "folder without title";
      }
      item.speeddial = details->speeddial;
      if (!used_details.insert(details).second) {
        tree->valid = false;
        LOG(ERROR) << "duplicated folder " << details->name;
      }
      bool bookmarks_folder =
          (level == 0 && details->name == kBookmarksFolderName);
      ParseBookmarkList(level + 1, children, &item.children,
                        inside_bookmarks_folder || bookmarks_folder);
    } else {
      // Bookmark URL
      if (level == 0) {
        tree->valid = false;
        LOG(ERROR) << "top-level bookmark " << item.title << "is not a folder";
      }
      if (std::string* value = dict.GetDict().FindString(kTitleKey)) {
        item.title = std::move(*value);
      }
      if (item.title.empty()) {
        tree->valid = false;
        LOG(ERROR) << "bookmark without title";
      }

      if (std::string* value = dict.GetDict().FindString(kNicknameKey)) {
        item.nickname = std::move(*value);
      }

      if (std::string* value = dict.GetDict().FindString(kDescriptionKey)) {
        item.description = std::move(*value);
      }

      if (std::string* value = dict.GetDict().FindString(kURLKey)) {
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
        tree->valid = false;
        LOG(ERROR) << "bookmark " << item.title << " without " << kURLKey;
      }

      if (inside_bookmarks_folder) {
        if (!used_details2.insert(details).second) {
          tree->valid = false;
          LOG(ERROR) << "bookmark is defined twice inside bookmarks folder - "
                     << item.title;
        }
        item.uuid = details->uuid2;
        item.alternative_uuid = details->uuid;
      } else {
        if (!used_details.insert(details).second) {
          tree->valid = false;
          LOG(ERROR) << "bookmark is defined twice - " << item.title;
        }
        item.uuid = details->uuid;
        item.alternative_uuid = details->uuid2;
      }

      item.thumbnail = details->thumbnail;

      item.favicon = details->favicon;

      GURL favicon_url(details->favicon_url);
      if (favicon_url.is_valid()) {
        item.favicon_url = std::move(favicon_url);
      }
    }

    default_items->push_back(std::move(item));
    tree->item_count++;
  }
}

void DefaultBookmarkParser::ParseJson(base::Value default_bookmarks_value) {
  if (!default_bookmarks_value.is_dict()) {
    LOG(ERROR) << "default bookmark json is not an object";
    return;
  }
  std::string* version =
      default_bookmarks_value.GetDict().FindString(kVersionKey);
  if (!version || version->empty()) {
    LOG(ERROR) << "no " << kVersionKey << " in json";
    return;
  }
  tree->version = std::move(*version);

  base::Value* bookmark_list =
      default_bookmarks_value.GetDict().Find(kChildrenKey);
  if (!bookmark_list) {
    LOG(ERROR) << "no " << kChildrenKey << " array.";
    return;
  }

  tree->valid = true;
  ParseBookmarkList(0, bookmark_list, &tree->top_items, false);
}

std::optional<base::Value> ReadDefaultBookmarks(std::string locale) {
  return ResourceReader::ReadJSON(vivaldi_partners::GetBookmarkResourceDir(),
                                  locale + ".json");
}

BookmarkUpdater::BookmarkUpdater(
    FaviconServiceGetter favicons_getter,
    const DefaultBookmarkTree* default_bookmark_tree,
    bookmarks::BookmarkModel* model)
    : favicons_getter_(std::move(favicons_getter)),
      default_bookmark_tree_(default_bookmark_tree),
      model_(model) {}

void BookmarkUpdater::SetDeletedPartners(PrefService* prefs) {
  auto& deleted_partners =
      prefs->GetList(vivaldiprefs::kBookmarksDeletedPartners);
  bool upgraded_old_ids = false;
  deleted_partner_uuids_ =
      vivaldi_partners::RemovedPartnersTracker::ReadRemovedPartners(
          deleted_partners, upgraded_old_ids);
}

void AddBookmarkUuids(const std::vector<DefaultBookmarkItem>& default_items,
                      std::vector<base::Uuid>& uuids) {
  for (const DefaultBookmarkItem& item : default_items) {
    uuids.push_back(item.uuid);
    AddBookmarkUuids(item.children, uuids);
  }
}

void BookmarkUpdater::RunCleanUpdate() {
  DCHECK(!g_bookmark_update_active);
  g_bookmark_update_active = true;

  FindExistingPartners(model_->bookmark_bar_node());
  FindExistingPartners(model_->trash_node());

  UpdateRecursively(nullptr, default_bookmark_tree_->top_items);
  AddRecursively(default_bookmark_tree_->top_items,
                 model_->bookmark_bar_node());

  if (kAllowPartnerRemoval) {
    std::vector<base::Uuid> defined_uuid_vector;
    AddBookmarkUuids(default_bookmark_tree_->top_items, defined_uuid_vector);
    base::flat_set<base::Uuid> defined_uuid_set(std::move(defined_uuid_vector));
    // A range loop cannot be used as we remove elements from the map not to
    // keep pointers to deleted nodes.
    for (auto i = existing_partner_bookmarks_.begin();
         i != existing_partner_bookmarks_.end();) {
      const base::Uuid& partner_id = i->first;
      if (!defined_uuid_set.contains(partner_id)) {
        const BookmarkNode* node = i->second;
        if (!model_->is_permanent_node(node)) {
          VLOG(2) << "Removing non-existing partner title=" << node->GetTitle()
                  << " uuid=" << node->uuid();
          i = existing_partner_bookmarks_.erase(i);
          model_->Remove(node, {}, FROM_HERE);
          continue;
        }
      }
      i++;
    }
  }

  DCHECK(g_bookmark_update_active);
  g_bookmark_update_active = false;
}

void BookmarkUpdater::FindExistingPartners(const BookmarkNode* top_node) {
  ui::TreeNodeIterator<const BookmarkNode> iterator(top_node);
  while (iterator.has_next()) {
    const BookmarkNode* node = iterator.Next();
    base::Uuid uuid = node->uuid();

    // If the UUID was for a former locale-specific partner id, adjust it to
    // locale-independent one as uuid_node_map_ is used to check for presence of
    // nodes that lost its partner status due to changes by the user.
    vivaldi_partners::MapLocaleIdToUuid(uuid);
    auto inserted_uuid = uuid_node_map_.emplace(std::move(uuid), node);
    if (!inserted_uuid.second) {
      // This happens after sync mixed older locale-based partners from several
      // locales.
      VLOG(1) << "Duplicated Uuid node_uuid=" << node->uuid()
              << " adjusted_uuid=" << uuid;
    }

    base::Uuid partner_id = vivaldi_bookmark_kit::GetPartner(node);
    if (!partner_id.is_valid())
      continue;
    if (vivaldi_partners::MapLocaleIdToUuid(partner_id)) {
      VLOG(2) << "Old locale-based partner id "
              << vivaldi_bookmark_kit::GetPartner(node) << " "
              << node->GetTitle();
    }
    auto inserted =
        existing_partner_bookmarks_.emplace(std::move(partner_id), node);
    if (!inserted.second) {
      // As with uuid, this is a normal situation after a sync accross profiles
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
    const DefaultBookmarkItem* parent_item,
    const std::vector<DefaultBookmarkItem>& default_items) {
  for (size_t i = 0; i < default_items.size(); ++i) {
    const DefaultBookmarkItem& item = default_items[i];
    auto iter = existing_partner_bookmarks_.find(item.uuid);
    if (iter != existing_partner_bookmarks_.end()) {
      // The partner still exists in bookmarks.
      const BookmarkNode* node = iter->second;
      UpdatePartnerNode(item, node);
      MovePartnerIfRequired(parent_item, i, item, node);
    }
    if (!item.children.empty()) {
      UpdateRecursively(&item, item.children);
    }
  }
}

void BookmarkUpdater::AddRecursively(
    const std::vector<DefaultBookmarkItem>& default_items,
    const BookmarkNode* parent_node) {
  for (size_t i = 0; i < default_items.size(); ++i) {
    const DefaultBookmarkItem& item = default_items[i];
    auto iter = existing_partner_bookmarks_.find(item.uuid);
    const BookmarkNode* node;
    if (iter != existing_partner_bookmarks_.end()) {
      // We need the node in case this is a folder when we need check
      // its children recursively.
      node = iter->second;
    } else {
      node = TryToAdd(item, parent_node);
    }
    if (node && !item.children.empty()) {
      AddRecursively(item.children, node);
    }
  }
}

void BookmarkUpdater::UpdatePartnerNode(const DefaultBookmarkItem& item,
                                        const BookmarkNode* node) {
  auto error = [&](std::string_view message) -> void {
    LOG(ERROR) << message << " - " << item.title;
    stats_.failed_updates++;
  };

  SetFavicon(node->url(), item.favicon_url, item.favicon);

  if (model_->is_permanent_node(node))
    return error("Partner became a permamnent node");

  if (item.url.is_empty()) {
    if (!node->is_folder())
      return error("Partner folder became a bookmark url");
  } else {
    if (node->is_folder())
      return error("Partner url became a bookmark folder");
  }

  VLOG(2) << "Updatning " << item.title << " uuid=" << node->uuid();
  model_->SetTitle(node, base::UTF8ToUTF16(item.title),
                   bookmarks::metrics::BookmarkEditSource::kUser);
  if (!item.url.is_empty()) {
    model_->SetURL(node, item.url,
                   bookmarks::metrics::BookmarkEditSource::kUser);
  }

  vivaldi_bookmark_kit::CustomMetaInfo custom_meta;
  const BookmarkNode::MetaInfoMap* old_meta_info = node->GetMetaInfoMap();
  if (old_meta_info) {
    custom_meta.SetMap(*old_meta_info);
  }

  custom_meta.SetPartner(item.uuid);

  // If nick is taken by other node do nothing. But ensure that it is cleared
  // if the nick in defaults is empty.
  if (item.nickname.empty() ||
      !::vivaldi_bookmark_kit::DoesNickExists(model_, item.nickname, node)) {
    custom_meta.SetNickname(item.nickname);
  }
  // We do not clear the partner status when the user selects a custom
  // thumbnail or uses a page snapshot as a thumbnail. So update the
  // thumbnail only if still points to the partner image.
  if (vivaldi_data_url_utils::IsResourceURL(
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

void BookmarkUpdater::MovePartnerIfRequired(
    const DefaultBookmarkItem* parent_item,
    size_t item_index,
    const DefaultBookmarkItem& item,
    const BookmarkNode* node) {
  if (!parent_item)
    return;
  auto iter = uuid_node_map_.find(parent_item->uuid);
  if (iter == uuid_node_map_.end())
    return;
  const BookmarkNode* expected_parent = iter->second;
  if (node->parent() == model_->trash_node()) {
    // Normal case - no move out of trash
    return;
  }
  if (expected_parent == node->parent()) {
    // Normal case - no move between folders.
    return;
  }

  // TODO(igor@vivaldi.com): Better deal with cases like user rearranging items
  // in expected_parent to track the existing partners and insert at more
  // sensible position. For now always insert at the end and ignore
  // `item_index`.
  //
  // Another problem is that this does not respect user actions like moving the
  // partners to another location. Ideally if the user moves a partner or a
  // folder, we should mark it as user-moved and stop any movement here.
  size_t new_index = expected_parent->children().size();
  VLOG(2) << "Moving " << item.title << " uuid=" << node->uuid() << " to "
          << parent_item->title << " folder, index=" << new_index;
  model_->Move(node, expected_parent, new_index);
  stats_.moved++;
}

const BookmarkNode* BookmarkUpdater::TryToAdd(const DefaultBookmarkItem& item,
                                              const BookmarkNode* parent_node) {
  auto iter = uuid_node_map_.find(item.uuid);
  if (iter != uuid_node_map_.end()) {
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
        << "Found former partner folder by Uuid match, title="
        << node->GetTitle() << " uuid=" << node->uuid();
    VLOG_IF(2, item.children.empty())
        << "Skipping former partner bookmark, title=" << node->GetTitle()
        << " uuid=" << node->uuid();
    return node;
  }

  // The partner item has not not matched a corresponding node in the tree. We
  // check if the the corresponding node was deleted. If so, we skip it. If not,
  // we create a node for the item.
  //
  // But first for folders we need to deal with a corner case of an older
  // locale-specific folder with a randomly generated UUID. We check if any of
  // folder's child items exists in the bookmark tree. If we find such item,
  // we guess that its parent was the original partner folder and update
  // children there.
  for (const DefaultBookmarkItem& child_item : item.children) {
    iter = existing_partner_bookmarks_.find(child_item.uuid);
    if (iter != existing_partner_bookmarks_.end()) {
      const BookmarkNode* node = iter->second->parent();
      VLOG(1) << "Guessed a folder from a child, title=" << node->GetTitle()
              << " uuid=" << node->uuid();
      return node;
    }
  }

  if (base::Contains(deleted_partner_uuids_, item.uuid)) {
    VLOG(2) << "Skipping deleted partner name=" << item.title
            << " uuid=" << item.uuid;
    return nullptr;
  }

  // Yet another corner case. We may have moved a partner between Speed Dial and
  // Bookmarks folders yet we also support a copy of the partner both in Speed
  // Dial and Bookmarks. The copy if any will use the alternative GUID. If we
  // moved the item during the update, we do not want add a copy to the same
  // folder.
  if (item.alternative_uuid.is_valid()) {
    auto iter_of_copy = existing_partner_bookmarks_.find(item.alternative_uuid);
    if (iter_of_copy != existing_partner_bookmarks_.end()) {
      const BookmarkNode* node_of_copy = iter_of_copy->second;
      if (node_of_copy->parent() == parent_node) {
        VLOG(2) << "Skipping adding a partner as a copy is already in the "
                   "folder, title="
                << item.title << " uuid=" << item.uuid
                << " uuid2=" << item.alternative_uuid;
        return nullptr;
      }
    }
  }

  return AddPartnerNode(item, parent_node);
}

const BookmarkNode* BookmarkUpdater::AddPartnerNode(
    const DefaultBookmarkItem& item,
    const BookmarkNode* parent_node) {
  vivaldi_bookmark_kit::CustomMetaInfo custom_meta;
  custom_meta.SetNickname(item.nickname);
  custom_meta.SetPartner(item.uuid);
  custom_meta.SetThumbnail(item.thumbnail);
  custom_meta.SetDescription(item.description);
  custom_meta.SetSpeeddial(item.speeddial);

  std::u16string title = base::UTF8ToUTF16(item.title);
  size_t index = parent_node->children().size();
  const BookmarkNode* node;
  if (item.url.is_empty()) {
    VLOG(2) << "Adding folder " << item.title << " uuid=" << item.uuid;
    node = model_->AddFolder(parent_node, index, title, custom_meta.map(),
                             std::nullopt, item.uuid);
    stats_.added_folders++;
  } else {
    VLOG(2) << "Adding url " << item.title << " uuid=" << item.uuid;
    node = model_->AddURL(parent_node, index, title, item.url,
                          custom_meta.map(), std::nullopt, item.uuid);
    stats_.added_urls++;

    SetFavicon(item.url, item.favicon_url, item.favicon);
  }
  return node;
}

void BookmarkUpdater::SetFavicon(const GURL& page_url,
                                 const GURL& icon_url,
                                 std::string icon_path) {
  if (page_url.is_empty() || icon_url.is_empty() || icon_path.empty())
    return;

  auto set_favicon_image = [](FaviconServiceGetter favicons_getter,
                              GURL page_url, GURL icon_url,
                              gfx::Image image) -> void {
    FaviconService* favicon_service = favicons_getter.Run();
    if (!favicon_service)
      return;
    favicon_service->SetFavicons({page_url}, icon_url,
                                 favicon_base::IconType::kFavicon, image);
  };
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ResourceReader::ReadPngImage, std::move(icon_path)),
      base::BindOnce(set_favicon_image, favicons_getter_, page_url, icon_url));
}

void UpdatePartnersInModel(std::unique_ptr<UpdaterClient> client,
                           const std::string& locale,
                           std::optional<base::Value> default_bookmarks_value,
                           UpdateCallback callback,
                           bookmarks::BookmarkModel* model) {
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

    PrefService* prefs = client->GetPrefService();
    DCHECK(prefs);

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

    BookmarkUpdater upgrader(client->GetFaviconServiceGetter(), &default_tree,
                             model);
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
              << " moved=" << stats.moved << " removed=" << stats.removed
              << " skipped=" << skipped << " locale=" << locale;

    if (stats.failed_updates) {
      LOG(ERROR) << " failed_updates=" << stats.failed_updates
                 << " locale=" << locale;
      break;
    }

    prefs->SetString(vivaldiprefs::kBookmarksVersion, default_tree.version);
    ok = true;

  } while (false);

  if (callback) {
    std::move(callback).Run(ok, no_version, locale);
  }
}

void UpdatePartnersFromDefaults(
    std::unique_ptr<UpdaterClient> client,
    const std::string& locale,
    UpdateCallback callback,
    std::optional<base::Value> default_bookmarks_value) {
  bookmarks::BookmarkModel* model = client->GetBookmarkModel();
  if (!model)
    return;
  vivaldi_bookmark_kit::RunAfterModelLoad(
      model,
      base::BindOnce(&UpdatePartnersInModel, std::move(client), locale,
                     std::move(default_bookmarks_value), std::move(callback)));
}

std::string GetBookmarkLocale(PrefService* prefs,
                              const std::string& application_locale) {
  std::string locale = prefs->GetString(vivaldiprefs::kBookmarksLanguage);
  if (!locale.empty()) {
    // Check if the locale is still a valid one.
    auto* i = std::find(std::begin(bookmark_locales),
                        std::end(bookmark_locales), locale);
    if (i != std::end(bookmark_locales))
      return locale;
  }
  locale =
      locale_kit::FindBestMatchingLocale(bookmark_locales, application_locale);
  DCHECK(!locale.empty());
  if (!locale.empty()) {
    prefs->SetString(vivaldiprefs::kBookmarksLanguage, locale);
  }
  return locale;
}

}  // namespace

UpdaterClient::~UpdaterClient() = default;

void UpdatePartners(std::unique_ptr<UpdaterClient> client,
                    UpdateCallback callback) {
  // A guest session cannot have persistent bookmarks and must not trigger
  // this call.
  if (!client) {
    std::move(callback).Run(false, false, std::string());
    return;
  }

  std::string locale = GetBookmarkLocale(client->GetPrefService(),
                                         client->GetApplicationLocale());
  LOG(INFO) << "Selected bookmark locale = " << locale;
  // Unretained() is safe as recording profiles are not deleted until shutdown.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ReadDefaultBookmarks, locale),
      base::BindOnce(&UpdatePartnersFromDefaults, std::move(client), locale,
                     std::move(callback)));
}

}  // namespace vivaldi_default_bookmarks
