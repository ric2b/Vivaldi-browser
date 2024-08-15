#include "components/notes/notes_submenu_observer_helper_views.h"

#include "components/notes/notes_submenu_observer.h"
#include "components/renderer_context_menu/views/toolkit_delegate_views.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/submenu_view.h"

NotesSubMenuObserverHelper* CreateSubMenuObserverHelper(
    NotesSubMenuObserver* sub_menu_observer,
    RenderViewContextMenuBase::ToolkitDelegate* toolkit_delegate) {
  return new NotesSubMenuObserverHelperViews(
      sub_menu_observer, static_cast<ToolkitDelegateViews*>(toolkit_delegate));
}

NotesSubMenuObserverHelperViews::NotesSubMenuObserverHelperViews(
    NotesSubMenuObserver* sub_menu_observer,
    ToolkitDelegateViews* toolkit_delegate)
    : sub_menu_observer_(sub_menu_observer),
      toolkit_delegate_(toolkit_delegate) {}

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
    int id = sub_menu_observer_->GetRootId();
    views::MenuItemView* root = toolkit_delegate_->vivaldi_get_menu_view();
    menumodel_to_view_map_[menu_model] = root->GetMenuItemByID(id);
    // In case the top node is not displayed in the menu (ie, we have a flat
    // view where the first level of notes are displayed directly), we have to
    // map the views of the first level of folders too.
    for (size_t i = 0; i < menu_model->GetItemCount(); i++) {
      ui::MenuModel* sub_menu_model = menu_model->GetSubmenuModelAt(i);
      if (sub_menu_model) {
        menumodel_to_view_map_[sub_menu_model] =
            root->GetMenuItemByID(menu_model->GetCommandIdAt(i));
      }
    }
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
        parent->HasSubmenu()
            ? static_cast<int>(parent->GetSubmenu()->children().size())
            : 0;
    views::MenuItemView* item =
        AddMenuItem(parent, menu_index, model, i, model->GetTypeAt(i));
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
    // If we want to load images / icons, this is the place. See
    // vivaldi_menubar.cc for how.
  }

  return menu_item;
}
