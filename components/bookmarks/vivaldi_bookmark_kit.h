// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_BOOKMARKS_VIVALDI_BOOKMARK_KIT_H_
#define COMPONENTS_BOOKMARKS_VIVALDI_BOOKMARK_KIT_H_

#include <string>

#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_node.h"

namespace bookmarks {
class BookmarkModel;
}

// A collection of helper classes and utilities to extend Chromium BookmarkModel
// and BookmarkNode functionality.

namespace vivaldi_bookmark_kit {

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

// Wrapper around BookmarkNode::MetaInfoMap to set Vivaldi-specific properties.
class CustomMetaInfo {
 public:
  CustomMetaInfo();
  ~CustomMetaInfo();

  const BookmarkNode::MetaInfoMap* map() const { return &map_; }
  void SetMap(const BookmarkNode::MetaInfoMap& map);

  void Clear();

  void SetSpeeddial(bool speeddial);
  void SetBookmarkbar(bool bookmarkbar);
  void SetNickname(const std::string& nickname);
  void SetDescription(const std::string& description);
  void SetPartner(const std::string& partner);
  void SetDefaultFaviconUri(const std::string& default_favicon_uri);
  void SetVisitedDate(base::Time visited);
  void SetThumbnail(const std::string& thumbnail);

 private:
  BookmarkNode::MetaInfoMap map_;
  DISALLOW_COPY_AND_ASSIGN(CustomMetaInfo);
};

void InitModelNonClonedKeys(BookmarkModel* bookmark_model);

}  // namespace vivaldi_bookmark_kit

#endif  // COMPONENTS_BOOKMARKS_VIVALDI_BOOKMARK_KIT_H_