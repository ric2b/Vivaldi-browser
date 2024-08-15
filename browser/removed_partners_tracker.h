// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_BOOKMARKS_REMOVED_PARTNERS_TRACKER_H_
#define COMPONENTS_BOOKMARKS_REMOVED_PARTNERS_TRACKER_H_

#include <set>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/uuid.h"
#include "base/values.h"
#if !BUILDFLAG(IS_IOS)
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#endif //!IS_IOS
#include "components/bookmarks/browser/bookmark_model_observer.h"

class PrefService;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

#if BUILDFLAG(IS_IOS)
class LegacyBookmarkModel;
#endif

namespace vivaldi_partners {
class RemovedPartnersTracker : public bookmarks::BookmarkModelObserver
#if !BUILDFLAG(IS_IOS)
, public ProfileManagerObserver
#endif //!BUILDFLAG(IS_IOS)
 {
 public:
#if !BUILDFLAG(IS_IOS)
  static void Create(Profile* profile, bookmarks::BookmarkModel* model);
#else
  static void Create(PrefService* prefs, bookmarks::BookmarkModel* model);
#endif
  static std::set<base::Uuid> ReadRemovedPartners(
      const base::Value::List& deleted_partners,
      bool& upgraded_old_id);

 private:
  class MetaInfoChangeFilter;

#if !BUILDFLAG(IS_IOS)
  RemovedPartnersTracker(Profile* profile, bookmarks::BookmarkModel* model);
#else
  RemovedPartnersTracker(PrefService* prefs, bookmarks::BookmarkModel* model);
#endif
  ~RemovedPartnersTracker() override;
  RemovedPartnersTracker(const RemovedPartnersTracker&) = delete;
  RemovedPartnersTracker& operator=(const RemovedPartnersTracker&) = delete;

#if !BUILDFLAG(IS_IOS)
  // ProfileManagerObserver implementation.
  void OnProfileMarkedForPermanentDeletion(Profile* profile) override;
#endif //!BUILDFLAG(IS_IOS)
  // Implementing BookmarkModelObserver
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeRemoved(
      const bookmarks::BookmarkNode* parent,
      size_t old_index,
      const bookmarks::BookmarkNode* node,
      const std::set<GURL>& no_longer_bookmarked,
      const base::Location& location) override {}
  void OnWillRemoveBookmarks(const bookmarks::BookmarkNode* parent,
                             size_t old_index,
                             const bookmarks::BookmarkNode* node,
                             const base::Location& location) override;
  void OnWillChangeBookmarkMetaInfo(
      const bookmarks::BookmarkNode* node) override;
  void BookmarkMetaInfoChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkModelLoaded(bool ids_reassigned) override;
  void BookmarkModelBeingDeleted() override;

  // Unused overrides from BookmarkModelObserver
  void BookmarkNodeMoved(const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override {}
  void BookmarkNodeAdded(const bookmarks::BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override {}
  void BookmarkNodeFaviconChanged(
      const bookmarks::BookmarkNode* node) override {}
  void BookmarkNodeChildrenReordered(
      const bookmarks::BookmarkNode* node) override {}
  void BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls,
                                   const base::Location& location) override {}

  void SaveRemovedPartners();
  void TrackRemovals(const bookmarks::BookmarkNode* node, bool recursive);
  void DoTrackRemovals(const bookmarks::BookmarkNode* node, bool recursive);

  const raw_ptr<bookmarks::BookmarkModel> model_;
  const raw_ptr<PrefService> prefs_;
  std::set<base::Uuid> removed_partners_;
#if !BUILDFLAG(IS_IOS)
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};
  const raw_ptr<Profile> profile_ = nullptr;
#endif //!IS_IOS
  std::unique_ptr<MetaInfoChangeFilter> change_filter_;

  base::WeakPtrFactory<RemovedPartnersTracker> weak_factory_{this};
};
}  // namespace vivaldi_partners

#endif  // COMPONENTS_BOOKMARKS_REMOVED_PARTNERS_TRACKER_H_
