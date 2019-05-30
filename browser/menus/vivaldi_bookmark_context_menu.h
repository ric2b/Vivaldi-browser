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
struct BookmarkMenuParams;
}

class Browser;
class Profile;

namespace vivaldi {
void BuildBookmarkContextMenu(ui::SimpleMenuModel* menu_model);
void ExecuteBookmarkContextMenuCommand(Browser* browser,
                                       bookmarks::BookmarkModel* model,
                                       int64_t id,
                                       int command);
void ExecuteBookmarkMenuCommand(Browser* browser, int menu_command,
                                int64_t bookmark_id, int mouse_event_flags);
void HandleHoverUrl(Browser* browser, const std::string& url);
void SetBookmarkMenuProperties(const BookmarkMenuParams* params);
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
