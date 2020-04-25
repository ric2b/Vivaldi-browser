//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
#ifndef UI_VIEWS_VIVALDI_BOOKMARK_MENU_VIEWS_H_
#define UI_VIEWS_VIVALDI_BOOKMARK_MENU_VIEWS_H_

// Used by all desktop platforms

#include "chrome/browser/ui/views/bookmarks/bookmark_menu_controller_observer.h"
#include "ui/vivaldi_context_menu.h"

class BookmarkMenuController;

namespace aura {
class Window;
}

namespace bookmarks {
class BookmarkNode;
}

namespace gfx {
class Point;
}

namespace views {
class MenuItemView;
class Widget;
}

class VivaldiBookmarkMenuViews : public vivaldi::VivaldiBookmarkMenu,
                                 public BookmarkMenuControllerObserver {
 public:
  ~VivaldiBookmarkMenuViews() override;
  VivaldiBookmarkMenuViews(content::WebContents* web_contents,
                           const vivaldi::BookmarkMenuParams& params,
                           const gfx::Rect& button_rect);
  bool CanShow() override;
  void Show() override;

  void set_observer(vivaldi::VivaldiBookmarkMenuObserver* observer) override;

  static gfx::NativeView GetActiveNativeViewFromWebContents(
      content::WebContents* web_contents);

  // BookmarkMenuControllerObserver
  void BookmarkMenuControllerDeleted(
      BookmarkMenuController* controller) override;

 private:
  views::Widget* GetTopLevelWidget();
  Browser* GetBrowser();
  content::WebContents* web_contents_;
  gfx::Rect button_rect_;
  BookmarkMenuController* controller_;  // Deletes iself
  vivaldi::VivaldiBookmarkMenuObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiBookmarkMenuViews);
};

#endif  // UI_VIEWS_VIVALDI_BOOKMARK_MENU_VIEWS_H_