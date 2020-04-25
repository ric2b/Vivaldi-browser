//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//

#include "ui/views/vivaldi_bookmark_menu_views.h"

#include "browser/menus/vivaldi_bookmark_context_menu.h"
#include "browser/vivaldi_browser_finder.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_menu_controller_views.h"
#include "components/renderer_context_menu/views/toolkit_delegate_views.h"
#include "content/public/browser/web_contents.h"
#include "extensions/tools/vivaldi_tools.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

VivaldiBookmarkMenuViews::VivaldiBookmarkMenuViews(
    content::WebContents* web_contents,
    const vivaldi::BookmarkMenuContainer* container,
    const bookmarks::BookmarkNode* node,
    int offset,
    const gfx::Rect& button_rect)
  : web_contents_(web_contents),
    button_rect_(button_rect),
    controller_(nullptr),
    observer_(nullptr) {
  Browser* browser = GetBrowser();
  if (browser) {
    int index = 0;
    for (vivaldi::BookmarkMenuContainerEntry e: container->siblings) {
      if (e.id == node->id()) {
        vivaldi::SetBookmarkContainer(container, index);
        controller_ = new BookmarkMenuController(browser, web_contents_,
            GetTopLevelWidget(), node, offset, false);
        controller_->set_observer(this);
        break;
      }
      index ++;
    }
  }
}

VivaldiBookmarkMenuViews::~VivaldiBookmarkMenuViews() {
  vivaldi::SetBookmarkContainer(nullptr, 0); // Cleanup. No deletion.
  if (controller_) {
    controller_->set_observer(nullptr);
  }
}

void VivaldiBookmarkMenuViews::set_observer(
    vivaldi::VivaldiBookmarkMenuObserver* observer) {
  observer_ = observer;
}

bool VivaldiBookmarkMenuViews::CanShow() {
  return controller_ != nullptr;
}

void VivaldiBookmarkMenuViews::Show() {
  controller_->RunMenuAt(GetTopLevelWidget()->GetContentsView(), button_rect_);
}

void VivaldiBookmarkMenuViews::BookmarkMenuControllerDeleted(
    BookmarkMenuController* controller) {
  if (observer_) {
    observer_->BookmarkMenuClosed(this);
  }
  controller_ = nullptr;
}

//static
gfx::NativeView VivaldiBookmarkMenuViews::GetActiveNativeViewFromWebContents(
    content::WebContents* web_contents) {
  return web_contents->GetFullscreenRenderWidgetHostView()
      ? web_contents->GetFullscreenRenderWidgetHostView()->GetNativeView()
      : web_contents->GetNativeView();
}

views::Widget* VivaldiBookmarkMenuViews::GetTopLevelWidget() {
  return views::Widget::GetTopLevelWidgetForNativeView(
      GetActiveNativeViewFromWebContents(web_contents_));
}

Browser* VivaldiBookmarkMenuViews::GetBrowser() {
  views::Widget* widget = GetTopLevelWidget();
  return widget ? chrome::FindBrowserWithWindow(widget->GetNativeWindow())
                : nullptr;
}