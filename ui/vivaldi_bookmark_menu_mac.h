//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
//

#ifndef UI_VIVALDI_BOOKMARK_MENU_MAC_H_
#define UI_VIVALDI_BOOKMARK_MENU_MAC_H_

#import <Cocoa/Cocoa.h>
#include <vector>

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

namespace vivaldi {

bool IsBookmarkSeparator(const bookmarks::BookmarkNode* node);

void ClearBookmarkMenu();

void GetBookmarkNodes(const bookmarks::BookmarkNode* node,
                      std::vector<bookmarks::BookmarkNode*>& nodes);
void AddAddTabToBookmarksMenuItem(const bookmarks::BookmarkNode* node,
								  NSMenu* menu);
void OnClearBookmarkMenu(NSMenu* menu, NSMenuItem* item);

}  // namespace vivaldi

#endif  // UI_VIVALDI_BOOKMARK_MENU_MAC_H_