//
// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
//
#ifndef UI_VIVALDI_CONTEXT_MENU_H_
#define UI_VIVALDI_CONTEXT_MENU_H_

#include <vector>

#include "browser/menus/bookmark_sorter.h"
#include "browser/menus/bookmark_support.h"
#include "extensions/schema/bookmark_context_menu.h"

class Browser;

namespace content {
class WebContents;
}

namespace gfx {
class Image;
class Rect;
}

namespace ui {
class Accelerator;
class SimpleMenuModel;
class MenuModel;
}

namespace views {
class Widget;
}

namespace vivaldi {
class ContextMenuPostitionDelegate;
class VivaldiBookmarkMenu;
class VivaldiContextMenu;
class VivaldiMenubarMenu;
struct BookmarkMenuContainer;
}

namespace vivaldi {

struct MenubarMenuEntry {
  // Menu id
  int id;
  // Size and position of main menu element that opens menu.
  gfx::Rect rect;
};

struct MenubarMenuParams {
  class Delegate {
   public:
    virtual void PopulateModel(int menu_id, bool dark_text_color,
                               ui::MenuModel** menu_model) {}
    virtual void PopulateSubmodel(int menu_id, bool dark_text_color,
                                  ui::MenuModel* menu_model) {}
    virtual void OnMenuOpened(int menu_id) {}
    virtual void OnMenuClosed() {}
    virtual void OnAction(int id, int state) {}
    virtual bool IsBookmarkMenu(int menu_id);
    virtual int GetSelectedMenuId();
    virtual bool IsItemChecked(int id);
    virtual bool IsItemPersistent(int id);
    virtual bool GetAccelerator(int id,
        ui::Accelerator* accelerator);
    virtual bool GetUrl(int id, std::string* url);
    virtual BookmarkMenuContainer* GetBookmarkMenuContainer();
  };

  MenubarMenuParams(Delegate* delegate);
  ~MenubarMenuParams();
  MenubarMenuEntry* GetSibling(int id);
  // All menus that can be opened.
  std::vector<MenubarMenuEntry> siblings;
  Delegate* delegate;
};

struct BookmarkMenuContainerEntry {
  // Bookmark folder id
  int64_t id;
  // Offset into folder
  int offset;
  // When true, sorted content will have folders first or last in list
  bool folder_group;
  // Size and position of main menu element that opens menu.
  gfx::Rect rect;
};

// Used by main menu bar and bookmark bar context menu
struct BookmarkMenuContainer {
  class Delegate {
   public:
    virtual void OnHover(const std::string& url) {}
    virtual void OnOpenBookmark(int64_t bookmark_id, int event_state) {}
    virtual void OnBookmarkAction(int64_t bookmark_id, int command) {}
    // To inform JS that a new menu has been made visible. For bookmark bar.
    virtual void OnOpenMenu(int64_t bookmark_id) {}
  };

  BookmarkMenuContainer(Delegate* delegate);
  ~BookmarkMenuContainer();

  // Icons to use for folders and bookmarks missing a fav icon
  BookmarkSupport support;
  // Sort options
  vivaldi::BookmarkSorter::SortField sort_field;
  vivaldi::BookmarkSorter::SortOrder sort_order;
  // All folders that can be opnend
  std::vector<BookmarkMenuContainerEntry> siblings;
  // Delegate that will execute commands
  Delegate* delegate;
};


VivaldiContextMenu* CreateVivaldiContextMenu(
    content::WebContents* web_contents,
    ui::SimpleMenuModel* menu_model,
    const gfx::Rect& rect,
    bool force_views,
    vivaldi::ContextMenuPostitionDelegate* delegate);

VivaldiBookmarkMenu* CreateVivaldiBookmarkMenu(
    content::WebContents* web_contents,
    const BookmarkMenuContainer* container,
    const bookmarks::BookmarkNode* node,
    int offset,
    const gfx::Rect& button_rect);

VivaldiMenubarMenu* CreateVivaldiMenubarMenu(
    content::WebContents* web_contents,
    MenubarMenuParams& params,
    int id);

void ConvertMenubarButtonRectToScreen(
    content::WebContents* web_contents,
    vivaldi::MenubarMenuParams& params);

void ConvertContainerRectToScreen(
    content::WebContents* web_contents,
    vivaldi::BookmarkMenuContainer& container);


class VivaldiBookmarkMenuObserver {
 public:
  virtual void BookmarkMenuClosed(VivaldiBookmarkMenu* menu) = 0;
 protected:
  virtual ~VivaldiBookmarkMenuObserver() {}
};

class VivaldiMenu {
  public:
    static gfx::NativeView GetActiveNativeViewFromWebContents(
      content::WebContents* web_contents);
    static views::Widget* GetTopLevelWidgetFromWebContents(
      content::WebContents* web_contents);
    static Browser* GetBrowserFromWebContents(
      content::WebContents* web_contents);
};

class VivaldiContextMenu : public VivaldiMenu {
 public:
  virtual ~VivaldiContextMenu() {}

  virtual void Show() = 0;
  virtual void SetIcon(const gfx::Image& icon, int id) {}
  virtual void UpdateMenu(ui::SimpleMenuModel* menu_model, int id) {}
};

class VivaldiBookmarkMenu : public VivaldiMenu {
 public:
  virtual ~VivaldiBookmarkMenu() {}
  virtual bool CanShow() = 0;
  virtual void Show() = 0;
  virtual void set_observer(VivaldiBookmarkMenuObserver* observer) {}
};

class VivaldiMenubarMenu : public VivaldiMenu {
 public:
  virtual ~VivaldiMenubarMenu() {}
  virtual bool CanShow() = 0;
  virtual void Show() = 0;
};

}  // namespace vivaldi

#endif  // UI_VIVALDI_CONTEXT_MENU_H_
