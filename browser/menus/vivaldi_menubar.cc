//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
#include "browser/menus/vivaldi_menubar.h"

#include "base/functional/bind.h"
#include "browser/menus/vivaldi_bookmark_context_menu.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_menu_delegate.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/favicon/core/favicon_service.h"
#include "ui/base/theme_provider.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/widget/widget.h"
#include "ui/vivaldi_context_menu.h"

namespace vivaldi {

static bool IsBookmarkCommand(int command_id) {
  return command_id >= IDC_FIRST_UNBOUNDED_MENU;
}

SkColor TextColorForMenu(views::MenuItemView* menu, views::Widget* widget) {
  // Use the same code as in MenuItemView::GetTextColor() for best result.
  if (widget && widget->GetNativeTheme()) {
    return widget->GetColorProvider()->GetColor(ui::kColorMenuItemForeground);
  } else {
    return menu->GetColorProvider()->GetColor(ui::kColorMenuItemForeground);
  }
}

// The Menubar class is a support class for the MenubarMenu api. It is used to
// display single context menus as well. They are basically a menubar with only
// one top element. Menus are created on demand just before shown do deal with
// large menus slowing execution time down.

Menubar::Menubar(Browser* browser, MenubarMenuParams& params, int run_types)
    : browser_(browser), params_(&params), run_types_(run_types) {}

Menubar::~Menubar() {
  // Ensure that all top level items (this is only important for menu bar mode
  // with multiple top level items - vivaldi menu mode with one top level item
  // is already handled in chrome code) have no delegate anymore (the delegate
  // is the destroyed api instance).
  std::map<int, views::MenuItemView*>::iterator it;
  for (it = id_to_menu_map_.begin(); it != id_to_menu_map_.end(); ++it)
    it->second->set_delegate(nullptr);
}

void Menubar::SetActiveMenu(int id) {
  active_menu_id_ = id;
  std::map<int, views::MenuItemView*>::iterator it =
      id_to_menu_map_.find(active_menu_id_);
  if (it == id_to_menu_map_.end()) {
    Populate(active_menu_id_);
  }
  params_->delegate->OnMenuOpened(active_menu_id_);
}

bool Menubar::IsDarkTextColor(views::MenuItemView* menu) {
  views::Widget* parent = views::Widget::GetWidgetForNativeWindow(
      browser_->window()->GetNativeWindow());
  return color_utils::IsDark(TextColorForMenu(menu, parent));
}

// This function populates the toplevel of a menu. Sub menus are created on
// demand in WillShowMenu()
void Menubar::Populate(int id) {
  DCHECK(params_->delegate);
  // Menu models are owned by the delegate
  views::MenuItemView* root = new views::MenuItemView(this);
  id_to_menu_map_[id] = root;
  ui::MenuModel* menu_model = nullptr;
  params_->delegate->PopulateModel(id, IsDarkTextColor(root), &menu_model);
  DCHECK(menu_model);
  PopulateMenu(root, menu_model);

  if (params_->delegate->IsBookmarkMenu(id)) {
    bookmark_menu_ = root;
  }
}

void Menubar::PopulateBookmarks() {
  if (bookmark_menu_delegate_) {
    return;
  }

  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser_->profile());
  if (!model->loaded()) {
    return;
  }

  views::Widget* parent = views::Widget::GetWidgetForNativeWindow(
      browser_->window()->GetNativeWindow());
  bookmark_menu_delegate_.reset(new BookmarkMenuDelegate(
      browser_,
      parent));
  bookmark_menu_delegate_->Init(this, bookmark_menu_,
                                model->bookmark_bar_node(), 0,
                                BookmarkMenuDelegate::HIDE_PERMANENT_FOLDERS,
                                BookmarkLaunchLocation::kNone);
}

void Menubar::PopulateMenu(views::MenuItemView* parent, ui::MenuModel* model) {
  for (int i = 0, max = model->GetItemCount(); i < max; ++i) {
    // Add the menu item at the end.
    int menu_index =
        parent->HasSubmenu()
            ? static_cast<int>(parent->GetSubmenu()->children().size())
            : 0;
    AddMenuItem(parent, menu_index, model, i, model->GetTypeAt(i));

    if (model->GetTypeAt(i) == ui::MenuModel::TYPE_SUBMENU) {
      id_to_menumodel_map_[model->GetCommandIdAt(i)] =
          model->GetSubmenuModelAt(i);
    }
  }
}

