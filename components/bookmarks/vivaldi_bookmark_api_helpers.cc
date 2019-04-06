
#include "chrome/browser/extensions/api/bookmarks/bookmark_api_helpers.h"

#include "chrome/browser/extensions/api/bookmarks/bookmark_api_constants.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace extensions {
namespace keys = bookmark_api_constants;

namespace bookmark_api_helpers {

bool MoveNodeToTrash(BookmarkModel* model,
  bookmarks::ManagedBookmarkService* managed,
  int64_t id,
  int insert_pos,
  std::string* error) {
  const BookmarkNode* node = bookmarks::GetBookmarkNodeByID(model, id);
  if (!node) {
    *error = keys::kNoNodeError;
    return false;
  }
  if (model->is_permanent_node(node)) {
    *error = keys::kModifySpecialError;
    return false;
  }
  if (!model->trash_node() ||
    bookmarks::IsDescendantOf(node, model->trash_node())) {
    // If it's already in trash, just delete it.
    model->Remove(node);
  }
  else {
    // Place it at the given offset.
    model->Move(node, model->trash_node(), insert_pos);
  }
  return true;
}

}  // namespace bookmark_api_helpers
}  // namespace extensions
