//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//

#include "ui/views/vivaldi_bookmark_menu_views.h"

#include "browser/menus/vivaldi_bookmark_context_menu.h"
#include "browser/vivaldi_browser_finder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_menu_controller_views.h"
#include "components/prefs/pref_service.h"
#include "components/renderer_context_menu/views/toolkit_delegate_views.h"
#include "content/public/browser/web_contents.h"
#include "extensions/tools/vivaldi_tools.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/widget/widget.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

namespace vivaldi {

VivaldiBookmarkMenu* CreateVivaldiBookmarkMenu(
    content::WebContents* web_contents,
    const BookmarkMenuContainer* container,
    const bookmarks::BookmarkNode* node,
    int offset,
    const gfx::Rect& button_rect) {
  return new VivaldiBookmarkMenuViews(web_contents, container, node, offset,
                                      button_rect);
}

void ConvertContainerRectToScreen(content::WebContents* web_contents,
                                  BookmarkMenuContainer& container) {
  views::Widget* widget = views::Widget::GetTopLevelWidgetForNativeView(
      VivaldiMenu::GetActiveNativeViewFromWebContents(web_contents));
  gfx::Point screen_loc;
  views::View::ConvertPointToScreen(widget->GetContentsView(), &screen_loc);
  for (BookmarkMenuContainerEntry& e : container.siblings) {
    gfx::Point point(e.rect.origin());
    point.Offset(screen_loc.x(), screen_loc.y());
    e.rect.set_origin(point);
  }
}

VivaldiBookmarkMenuViews::VivaldiBookmarkMenuViews(
    content::WebContents* web_contents,
    const BookmarkMenuContainer* container,
    const bookmarks::BookmarkNode* node,
    int offset,
    const gfx::Rect& button_rect)
    : web_contents_(web_contents), button_rect_(button_rect) {
  Browser* browser = vivaldi::FindBrowserForEmbedderWebContents(web_contents_);
  if (browser) {
    int index = 0;
    for (BookmarkMenuContainerEntry e : container->siblings) {
      if (e.id == node->id()) {
        SetBookmarkContainer(container, index);
        controller_ = new BookmarkMenuController(
            browser, GetTopLevelWidgetFromWebContents(web_contents_), node,
            offset, false);
        controller_->set_observer(this);
        break;
      }
      index++;
    }
  }
  Profile* profile = Profile::FromBrowserContext(
      web_contents->GetBrowserContext());
  views::MenuController::VivaldiSetCompactLayout(
      profile->GetPrefs()->GetBoolean(vivaldiprefs::kMenuCompact));
  views::MenuController::VivaldiSetContextMenu(false);
}

VivaldiBookmarkMenuViews::~VivaldiBookmarkMenuViews() {
  SetBookmarkContainer(nullptr, 0);  // Cleanup. No deletion.
  if (controller_) {
    controller_->set_observer(nullptr);
  }
}

void VivaldiBookmarkMenuViews::set_observer(
    VivaldiBookmarkMenuObserver* observer) {
  observer_ = observer;
}

bool VivaldiBookmarkMenuViews::CanShow() {
  return controller_ != nullptr;
}

void VivaldiBookmarkMenuViews::Show() {
  controller_->RunMenuAt(
      GetTopLevelWidgetFromWebContents(web_contents_)->GetContentsView(),
      button_rect_);
}

void VivaldiBookmarkMenuViews::BookmarkMenuControllerDeleted(
    BookmarkMenuController* controller) {
  if (observer_) {
    observer_->BookmarkMenuClosed(this);
  }
  controller_ = nullptr;
}

base::RepeatingCallback<content::PageNavigator*()>
VivaldiBookmarkMenuViews::GetPageNavigatorGetter() {
  auto getter = [](base::WeakPtr<VivaldiBookmarkMenuViews> bookmark_bar_view)
      -> content::PageNavigator* {
    if (!bookmark_bar_view)
      return nullptr;
    return bookmark_bar_view->web_contents_;
  };
  return base::BindRepeating(getter, weak_ptr_factory_.GetWeakPtr());
}

}  // namespace vivaldi