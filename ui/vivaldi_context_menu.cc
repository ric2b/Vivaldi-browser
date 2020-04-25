#include "ui/vivaldi_context_menu.h"

namespace vivaldi {
BookmarkMenuContainer::BookmarkMenuContainer(Delegate* a_delegate)
  :delegate(a_delegate) {}
BookmarkMenuContainer::~BookmarkMenuContainer() {}
MenubarMenuParams::MenubarMenuParams(Delegate* a_delegate)
  :delegate(a_delegate) {}
MenubarMenuParams::~MenubarMenuParams() {}

bool MenubarMenuParams::Delegate::IsBookmarkMenu(int menu_id) {
  return false;
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
