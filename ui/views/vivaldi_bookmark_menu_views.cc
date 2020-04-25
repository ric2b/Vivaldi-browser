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
#include "ui/aura/window.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/widget/widget.h"

VivaldiBookmarkMenuViews::VivaldiBookmarkMenuViews(
    content::WebContents* web_contents,
    const vivaldi::BookmarkMenuParams& params,
    const gfx::Rect& button_rect)
    : web_contents_(web_contents),
      button_rect_(button_rect),
      controller_(nullptr),
      observer_(nullptr) {
  Browser* browser = GetBrowser();
  if (browser) {
    vivaldi::SetBookmarkMenuProperties(&params);
    controller_ = new BookmarkMenuController(browser, web_contents_,
        GetTopLevelWidget(), params.node, params.offset, false);
    controller_->set_observer(this);
  }
}

VivaldiBookmarkMenuViews::~VivaldiBookmarkMenuViews() {
  vivaldi::SetBookmarkMenuProperties(nullptr);  // Cleanup. No deletion.
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