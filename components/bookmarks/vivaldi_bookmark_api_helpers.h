// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#ifndef VIVALDI_COMPONENTS_BOOKMARKS_VIVALDI_BOOKMARK_API_HELPERS_H_
#define VIVALDI_COMPONENTS_BOOKMARKS_VIVALDI_BOOKMARK_API_HELPERS_H_

namespace extensions {
namespace bookmark_api_helpers {

bool MoveNodeToTrash(bookmarks::BookmarkModel* model,
                     bookmarks::ManagedBookmarkService* managed,
                     int64_t id,
                     int insert_pos,
                     std::string* error);

// Returns true if the nickname exists in bookmark model, false otherwise
// If |id| is -1 then the bookmark is being created, otherwise the nickname
// is being updated
bool DoesNickExists(BookmarkModel* model, base::string16 nickname, int64_t id);

}  // bookmark_api_helpers
}  // namespace extensions

#endif // VIVALDI_COMPONENTS_BOOKMARKS_VIVALDI_BOOKMARK_API_HELPERS_H_
