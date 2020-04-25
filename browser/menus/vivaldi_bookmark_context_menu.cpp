// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#include "browser/menus/vivaldi_bookmark_context_menu.h"

#include "app/vivaldi_resources.h"
#include "browser/menus/vivaldi_menu_enums.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/app/chrome_command_ids.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "extensions/api/bookmark_context_menu/bookmark_context_menu_api.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/vivaldi_context_menu.h"
namespace vivaldi {

// Owned by the active menu api instance. Always present while a menu is open.
static const BookmarkMenuContainer* Container;
// Index of active menu bar element.
static unsigned int CurrentIndex = 0;

typedef std::map<int, const bookmarks::BookmarkNode*> MenuIdToNodeMap;
static int NextMenuId = 0;
static MenuIdToNodeMap MenuIdToBookmarkMap;

void BuildBookmarkContextMenu(ui::SimpleMenuModel* menu_model) {
  menu_model->AddItemWithStringId(IDC_VIV_BOOKMARK_BAR_OPEN_NEW_TAB,
      IDS_VIV_BOOKMARK_BAR_OPEN_NEW_TAB);
  menu_model->AddItemWithStringId(IDC_VIV_BOOKMARK_BAR_OPEN_BACKGROUND_TAB,
      IDS_VIV_BOOKMARK_BAR_OPEN_BACKGROUND_TAB);
  menu_model->AddItemWithStringId(IDC_VIV_BOOKMARK_BAR_OPEN_CURRENT_TAB,
      IDS_VIV_BOOKMARK_BAR_OPEN_CURRENT_TAB);
  menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model->AddItemWithStringId(IDC_VIV_BOOKMARK_BAR_OPEN_NEW_WINDOW,
      IDS_VIV_BOOKMARK_BAR_OPEN_NEW_WINDOW);
  menu_model->AddItemWithStringId(IDC_VIV_BOOKMARK_BAR_OPEN_NEW_PRIVATE_WINDOW,
      IDS_VIV_BOOKMARK_BAR_OPEN_NEW_PRIVATE_WINDOW);
  menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model->AddItemWithStringId(IDC_VIV_BOOKMARK_BAR_ADD_ACTIVE_TAB,
      IDS_VIV_BOOKMARK_ADD_ACTIVE_TAB);
  menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model->AddItemWithStringId(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK,
      IDS_VIV_BOOKMARK_BAR_NEW_BOOKMARK);
  menu_model->AddItemWithStringId(IDC_BOOKMARK_BAR_NEW_FOLDER,
      IDS_VIV_BOOKMARK_BAR_NEW_FOLDER);
  menu_model->AddItemWithStringId(IDC_BOOKMARK_BAR_EDIT,
      IDS_VIV_BOOKMARK_BAR_EDIT);
  menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model->AddItemWithStringId(IDC_CUT,
      IDS_VIV_BOOKMARK_BAR_CUT);
  menu_model->AddItemWithStringId(IDC_COPY,
      IDS_VIV_BOOKMARK_BAR_COPY);
  menu_model->AddItemWithStringId(IDC_PASTE,
      IDS_VIV_BOOKMARK_BAR_PASTE);
  menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model->AddItemWithStringId(IDC_BOOKMARK_BAR_REMOVE,
      IDS_VIV_BOOKMARK_BAR_REMOVE);
}

void MoveToTrash(bookmarks::BookmarkModel* model, int64_t id) {
  const bookmarks::BookmarkNode *node =
      bookmarks::GetBookmarkNodeByID(model, id);
  if (node && model->trash_node()) {
    model->Move(node, model->trash_node(), 0);
  }
}

void ExecuteBookmarkContextMenuCommand(Browser* browser,
                                       bookmarks::BookmarkModel* model,
                                       int64_t bookmark_id,
                                       int menu_id) {
  switch (menu_id) {
    case IDC_VIV_BOOKMARK_BAR_OPEN_CURRENT_TAB:
    case IDC_VIV_BOOKMARK_BAR_OPEN_NEW_TAB:
    case IDC_VIV_BOOKMARK_BAR_OPEN_BACKGROUND_TAB:
    case IDC_VIV_BOOKMARK_BAR_OPEN_NEW_WINDOW:
    case IDC_VIV_BOOKMARK_BAR_OPEN_NEW_PRIVATE_WINDOW:
    case IDC_VIV_BOOKMARK_BAR_ADD_ACTIVE_TAB:
    case IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK:
    case IDC_BOOKMARK_BAR_NEW_FOLDER:
    case IDC_BOOKMARK_BAR_EDIT:
    case IDC_CUT:
    case IDC_COPY:
    case IDC_PASTE:
      Container->delegate->OnBookmarkAction(bookmark_id, menu_id);
      break;
    case IDC_BOOKMARK_BAR_REMOVE:
      // Handle locally so we can use chrome's code to keep menu open.
      MoveToTrash(model, bookmark_id);
      break;
  }
}

void ExecuteBookmarkMenuCommand(Browser* browser, int menu_id,
                                int64_t bookmark_id, int mouse_event_flags) {
  if (IsVivaldiMenuItem(menu_id)) {
    // Currently, and probably forever, we only have one specific menu item so
    // no more tests.
    Container->delegate->OnBookmarkAction(MenuIdToBookmarkMap[menu_id]->id(),
                                          IDC_VIV_BOOKMARK_BAR_ADD_ACTIVE_TAB);
  } else if (bookmark_id != -1) {
    Container->delegate->OnOpenBookmark(bookmark_id, mouse_event_flags);
  }
}

void HandleHoverUrl(Browser* browser, const std::string& url) {
  Container->delegate->OnHover(url);
}

void HandleOpenMenu(Browser* browser, int64_t id) {
  for (const ::vivaldi::BookmarkMenuContainerEntry& e:  Container->siblings) {
    if (e.id == id) {
      Container->delegate->OnOpenMenu(id);
      break;
    }
  }
}

const bookmarks::BookmarkNode* GetNodeByPosition(
    bookmarks::BookmarkModel* model, const gfx::Point& screen_point,
    int* start_index, gfx::Rect* rect) {
  unsigned int i = 0;
  for (const ::vivaldi::BookmarkMenuContainerEntry& e: Container->siblings) {
    if (e.rect.Contains(screen_point)) {
      CurrentIndex = i;
      *rect = e.rect;
      *start_index = e.offset;
      return bookmarks::GetBookmarkNodeByID(model, e.id);
    }
    i ++;
  }
  return nullptr;
}


const bookmarks::BookmarkNode* GetNextNode(bookmarks::BookmarkModel* model,
                                           bool next,
                                           int* start_index,
                                           gfx::Rect* rect) {
  if (Container->siblings.size() <= 1) {
    return nullptr;
  }
  if (next) {
    CurrentIndex ++;
    if (CurrentIndex >= Container->siblings.size()) {
      CurrentIndex = 0;
    }
  } else {
    if (CurrentIndex == 0) {
      CurrentIndex = Container->siblings.size() - 1;
    } else {
      CurrentIndex --;
    }
  }
  const ::vivaldi::BookmarkMenuContainerEntry& e =
      Container->siblings.at(CurrentIndex);
  *rect = e.rect;
  *start_index = e.offset;
  return bookmarks::GetBookmarkNodeByID(model, e.id);
}

void SetBookmarkContainer(const BookmarkMenuContainer* container,
                          int current_index) {
  Container = container;
  CurrentIndex = current_index;
  NextMenuId = 0;
  MenuIdToBookmarkMap.clear();
}

void SortBookmarkNodes(const bookmarks::BookmarkNode* parent,
                       std::vector<bookmarks::BookmarkNode*>& nodes) {
  nodes.reserve(parent->children().size());
  for (auto& it: parent->children()) {
    nodes.push_back(const_cast<bookmarks::BookmarkNode*>(it.get()));
  }
  bool folder_group =
      (CurrentIndex >= 0 && CurrentIndex < Container->siblings.size()) ?
          Container->siblings[CurrentIndex].folder_group : false;
  BookmarkSorter sorter(Container->sort_field, Container->sort_order,
                        folder_group);
  sorter.sort(nodes);
}

void AddVivaldiBookmarkMenuItems(Profile* profile, views::MenuItemView* menu,
                                 const bookmarks::BookmarkNode* parent) {
  menu->AppendMenuItemWithLabel(NextMenuId,
      l10n_util::GetStringUTF16(IDS_VIV_BOOKMARK_ADD_ACTIVE_TAB));
  MenuIdToBookmarkMap[NextMenuId] = parent;
  NextMenuId ++;
}

bool IsVivaldiMenuItem(int id) {
  return MenuIdToBookmarkMap[id] != nullptr;
}

void AddSeparator(views::MenuItemView* menu) {
  if (menu->HasSubmenu() &&  menu->GetSubmenu()->HasVisibleChildren()) {
    menu->AppendSeparator();
  }
}

bool AddIfSeparator(const bookmarks::BookmarkNode* node,
                    views::MenuItemView* menu) {
  static base::string16 separator = base::UTF8ToUTF16("---");
  static base::string16 separator_description = base::UTF8ToUTF16("separator");
  if (node->GetTitle().compare(separator) == 0 &&
      node->GetDescription().compare(separator_description) == 0) {
    // Old add separators in unsorted mode
    if (Container->sort_field == BookmarkSorter::FIELD_NONE) {
      menu->AppendSeparator();
    }
    return true;
  }
  return false;
}

const gfx::ImageSkia* GetBookmarkDefaultIcon() {
  return Container->support.icons[BookmarkSupport::kUrl].ToImageSkia();
}

gfx::ImageSkia GetBookmarkFolderIcon(SkColor text_color) {
  return *Container->support.icons[color_utils::IsDark(text_color) ?
      BookmarkSupport::kFolder : BookmarkSupport::kFolderDark].ToImageSkia();
}

gfx::ImageSkia GetBookmarkSpeeddialIcon(SkColor text_color) {
  return *Container->support.icons[color_utils::IsDark(text_color) ?
      BookmarkSupport::kSpeeddial :
      BookmarkSupport::kSpeeddialDark].ToImageSkia();
}

}  // vivaldi