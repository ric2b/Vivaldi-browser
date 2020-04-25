#include "ui/vivaldi_context_menu.h"

#include "browser/vivaldi_browser_finder.h"

namespace vivaldi {

// static
gfx::NativeView VivaldiMenu::GetActiveNativeViewFromWebContents(
    content::WebContents* web_contents) {
  return web_contents->GetFullscreenRenderWidgetHostView()
      ? web_contents->GetFullscreenRenderWidgetHostView()->GetNativeView()
      : web_contents->GetNativeView();
}

// static
views::Widget* VivaldiMenu::GetTopLevelWidgetFromWebContents(
    content::WebContents* web_contents) {
  return views::Widget::GetTopLevelWidgetForNativeView(
      GetActiveNativeViewFromWebContents(web_contents));
}

// static
Browser* VivaldiMenu::GetBrowserFromWebContents(
    content::WebContents* web_contents) {
  views::Widget* widget = GetTopLevelWidgetFromWebContents(web_contents);
  return widget ? chrome::FindBrowserWithWindow(widget->GetNativeWindow())
                : nullptr;
}

BookmarkMenuContainer::BookmarkMenuContainer(Delegate* a_delegate)
  :delegate(a_delegate) {}
BookmarkMenuContainer::~BookmarkMenuContainer() {}
MenubarMenuParams::MenubarMenuParams(Delegate* a_delegate)
  :delegate(a_delegate) {}
MenubarMenuParams::~MenubarMenuParams() {}

bool MenubarMenuParams::Delegate::IsBookmarkMenu(int menu_id) {
  return false;
}

int MenubarMenuParams::Delegate::GetSelectedMenuId() {
  return -1;
}

bool MenubarMenuParams::Delegate::IsItemChecked(int id) {
  return false;
}

bool MenubarMenuParams::Delegate::IsItemPersistent(int id) {
  return false;
}

bool MenubarMenuParams::Delegate::GetAccelerator(int id,
    ui::Accelerator* accelerator) {
  return false;
}

bool MenubarMenuParams::Delegate::GetUrl(int id, std::string* url) {
  return false;
}

BookmarkMenuContainer* MenubarMenuParams::Delegate::GetBookmarkMenuContainer() {
  return nullptr;
}

MenubarMenuEntry* MenubarMenuParams::GetSibling(int id) {
  for (::vivaldi::MenubarMenuEntry& sibling: siblings) {
    if (sibling.id == id) {
      return &sibling;
    }
  }
  return nullptr;
}

} // vivaldi
