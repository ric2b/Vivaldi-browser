// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LEGACY_BOOKMARK_MODEL_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LEGACY_BOOKMARK_MODEL_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/keyed_service/core/keyed_service.h"

// Vivaldi
#include "components/bookmarks/browser/bookmark_node.h"
// End Vivaldi

class GURL;

namespace base {
class Uuid;
}  // namespace base

namespace bookmarks {
class BookmarkModel;
class BookmarkModelObserver;
class BookmarkNode;
struct QueryFields;
}  // namespace bookmarks

namespace ios {
class AccountBookmarkModelFactory;
class LocalOrSyncableBookmarkModelFactory;
}  // namespace ios

// Vivaldi
using bookmarks::BookmarkNode;
// End Vivaldi

// iOS-specific interface that mimcs bookmarks::BookmarkModel that allows
// a gradual migration of code under ios/ to use one BookmarkModel instance
// instead of two, by allowing subclasses to expose the legacy behavior (two
// instances) on top of one shared underlying BookmarkModel instance.
class LegacyBookmarkModel : public KeyedService {
 public:
  LegacyBookmarkModel();
  LegacyBookmarkModel(const LegacyBookmarkModel&) = delete;
  ~LegacyBookmarkModel() override;

  LegacyBookmarkModel& operator=(const LegacyBookmarkModel&) = delete;

  // KeyedService overrides.
  void Shutdown() override;

  // Returns the root node. The 'bookmark bar' node and 'other' node are
  // children of the root node.
  // WARNING: avoid exercising this API, in particular if the caller may use
  // the node to iterate children. This is because the behavior of this function
  // changes based on whether or not feature
  // `syncer::kEnableBookmarkFoldersForAccountStorage` is enabled.
  const bookmarks::BookmarkNode* subtle_root_node_with_unspecified_children()
      const;

  // All public functions below are identical to the functions with the same
  // name in bookmarks::BookmarkModel API or, in some cases, related utility
  // libraries.
  bool loaded() const;
  void Remove(const bookmarks::BookmarkNode* node,
              bookmarks::metrics::BookmarkEditSource source);
  void Move(const bookmarks::BookmarkNode* node,
            const bookmarks::BookmarkNode* new_parent,
            size_t index);
  void Copy(const bookmarks::BookmarkNode* node,
            const bookmarks::BookmarkNode* new_parent,
            size_t index);
  void SetTitle(const bookmarks::BookmarkNode* node,
                const std::u16string& title,
                bookmarks::metrics::BookmarkEditSource source);
  void SetURL(const bookmarks::BookmarkNode* node,
              const GURL& url,
              bookmarks::metrics::BookmarkEditSource source);
  void SetDateAdded(const bookmarks::BookmarkNode* node, base::Time date_added);
  bool HasNoUserCreatedBookmarksOrFolders() const;

#if defined(VIVALDI_BUILD)
  // Note (prio@vivaldi.com) - Forked from main BookmarkModel since
  // LegacyBookmarkModel do not offer the same method arguments we need.

  // Adds a new folder node at the specified position with the given
  // `creation_time`, `uuid` and `meta_info`. If no UUID is provided (i.e.
  // nullopt), then a random one will be generated. If a UUID is provided, it
  // must be valid.
  const BookmarkNode* AddFolder(
      const BookmarkNode* parent,
      size_t index,
      const std::u16string& title,
      const BookmarkNode::MetaInfoMap* meta_info = nullptr,
      std::optional<base::Time> creation_time = std::nullopt,
      std::optional<base::Uuid> uuid = std::nullopt);

  // Adds a new bookmark for the given `url` at the specified position with the
  // given `meta_info`. Used for bookmarks being added through some direct user
  // action (e.g. the bookmark star).
  const BookmarkNode* AddNewURL(
      const BookmarkNode* parent,
      size_t index,
      const std::u16string& title,
      const GURL& url,
      const BookmarkNode::MetaInfoMap* meta_info = nullptr);

