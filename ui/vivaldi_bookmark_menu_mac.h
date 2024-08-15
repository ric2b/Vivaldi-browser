//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
//

#ifndef UI_VIVALDI_BOOKMARK_MENU_MAC_H_
#define UI_VIVALDI_BOOKMARK_MENU_MAC_H_

#import <Cocoa/Cocoa.h>
#include <vector>
#include "ui/base/window_open_disposition.h"

class PrefService;

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

namespace vivaldi {

// Returns the ids of items in the bookmark menu that are not bookmarks.
std::vector<int>& GetBookmarkMenuIds();
bool IsBookmarkMenuId(int candidate);

void SetBookmarkMenu(NSMenu* menu);
NSMenu* GetBookmarkMenu();

int GetMenuIndex();

void SetContainerState(const std::string& edge, int menu_index);

void ClearBookmarkMenu();

void GetBookmarkNodes(const bookmarks::BookmarkNode* node,
                      std::vector<bookmarks::BookmarkNode*>& nodes);
void AddExtraBookmarkMenuItems(NSMenu* menu,
                               unsigned int* menu_index,
                               const bookmarks::BookmarkNode* node,
                               bool on_top);
void OnClearBookmarkMenu(NSMenu* menu, NSMenuItem* item);

WindowOpenDisposition WindowOpenDispositionFromNSEvent(NSEvent* event,
                                                       PrefService* prefs);

}  // namespace vivaldi

#endif  // UI_VIVALDI_BOOKMARK_MENU_MAC_H_