views::MenuItemView* Menubar::AddMenuItem(views::MenuItemView* parent,
                                          int menu_index,
                                          ui::MenuModel* model,
                                          int model_index,
                                          ui::MenuModel::ItemType menu_type) {
  int command_id = model->GetCommandIdAt(model_index);
  views::MenuItemView* menu_item =
      views::MenuModelAdapter::AddMenuItemFromModelAt(
          model, model_index, parent, menu_index, command_id);

  if (menu_item) {
    if (model->GetTypeAt(model_index) == ui::MenuModel::TYPE_COMMAND) {
      std::string url;
      if (params_->delegate->GetUrl(command_id, &url)) {
        RequestFavicon(command_id, active_menu_id_, url);
      }
    }
  }

  return menu_item;
}

void Menubar::RunMenu(views::Widget* parent) {
  if (active_menu_id_ < 0) {
    // This will release the api instance
    params_->delegate->OnMenuClosed();
  } else {
    int32_t types = views::MenuRunner::HAS_MNEMONICS;
    if (run_types_ & views::MenuRunner::SHOULD_SHOW_MNEMONICS)
      types |= views::MenuRunner::SHOULD_SHOW_MNEMONICS;
    // The root menus (the one we create here and its siblings) will be managed
    // by the menu runner and released when menu runner terminates.
    views::MenuItemView* root = id_to_menu_map_[active_menu_id_];
    menu_runner_.reset(new views::MenuRunner(root, types));

    const gfx::Rect& rect = params_->GetSibling(active_menu_id_)->rect;
    menu_runner_->RunMenuAt(parent, nullptr, rect,
                            views::MenuAnchorPosition::kTopLeft,
                            ui::MENU_SOURCE_NONE);
  }
}

void Menubar::WillShowMenu(views::MenuItemView* menu) {
  int id = menu->GetCommand();
  ui::MenuModel* menu_model = id_to_menumodel_map_[id];
  if (menu_model && menu_model->GetItemCount() == 0) {
    params_->delegate->PopulateSubmodel(id, IsDarkTextColor(menu), menu_model);
    PopulateMenu(menu, id_to_menumodel_map_[id]);
    if (params_->delegate->IsBookmarkMenu(id)) {
      bookmark_menu_ = menu;
    }
  }

  if (has_been_shown_ == false) {
    has_been_shown_ = true;
    // When using this class for the Vivaldi menu we support shortcuts opening
    // a sub menu as a child of the Vivaldi menu once the latter opens. This can
    // only happen one time, that is, when the Vivaldi menu opens.
    views::MenuItemView* item =
        menu->GetMenuItemByID(params_->delegate->GetSelectedMenuId());
    if (item) {
      views::MenuController::GetActiveInstance()->VivaldiOpenMenu(item);
    }
  }

  if (menu == bookmark_menu_) {
    // Top level
    vivaldi::BookmarkMenuContainer* container =
        params_->delegate->GetBookmarkMenuContainer();
    if (container) {
      vivaldi::SetBookmarkContainer(container, 0);
      PopulateBookmarks();
    }

    // Top level
    PopulateBookmarks();
  } else if (bookmark_menu_delegate_) {
    // Bookmark sub menu
    bookmark_menu_delegate_->WillShowMenu(menu);
  }
}

void Menubar::CloseMenu() {
  if (menu_runner_.get())
    menu_runner_->Cancel();
}

bool Menubar::IsShowing() const {
  return menu_runner_.get() && menu_runner_->IsRunning();
}

bool Menubar::ShouldExecuteCommandWithoutClosingMenu(int id,
                                                     const ui::Event& e) {
  if (IsBookmarkCommand(id) || IsVivaldiMenuItem(id)) {
    bookmark_menu_delegate_->ShouldExecuteCommandWithoutClosingMenu(id, e);
  }
  return params_->delegate->IsItemPersistent(id);
}

// We want all menus to open under or over the menu bar to prevent long menus
// from opening left or right to the menu bar button. This would prevent proper
// bar navigation.
bool Menubar::ShouldTryPositioningBesideAnchor() const {
  return false;
}

bool Menubar::VivaldiShouldTryPositioningInMenuBar() const {
  return true;
}

