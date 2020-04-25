#include "notes/notes_submenu_observer_helper_views.h"

#include "components/renderer_context_menu/views/toolkit_delegate_views.h"
#include "notes/notes_submenu_observer.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/submenu_view.h"


NotesSubMenuObserverHelper* CreateSubMenuObserverHelper(
    NotesSubMenuObserver* sub_menu_observer,
    RenderViewContextMenuBase::ToolkitDelegate* toolkit_delegate) {
  return new NotesSubMenuObserverHelperViews(sub_menu_observer,
      static_cast<ToolkitDelegateViews*>(toolkit_delegate));
}


NotesSubMenuObserverHelperViews::NotesSubMenuObserverHelperViews(
    NotesSubMenuObserver* sub_menu_observer,
    ToolkitDelegateViews* toolkit_delegate)
  :sub_menu_observer_(sub_menu_observer),
   toolkit_delegate_(toolkit_delegate) {
}

NotesSubMenuObserverHelperViews::~NotesSubMenuObserverHelperViews() {}

bool NotesSubMenuObserverHelperViews::SupportsDelayedLoading() {
  return true;
}

void NotesSubMenuObserverHelperViews::ExecuteCommand(int command_id,
                                                     int event_flags) {
  sub_menu_observer_->ExecuteCommand(command_id);
}

void NotesSubMenuObserverHelperViews::InitMap() {
  if (menumodel_to_view_map_.empty()) {
    ui::SimpleMenuModel* menu_model = sub_menu_observer_->get_root_model();
    int id = sub_menu_observer_->get_root_id();
    views::MenuItemView* root = toolkit_delegate_->vivaldi_get_menu_view();
    menumodel_to_view_map_[menu_model] = root->GetMenuItemByID(id);
  }
}

void NotesSubMenuObserverHelperViews::OnMenuWillShow(
    ui::SimpleMenuModel* menu_model) {
  if (!SupportsDelayedLoading()) {
    return;
  }

  InitMap();

  if (menu_model->GetItemCount() == 0) {
    // Fill up menu model
    sub_menu_observer_->PopulateModel(menu_model);
    // Use menu model to populate menu
    PopulateMenu(menumodel_to_view_map_[menu_model], menu_model);
  }
}

void NotesSubMenuObserverHelperViews::PopulateMenu(views::MenuItemView* parent,
                                                   ui::MenuModel* model) {

  for (int i = 0, max = model->GetItemCount(); i < max; ++i) {
    // Add the menu item at the end.
    int menu_index =
        parent->HasSubmenu() ? int{parent->GetSubmenu()->children().size()} : 0;
    views::MenuItemView* item = AddMenuItem(parent, menu_index, model, i,
        model->GetTypeAt(i));
    if (model->GetTypeAt(i) == ui::MenuModel::TYPE_SUBMENU) {
      menumodel_to_view_map_[model->GetSubmenuModelAt(i)] = item;
      toolkit_delegate_->VivaldiSetMenu(item, model->GetSubmenuModelAt(i));
    }
  }
}

views::MenuItemView* NotesSubMenuObserverHelperViews::AddMenuItem(
    views::MenuItemView* parent,
    int menu_index,
    ui::MenuModel* model,
    int model_index,
    ui::MenuModel::ItemType menu_type) {
  int command_id = model->GetCommandIdAt(model_index);
  views::MenuItemView* menu_item =
      views::MenuModelAdapter::AddMenuItemFromModelAt(
          model, model_index, parent, menu_index, command_id);

  if (menu_item) {
    // Flush all buttons to the right side of the menu for the new menu type.
    menu_item->set_use_right_margin(false);
    // If we want to load images / icons, this is the place. See
    // vivaldi_menubar.cc for how.
  }

  return menu_item;
}
