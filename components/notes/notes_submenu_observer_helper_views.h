// Copyright (c) 2019 Vivaldi. All rights reserved.

#ifndef COMPONENTS_NOTES_NOTES_SUBMENU_OBSERVER_HELPER_VIEWS_H_
#define COMPONENTS_NOTES_NOTES_SUBMENU_OBSERVER_HELPER_VIEWS_H_

#include <map>

#include "components/notes/notes_submenu_observer_helper.h"

class NotesSubMenuObserver;
class ToolkitDelegateViews;

namespace views {
class MenuItemView;
}

// Helper class for NotesSubMenuObserver. Views specific
class NotesSubMenuObserverHelperViews : public NotesSubMenuObserverHelper {
 public:
  NotesSubMenuObserverHelperViews(NotesSubMenuObserver* sub_menu_observer,
                                  ToolkitDelegateViews* toolkit_delegate);
  ~NotesSubMenuObserverHelperViews() override;
  NotesSubMenuObserverHelperViews(const NotesSubMenuObserverHelperViews&) =
      delete;
  NotesSubMenuObserverHelperViews& operator=(
      const NotesSubMenuObserverHelperViews&) = delete;

  bool SupportsDelayedLoading() override;

  // SimpleMenuModel::Delegate via NotesSubMenuObserverHelper
  void ExecuteCommand(int command_id, int event_flags) override;
  void OnMenuWillShow(ui::SimpleMenuModel* source) override;

 private:
  typedef std::map<ui::MenuModel*, views::MenuItemView*> MenuModelToMenuView;

  void PopulateMenu(views::MenuItemView* parent, ui::MenuModel* model);
  views::MenuItemView* AddMenuItem(views::MenuItemView* parent,
                                   int menu_index,
                                   ui::MenuModel* model,
                                   int model_index,
                                   ui::MenuModel::ItemType menu_type);
  void InitMap();

  const raw_ptr<NotesSubMenuObserver> sub_menu_observer_;
  const raw_ptr<ToolkitDelegateViews> toolkit_delegate_;
  MenuModelToMenuView menumodel_to_view_map_;
};

#endif  // COMPONENTS_NOTES_NOTES_SUBMENU_OBSERVER_HELPER_VIEWS_H_