void Menubar::ExecuteCommand(int id, int mouse_event_flags) {
  if (IsBookmarkCommand(id) || IsVivaldiMenuItem(id)) {
    bookmark_menu_delegate_->ExecuteCommand(id, mouse_event_flags);
  } else {
    params_->delegate->OnAction(id, mouse_event_flags);
  }
}

// This happens only when a menu is closed and no new is opened elsewhere.
void Menubar::OnMenuClosed(views::MenuItemView* menu) {
  params_->delegate->OnMenuClosed();
}

bool Menubar::IsTriggerableEvent(views::MenuItemView* menu,
                                 const ui::Event& e) {
  return IsBookmarkCommand(menu->GetCommand())
             ? bookmark_menu_delegate_->IsTriggerableEvent(menu, e)
             : MenuDelegate::IsTriggerableEvent(menu, e);
}

void Menubar::VivaldiSelectionChanged(views::MenuItemView* menu) {
  if (IsBookmarkCommand(menu->GetCommand())) {
    bookmark_menu_delegate_->VivaldiSelectionChanged(menu);
  }
}

bool Menubar::ShowContextMenu(views::MenuItemView* source,
                              int command_id,
                              const gfx::Point& p,
                              ui::MenuSourceType source_type) {
  return IsBookmarkCommand(command_id)
             ? bookmark_menu_delegate_->ShowContextMenu(source, command_id, p,
                                                        source_type)
             : false;
}

bool Menubar::IsItemChecked(int id) const {
  return params_->delegate->IsItemChecked(id);
}

bool Menubar::IsCommandEnabled(int id) const {
  return params_->delegate->IsItemEnabled(id);
}

bool Menubar::GetAccelerator(int id, ui::Accelerator* accelerator) const {
  return params_->delegate->GetAccelerator(id, accelerator);
}

views::MenuItemView* Menubar::GetVivaldiSiblingMenu(
    views::MenuItemView* menu,
    const gfx::Point& screen_point,
    gfx::Rect* rect,
    views::MenuAnchorPosition* anchor) {
  for (const MenubarMenuEntry& e : params_->siblings) {
    if (e.rect.Contains(screen_point)) {
      if (e.id == active_menu_id_) {
        return nullptr;
      }
      *rect = e.rect;
      *anchor = views::MenuAnchorPosition::kTopLeft;
      SetActiveMenu(e.id);
      return id_to_menu_map_[active_menu_id_];
    }
  }
  return nullptr;
}

views::MenuItemView* Menubar::GetNextSiblingMenu(
    bool next,
    bool* has_mnemonics,
    gfx::Rect* rect,
    views::MenuAnchorPosition* anchor) {
  if (params_->siblings.size() == 0) {
    return nullptr;
  }

  unsigned int index = 0;
  for (const MenubarMenuEntry& e : params_->siblings) {
    if (e.id == active_menu_id_) {
      break;
    }
    index++;
  }
  if (next) {
    index++;
    if (index >= params_->siblings.size()) {
      index = 0;
    }
  } else {
    if (index == 0) {
      index = params_->siblings.size() - 1;
    } else {
      index--;
    }
  }
  *has_mnemonics = true;
  return GetVivaldiSiblingMenu(
      nullptr, params_->siblings.at(index).rect.origin(), rect, anchor);
}

// Note: This is not used by bookmarks. That uses a separate system.
void Menubar::RequestFavicon(int id, int menu_id, const std::string& url) {
  if (!favicon_service_) {
    favicon_service_ = FaviconServiceFactory::GetForProfile(
        browser_->profile(), ServiceAccessType::EXPLICIT_ACCESS);
    if (!favicon_service_)
      return;
  }

  favicon_base::FaviconImageCallback callback = base::BindOnce(
      &Menubar::OnFaviconAvailable, base::Unretained(this), id, menu_id);

  favicon_service_->GetFaviconImageForPageURL(GURL(url), std::move(callback),
                                              &cancelable_task_tracker_);
}

void Menubar::OnFaviconAvailable(
    int id,
    int menu_id,
    const favicon_base::FaviconImageResult& image_result) {
  if (!image_result.image.IsEmpty()) {
    std::map<int, views::MenuItemView*>::iterator it =
        id_to_menu_map_.find(menu_id);
    if (it != id_to_menu_map_.end()) {
      it->second->GetMenuItemByID(id)->SetIcon(
          ui::ImageModel::FromImage(image_result.image));
    }
  }
}

}  // namespace vivaldi