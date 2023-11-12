// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#ifndef BROWSER_MENUS_VIVALDI_BOOKMARK_CONTEXT_MENUS_H_
#define BROWSER_MENUS_VIVALDI_BOOKMARK_CONTEXT_MENUS_H_

#include <vector>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/controls/menu/menu_item_view.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace ui {
class SimpleMenuModel;
}

namespace gfx {
class ImageSkia;
class Point;
class Rect;
}  // namespace gfx

namespace vivaldi {
struct BookmarkMenuContainer;
}

class Browser;
class Profile;

namespace vivaldi {
void SetBookmarkContainer(const BookmarkMenuContainer* container,
                          int current_index);
void BuildBookmarkContextMenu(Profile* profile,
                              ui::SimpleMenuModel* menu_model);
void ExecuteBookmarkContextMenuCommand(Browser* browser,
                                       bookmarks::BookmarkModel* model,
                                       int64_t id,
                                       int command);
void ExecuteBookmarkMenuCommand(Browser* browser,
                                int menu_command,
                                int64_t bookmark_id,
                                int mouse_event_flags);
void HandleHoverUrl(Browser* browser, const std::string& url);
void HandleOpenMenu(Browser* browser, int64_t id);
const bookmarks::BookmarkNode* GetNodeByPosition(
    bookmarks::BookmarkModel* model,
    const gfx::Point& screen_point,
    int* start_index,
    gfx::Rect* rect);
const bookmarks::BookmarkNode* GetNextNode(bookmarks::BookmarkModel* model,
                                           bool next,
                                           int* start_index,
                                           gfx::Rect* rect);
void SortBookmarkNodes(const bookmarks::BookmarkNode* parent,
                       std::vector<bookmarks::BookmarkNode*>& nodes);
void AddExtraBookmarkMenuItems(Profile* profile,
                               views::MenuItemView* menu,
                               unsigned int* menu_index,
                               const bookmarks::BookmarkNode* parent,
                               bool on_top);
void AddSeparator(views::MenuItemView* menu, unsigned int* menu_index);
bool AddIfSeparator(const bookmarks::BookmarkNode* node,
                    views::MenuItemView* menu,
                    unsigned int* menu_index);
views::MenuItemView* AddMenuItem(views::MenuItemView* menu,
                                 unsigned int* menu_index,
                                 int id,
                                 const std::u16string& label,
                                 const ui::ImageModel& icon,
                                 views::MenuItemView::Type type);

unsigned int GetStartIndexForBookmarks(views::MenuItemView* menu, int64_t id);
bool IsVivaldiMenuItem(int id);
const gfx::Image GetBookmarkDefaultIcon();
const gfx::Image GetBookmarkletIcon(views::MenuItemView* menu,
                                         views::Widget* widget);
ui::ImageModel GetBookmarkFolderIcon(views::MenuItemView* menu,
                                     views::Widget* widget);
ui::ImageModel GetBookmarkSpeeddialIcon(views::MenuItemView* menu,
                                        views::Widget* widget);
}  // namespace vivaldi

#endif  // BROWSER_MENUS_VIVALDI_BOOKMARK_CONTEXT_MENUS_H_
