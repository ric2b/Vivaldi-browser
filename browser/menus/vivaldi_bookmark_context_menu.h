// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#ifndef BROWSER_MENUS_VIVALDI_BOOKMARK_CONTEXT_MENUS_H_
#define BROWSER_MENUS_VIVALDI_BOOKMARK_CONTEXT_MENUS_H_

#include <vector>

#include "browser/menus/bookmark_sorter.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}

namespace ui {
class SimpleMenuModel;
}

namespace gfx {
class ImageSkia;
}

namespace views {
class MenuItemView;
}

namespace vivaldi {
struct BookmarkMenuContainer;
}

class Browser;
class Profile;

namespace vivaldi {
void SetBookmarkContainer(const BookmarkMenuContainer* container,
                          int current_index);
void BuildBookmarkContextMenu(ui::SimpleMenuModel* menu_model);
void ExecuteBookmarkContextMenuCommand(Browser* browser,
                                       bookmarks::BookmarkModel* model,
                                       int64_t id,
                                       int command);
void ExecuteBookmarkMenuCommand(Browser* browser, int menu_command,
                                int64_t bookmark_id, int mouse_event_flags);
void HandleHoverUrl(Browser* browser, const std::string& url);
void HandleOpenMenu(Browser* browser, int64_t id);
const bookmarks::BookmarkNode* GetNodeByPosition(
    bookmarks::BookmarkModel* model, const gfx::Point& screen_point,
    int* start_index, gfx::Rect* rect);
const bookmarks::BookmarkNode* GetNextNode(bookmarks::BookmarkModel* model,
                                           bool next,
                                           int* start_index,
                                           gfx::Rect* rect);
void SortBookmarkNodes(const bookmarks::BookmarkNode* parent,
                       std::vector<bookmarks::BookmarkNode*>& nodes);
void AddVivaldiBookmarkMenuItems(Profile* profile_, views::MenuItemView* menu,
                                 const bookmarks::BookmarkNode* parent);
void AddSeparator(views::MenuItemView* menu);
bool AddIfSeparator(const bookmarks::BookmarkNode* node,
                    views::MenuItemView* menu);
bool IsVivaldiMenuItem(int id);
const gfx::ImageSkia* GetBookmarkDefaultIcon();
gfx::ImageSkia GetBookmarkFolderIcon(SkColor text_color);
gfx::ImageSkia GetBookmarkSpeeddialIcon(SkColor text_color);
}  // vivaldi


#endif  // BROWSER_MENUS_VIVALDI_BOOKMARK_CONTEXT_MENUS_H_
