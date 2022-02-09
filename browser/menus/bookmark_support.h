// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#ifndef BROWSER_MENUS_BOOKMARK_SUPPORT_H_
#define BROWSER_MENUS_BOOKMARK_SUPPORT_H_

#include <string>
#include <vector>

namespace bookmarks {
class BookmarkNode;
}

namespace gfx {
class Image;
}

namespace vivaldi {

struct BookmarkSupport {
  enum Icons {
    kUrl = 0,
    kFolder = 1,
    kFolderDark = 2,
    kSpeeddial = 3,
    kSpeeddialDark = 4,
    kBookmarklet = 5,
    kBookmarkletDark = 6,
    kMax = 7,
  };
  BookmarkSupport();
  ~BookmarkSupport();
  void initIcons(const std::vector<std::string>& icons);
  const gfx::Image& iconForNode(const bookmarks::BookmarkNode* node) const;
  std::u16string add_label;
  std::vector<gfx::Image> icons;
  bool observer_enabled;
};

}  // namespace vivaldi

#endif  // BROWSER_MENUS_BOOKMARK_SUPPORT_H_
