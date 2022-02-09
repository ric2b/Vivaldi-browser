// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#ifndef BROWSER_MENUS_BOOKMARK_SORTER_H_
#define BROWSER_MENUS_BOOKMARK_SORTER_H_

#include <vector>

#include "third_party/icu/source/i18n/unicode/coll.h"

namespace bookmarks {
class BookmarkNode;
}

namespace vivaldi {

class BookmarkSorter {
 public:
  enum SortField {
    FIELD_NONE,
    FIELD_TITLE,
    FIELD_URL,
    FIELD_NICKNAME,
    FIELD_DESCRIPTION,
    FIELD_DATEADDED
  };
  enum SortOrder { ORDER_NONE, ORDER_ASCENDING, ORDER_DESCENDING };

  ~BookmarkSorter();

  BookmarkSorter(SortField sort_field,
                 SortOrder sort_order,
                 bool group_folders);
  void sort(std::vector<bookmarks::BookmarkNode*>& vector);
  void setGroupFolders(bool group_folders) { group_folders_ = group_folders; }
  bool isManualOrder() const { return sort_field_ == FIELD_NONE; }

 private:
  bool fallbackToTitleSort(const icu::Collator* collator,
                           bookmarks::BookmarkNode* b1,
                           bookmarks::BookmarkNode* b2,
                           size_t l1,
                           size_t l2);
  bool fallbackToDateSort(bookmarks::BookmarkNode* b1,
                          bookmarks::BookmarkNode* b2,
                          size_t l1,
                          size_t l2);

  SortField sort_field_;
  SortOrder sort_order_;
  bool group_folders_;
  std::unique_ptr<icu::Collator> collator_;
};

}  // namespace vivaldi

#endif  // BROWSER_MENUS_BOOKMARK_SORTER_H_