  // Adds a url at the specified position with the given `creation_time`,
  // `meta_info`, `uuid`, and `last_used_time`. If no UUID is provided
  // (i.e. nullopt), then a random one will be generated. If a UUID is
  // provided, it must be valid. Used for bookmarks not being added from
  // direct user actions (e.g. created via sync, locally modified bookmark
  // or pre-existing bookmark). `added_by_user` is true when a new bookmark was
  // added by the user and false when a node is added by sync or duplicated.
  const BookmarkNode* AddURL(
      const BookmarkNode* parent,
      size_t index,
      const std::u16string& title,
      const GURL& url,
      const BookmarkNode::MetaInfoMap* meta_info = nullptr,
      std::optional<base::Time> creation_time = std::nullopt,
      std::optional<base::Uuid> uuid = std::nullopt,
      bool added_by_user = false);
#else
  const bookmarks::BookmarkNode* AddFolder(
      const bookmarks::BookmarkNode* parent,
      size_t index,
      const std::u16string& title);
  const bookmarks::BookmarkNode* AddNewURL(
      const bookmarks::BookmarkNode* parent,
      size_t index,
      const std::u16string& title,
      const GURL& url);
  const bookmarks::BookmarkNode* AddURL(const bookmarks::BookmarkNode* parent,
                                        size_t index,
                                        const std::u16string& title,
                                        const GURL& url);
#endif // End Vivaldi

  void RemoveMany(const std::set<const bookmarks::BookmarkNode*>& nodes,
                  bookmarks::metrics::BookmarkEditSource source);
  void CommitPendingWriteForTest();

  // Vivaldi
  bool DoesNickExists(const std::string& nickname,
                      const BookmarkNode* updated_node);
  void SetNodeNickname(const BookmarkNode* node,
                       const std::string& nickname);
  void SetNodeDescription(const BookmarkNode* node,
                          const std::string& description);
  void SetNodeSpeeddial(const BookmarkNode* node,
                        bool speeddial);
  void SetNodeThumbnail(const BookmarkNode* node,
                        const std::string& path);
  void SetNodeMetaInfoMap(const BookmarkNode* node,
                          const BookmarkNode::MetaInfoMap& meta_info_map);
  void RemovePartnerId(const BookmarkNode* node);
  // End Vivaldi

  // LegacyBookmarkModel has three top-level permanent nodes (as opposed to
  // bookmarks::BookmarkModel that can have up to six).
  virtual const bookmarks::BookmarkNode* bookmark_bar_node() const = 0;
  virtual const bookmarks::BookmarkNode* other_node() const = 0;
  virtual const bookmarks::BookmarkNode* mobile_node() const = 0;
  virtual const bookmarks::BookmarkNode* managed_node() const = 0;

  // Vivaldi
  virtual const bookmarks::BookmarkNode* trash_node() const = 0;
  // End Vivaldi

  virtual bool IsBookmarked(const GURL& url) const = 0;
  virtual bool is_permanent_node(const bookmarks::BookmarkNode* node) const = 0;
  virtual void AddObserver(bookmarks::BookmarkModelObserver* observer) = 0;
  virtual void RemoveObserver(bookmarks::BookmarkModelObserver* observer) = 0;
  [[nodiscard]] virtual std::vector<
      raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>
  GetNodesByURL(const GURL& url) const = 0;
  virtual const bookmarks::BookmarkNode* GetNodeByUuid(
      const base::Uuid& uuid) const = 0;
  virtual const bookmarks::BookmarkNode* GetMostRecentlyAddedUserNodeForURL(
      const GURL& url) const = 0;
  virtual bool HasBookmarks() const = 0;

  // Functions that aren't present in BookmarkModel but in utility libraries
  // that require a subclass-specific implementation.
  virtual std::vector<const bookmarks::BookmarkNode*>
  GetBookmarksMatchingProperties(const bookmarks::QueryFields& query,
                                 size_t max_count) = 0;
  virtual const bookmarks::BookmarkNode* GetNodeById(int64_t id) = 0;
  // Returns whether `node` is part of, or relevant, in the scope of `this`.
  virtual bool IsNodePartOfModel(const bookmarks::BookmarkNode* node) const = 0;
  virtual const bookmarks::BookmarkNode*
  MoveToOtherModelPossiblyWithNewNodeIdsAndUuids(
      const bookmarks::BookmarkNode* node,
      LegacyBookmarkModel* dest_model,
      const bookmarks::BookmarkNode* dest_parent) = 0;

  virtual base::WeakPtr<LegacyBookmarkModel> AsWeakPtr() = 0;

 protected:
  // Allow factories to access the underlying model.
  friend class ios::AccountBookmarkModelFactory;
  friend class ios::LocalOrSyncableBookmarkModelFactory;

  virtual const bookmarks::BookmarkModel* underlying_model() const = 0;
  virtual bookmarks::BookmarkModel* underlying_model() = 0;
};

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LEGACY_BOOKMARK_MODEL_H_
