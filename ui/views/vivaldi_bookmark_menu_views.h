//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
#ifndef UI_VIEWS_VIVALDI_BOOKMARK_MENU_VIEWS_H_
#define UI_VIEWS_VIVALDI_BOOKMARK_MENU_VIEWS_H_

// Used by all desktop platforms

#include "chrome/browser/ui/views/bookmarks/bookmark_menu_controller_observer.h"
#include "ui/vivaldi_context_menu.h"

class BookmarkMenuController;

namespace bookmarks {
class BookmarkNode;
}

namespace views {
class MenuItemView;
class Widget;
}

namespace vivaldi {

class VivaldiBookmarkMenuViews : public VivaldiBookmarkMenu,
                                 public BookmarkMenuControllerObserver {
 public:
  ~VivaldiBookmarkMenuViews() override;
  VivaldiBookmarkMenuViews(content::WebContents* web_contents,
                           const BookmarkMenuContainer* container,
                           const bookmarks::BookmarkNode* node,
                           int offset,
                           const gfx::Rect& button_rect);
  bool CanShow() override;
  void Show() override;

  void set_observer(VivaldiBookmarkMenuObserver* observer) override;

  // BookmarkMenuControllerObserver
  void BookmarkMenuControllerDeleted(
      BookmarkMenuController* controller) override;

 private:
  content::WebContents* web_contents_;
  gfx::Rect button_rect_;
  BookmarkMenuController* controller_;  // Deletes iself
  VivaldiBookmarkMenuObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiBookmarkMenuViews);
};

}  // namespace vivaldi

#endif  // UI_VIEWS_VIVALDI_BOOKMARK_MENU_VIEWS_H_