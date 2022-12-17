#include "ui/vivaldi_context_menu.h"

#include "browser/vivaldi_browser_finder.h"
#include "chrome/browser/ui/browser_finder.h"
#include "content/public/browser/render_widget_host_view.h"

namespace vivaldi {

// static
gfx::NativeView VivaldiMenu::GetActiveNativeViewFromWebContents(
    content::WebContents* web_contents) {
  // We used to test for a fullscreen view pre ch88, but that function got
  // removed (WebContents::GetFullscreenRenderWidgetHostView()) with 88. Seems
  // it is no longer required but keeping this wrapper function for a while in
  // case that turns out to be wrong.
  return web_contents->GetNativeView();
}

// static
views::Widget* VivaldiMenu::GetTopLevelWidgetFromWebContents(
    content::WebContents* web_contents) {
  return views::Widget::GetTopLevelWidgetForNativeView(
      GetActiveNativeViewFromWebContents(web_contents));
}

bool VivaldiContextMenu::HasDarkTextColor() {
  return true;
}

BookmarkMenuContainer::BookmarkMenuContainer(Delegate* a_delegate)
    : edge(Below), delegate(a_delegate) {}
BookmarkMenuContainer::~BookmarkMenuContainer() {}
MenubarMenuParams::MenubarMenuParams(Delegate* a_delegate)
    : delegate(a_delegate) {}
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

bool MenubarMenuParams::Delegate::IsItemEnabled(int id) {
  return true;
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
  for (::vivaldi::MenubarMenuEntry& sibling : siblings) {
    if (sibling.id == id) {
      return &sibling;
    }
  }
  return nullptr;
}

}  // namespace vivaldi
