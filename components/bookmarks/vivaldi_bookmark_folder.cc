// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"

// static
BookmarkParentFolder BookmarkParentFolder::TrashFolder() {
  return BookmarkParentFolder(PermanentFolderType::kTrash);
}
