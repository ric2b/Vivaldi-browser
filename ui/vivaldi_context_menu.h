//
// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
//
#ifndef UI_VIVALDI_CONTEXT_MENU_H_
#define UI_VIVALDI_CONTEXT_MENU_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "browser/menus/bookmark_sorter.h"
#include "browser/menus/bookmark_support.h"
#include "components/renderer_context_menu/render_view_context_menu_base.h"
#include "extensions/schema/bookmark_context_menu.h"
#include "extensions/schema/menubar_menu.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

class Browser;

namespace content {
class WebContents;
}

namespace gfx {
class Image;
}

namespace ui {
class Accelerator;
class SimpleMenuModel;
class MenuModel;
}  // namespace ui

namespace views {
class Widget;
}

namespace vivaldi {
class ContextMenuPostitionDelegate;
class VivaldiBookmarkMenu;
class VivaldiContextMenu;
class VivaldiMenubarMenu;
struct BookmarkMenuContainer;
class VivaldiRenderViewContextMenu;
}  // namespace vivaldi

namespace vivaldi {

ui::ImageModel GetImageModel(const std::string icon);

struct MenubarMenuEntry {
  // Menu id
  int id;
  // Size and position of main menu element that opens menu.
  gfx::Rect rect;
};

struct MenubarMenuParams {
  class Delegate {
   public:
    virtual void OnMenuOpened(int menu_id) {}
    virtual void OnMenuClosed() {}
    virtual void OnAction(int id, int state) {}
    virtual void OnError(std::string message) {}
  };

  MenubarMenuParams();
  ~MenubarMenuParams();
  MenubarMenuEntry* GetSibling(int id);
  // All menus that can be opened.
  std::vector<MenubarMenuEntry> siblings;
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
  // Offset into menu for where to start adding bookmark elements.
  unsigned int menu_index;
  // Add a separator after bottom edge (if any).
  bool tweak_separator;
};

// Used by main menu bar and bookmark bar context menu
struct BookmarkMenuContainer {
  // Position of extra items wrt the bookmarks.
  enum Edge { Above = 0, Below, Off };

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
  // Where extra items (like 'Add Active Tab') be shown wrt the list.
  Edge edge;
  // Delegate that will execute commands
  const raw_ptr<Delegate> delegate;
};

VivaldiContextMenu* CreateVivaldiContextMenu(
    content::WebContents* web_contents,
    ui::SimpleMenuModel* menu_model,
    const gfx::Rect& rect,
    bool force_views,
    VivaldiRenderViewContextMenu* render_view_context_menu);

VivaldiBookmarkMenu* CreateVivaldiBookmarkMenu(
    content::WebContents* web_contents,
    const BookmarkMenuContainer* container,
    const bookmarks::BookmarkNode* node,
    int offset,
    const gfx::Rect& button_rect);

VivaldiMenubarMenu* CreateVivaldiMenubarMenu(content::WebContents* web_contents,
                                             MenubarMenuParams::Delegate* delegate,
                                             std::optional<extensions::vivaldi::menubar_menu::Show::Params> api_params,
                                             int id);

void ConvertMenubarButtonRectToScreen(content::WebContents* web_contents,
                                      vivaldi::MenubarMenuParams& bar_params);

void ConvertContainerRectToScreen(content::WebContents* web_contents,
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
};

class VivaldiContextMenu : public VivaldiMenu {
 public:
  virtual ~VivaldiContextMenu() {}

  virtual void Init(ui::SimpleMenuModel* menu_model,
                    base::WeakPtr<ContextMenuPostitionDelegate> delegate) = 0;
  virtual bool Show() = 0;
  virtual void SetIcon(const gfx::Image& icon, int id) {}
  virtual void SetTitle(const std::u16string& title, int id) {}
  virtual void Refresh() {}
  virtual void UpdateMenu(ui::SimpleMenuModel* menu_model, int id) {}
  virtual bool HasDarkTextColor();
  virtual bool IsViews() = 0;
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
