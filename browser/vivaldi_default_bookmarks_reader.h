// Copyright 2018 Vivaldi Technologies. All rights reserved.
//

#ifndef BROWSER_VIVALDI_DEFAULT_BOOKMARKS_READER_H_
#define BROWSER_VIVALDI_DEFAULT_BOOKMARKS_READER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/optional.h"
#include "base/values.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"

namespace bookmarks {
class BookmarkModel;
}

namespace base {
class Value;
}

namespace vivaldi {

namespace first_run {

class VivaldiDefaultBookmarksReader : public bookmarks::BookmarkModelObserver {

public:
 // Returns the VivaldiDefaultBookmarksReader instance (singleton).
 static VivaldiDefaultBookmarksReader* GetInstance();

 void ReadBookmarks();

private:
  // This helper class modifies the bookmark model in the UI thread.
  // All methods must run in the UI thread.
  class Task : public base::RefCountedThreadSafe<Task> {

  public:
   Task();

   void CreateBookmarks(base::Value* root,
                        bookmarks::BookmarkModel* model);

  private:
   ~Task();

   bool DecodeFolder(const base::Value& value,
                     bookmarks::BookmarkModel* model,
                     const int64_t parent_id);

   friend class base::RefCountedThreadSafe<Task>;

   DISALLOW_COPY_AND_ASSIGN(Task);
  };

  // bookmarks::BookmarkModelObserver
  void BookmarkModelLoaded(bookmarks::BookmarkModel* model,
      bool ids_reassigned) override;

  void OnWillChangeBookmarkMetaInfo(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* node) override {}

  void BookmarkMetaInfoChanged(bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* node) override {}

  void BookmarkNodeMoved(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override {}

  void BookmarkNodeAdded(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* parent,
                         size_t index) override {}

  void BookmarkNodeRemoved(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* parent,
      size_t old_index,
      const bookmarks::BookmarkNode* node,
      const std::set<GURL>& no_longer_bookmarked) override {}

  void BookmarkNodeChanged(bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* node) override {}

  void BookmarkNodeFaviconChanged(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* node) override {}

  void BookmarkNodeChildrenReordered(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* node) override {}

  void BookmarkAllUserNodesRemoved(
      bookmarks::BookmarkModel* model,
      const std::set<GURL>& removed_urls) override {}

  bool ReadJson();
  base::FilePath GetDefaultBookmarksFilePath();

  VivaldiDefaultBookmarksReader();
  ~VivaldiDefaultBookmarksReader() override;

private:
  bookmarks::BookmarkModel* model_ = nullptr;
  base::Optional<base::Value> root_;
  //  True if an observer on the bookmark model has been added.
  bool added_bookmark_observer_ = false;
  // True if waiting for the bookmark model to finish loading.
  bool waiting_for_bookmark_model_ = false;

  friend struct base::DefaultSingletonTraits<VivaldiDefaultBookmarksReader>;

  DISALLOW_COPY_AND_ASSIGN(VivaldiDefaultBookmarksReader);
};

}  // namespace first_run
}  // namespace vivaldi

#endif // BROWSER_VIVALDI_DEFAULT_BOOKMARKS_READER_H_
