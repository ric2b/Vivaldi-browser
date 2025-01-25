// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "components/sync_bookmarks/bookmark_model_view.h"

#include "components/bookmarks/browser/bookmark_model.h"

namespace sync_bookmarks {

const bookmarks::BookmarkNode*
BookmarkModelViewUsingLocalOrSyncableNodes::trash_node() const {
  return underlying_model()->trash_node();
}

const bookmarks::BookmarkNode*
BookmarkModelViewUsingAccountNodes::trash_node() const {
  return underlying_model()->account_trash_node();
}

}  // namespace bookmarks
