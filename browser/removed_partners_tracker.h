// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_BOOKMARKS_REMOVED_PARTNERS_TRACKER_H_
#define COMPONENTS_BOOKMARKS_REMOVED_PARTNERS_TRACKER_H_

#include <set>

#include "base/guid.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"

class PrefService;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace vivaldi_partners {
class RemovedPartnersTracker : public bookmarks::BookmarkModelObserver {
 public:
  static void Create(PrefService* prefs, bookmarks::BookmarkModel* model);
  static std::set<base::GUID> ReadRemovedPartners(
      const base::Value::List& deleted_partners,
      bool& upgraded_old_id);

 private:
  class MetaInfoChangeFilter;

  RemovedPartnersTracker(PrefService* prefs, bookmarks::BookmarkModel* model);
  ~RemovedPartnersTracker() override;
  RemovedPartnersTracker(const RemovedPartnersTracker&) = delete;
  RemovedPartnersTracker& operator=(const RemovedPartnersTracker&) = delete;

  // Implementing BookmarkModelObserver
  void BookmarkNodeChanged(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeRemoved(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls) override;
  void OnWillChangeBookmarkMetaInfo(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* node) override;
  void BookmarkMetaInfoChanged(bookmarks::BookmarkModel* model,
                               const bookmarks::BookmarkNode* node) override;
  void BookmarkModelLoaded(bookmarks::BookmarkModel* model,
                           bool ids_reassigned) override;
  void BookmarkModelBeingDeleted(bookmarks::BookmarkModel* model) override;

  // Unused overrides from BookmarkModelObserver
  void BookmarkNodeMoved(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override {}
  void BookmarkNodeAdded(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override {}
  void BookmarkNodeFaviconChanged(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* node) override {}
  void BookmarkNodeChildrenReordered(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* node) override {}
  void BookmarkAllUserNodesRemoved(
      bookmarks::BookmarkModel* model,
      const std::set<GURL>& removed_urls) override {}

  void SaveRemovedPartners();
  void TrackRemovals(const bookmarks::BookmarkNode* node, bool recursive);
  void DoTrackRemovals(const bookmarks::BookmarkNode* node, bool recursive);

  bookmarks::BookmarkModel* model_;
  PrefService* prefs_;
  std::set<base::GUID> removed_partners_;

  std::unique_ptr<MetaInfoChangeFilter> change_filter_;

  base::WeakPtrFactory<RemovedPartnersTracker> weak_factory_{this};
};
}  // namespace vivaldi_partners

#endif  // COMPONENTS_BOOKMARKS_REMOVED_PARTNERS_TRACKER_H_
