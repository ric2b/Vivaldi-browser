//
// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
//
#ifndef UI_VIVALDI_CONTEXT_MENU_H_
#define UI_VIVALDI_CONTEXT_MENU_H_

#include "browser/menus/bookmark_sorter.h"
#include "browser/menus/bookmark_support.h"

namespace bookmarks {
class BookmarkNode;
}

namespace content {
class WebContents;
struct ContextMenuParams;
}

namespace gfx {
class Image;
class Rect;
}

namespace ui {
class SimpleMenuModel;
}

namespace vivaldi {
class VivaldiBookmarkMenu;
}

namespace vivaldi {
class VivaldiContextMenu;
}

namespace vivaldi {
struct BookmarkMenuParams {
  BookmarkMenuParams();
  ~BookmarkMenuParams();

  // The folder to display
  const bookmarks::BookmarkNode* node;
  // Offset into the folder
  int offset;
  // Place all subfolders together
  bool folder_group;
  // Icons to use for folders and bookmarks missing a fav icon
  BookmarkSupport support;
  // What to sort for and order
  vivaldi::BookmarkSorter::SortField sort_field;
  vivaldi::BookmarkSorter::SortOrder sort_order;
};

VivaldiContextMenu* CreateVivaldiContextMenu(
    content::WebContents* web_contents,
    ui::SimpleMenuModel* menu_model,
    const content::ContextMenuParams& params);

VivaldiBookmarkMenu* CreateVivaldiBookmarkMenu(
    content::WebContents* web_contents,
    const BookmarkMenuParams& params,
    const gfx::Rect& button_rect);

class VivaldiBookmarkMenuObserver {
 public:
  virtual void BookmarkMenuClosed(VivaldiBookmarkMenu* menu) = 0;
 protected:
  virtual ~VivaldiBookmarkMenuObserver() {}
};

class VivaldiContextMenu {
 public:
  virtual ~VivaldiContextMenu() {}

  virtual void Show() = 0;
  virtual void SetIcon(const gfx::Image& icon, int id) {}
  virtual void SetSelectedItem(int id) {}
  virtual void UpdateMenu(ui::SimpleMenuModel* menu_model, int id) {}
};

class VivaldiBookmarkMenu {
 public:
  virtual ~VivaldiBookmarkMenu() {}
  virtual bool CanShow() = 0;
  virtual void Show() = 0;
  virtual void set_observer(VivaldiBookmarkMenuObserver* observer) {}
};


}  // namespace vivaldi

#endif  // UI_VIVALDI_CONTEXT_MENU_H_
