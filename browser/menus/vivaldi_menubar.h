//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
#ifndef BROWSER_MENUS_VIVALDI_MENUBAR_H_
#define BROWSER_MENUS_VIVALDI_MENUBAR_H_

#include <map>

#include "ui/base/models/menu_model.h"
#include "ui/views/controls/menu/menu_delegate.h"

class Browser;
class BookmarkMenuDelegate;

namespace favicon {
class FaviconService;
}

namespace favicon_base {
struct FaviconImageResult;
}

namespace views {
class MenuItemView;
class MenuRunner;
}

namespace vivaldi {

struct MenubarMenuParams;


class Menubar : public views::MenuDelegate {
 public:
  Menubar(Browser* browser, MenubarMenuParams& params, int run_types);
  ~Menubar() override;

  void SetActiveMenu(int id);

  // Shows the menu relative to the specified controller's button.
  void RunMenu(views::Widget* parent);

  // Closes the menu if it is open, otherwise does nothing.
  void CloseMenu();

  // Whether the menu is currently visible to the user.
  bool IsShowing() const;

  bool for_drop() const {
    return (run_types_ & views::MenuRunner::FOR_DROP) != 0;
  }

  bool ShouldExecuteCommandWithoutClosingMenu(int id, const ui::Event& e) override;
  // MenuDelegate overrides:
  void ExecuteCommand(int id, int mouse_event_flags) override;
  //void ExecuteCommand(int id) override;
  bool IsItemChecked(int id) const override;
  bool GetAccelerator(int id, ui::Accelerator* accelerator) const override;

  void WillShowMenu(views::MenuItemView* menu) override;
  void OnMenuClosed(views::MenuItemView* menu) override;
  bool IsTriggerableEvent(views::MenuItemView* menu,
                          const ui::Event& e) override;
  void VivaldiSelectionChanged(views::MenuItemView* menu) override;

  bool ShowContextMenu(views::MenuItemView* source,
                       int command_id,
                       const gfx::Point& p,
                       ui::MenuSourceType source_type) override;

  views::MenuItemView* GetVivaldiSiblingMenu(views::MenuItemView* menu,
                                             const gfx::Point& screen_point,
                                             gfx::Rect* rect) override;
  views::MenuItemView* GetNextSiblingMenu(bool next, bool* has_mnemonics,
                                          gfx::Rect* rect) override;

 private:
  typedef std::map<int, views::MenuItemView*> IdToMenuMap;

  void Populate(int id);
  void PopulateBookmarks();
  void PopulateMenu(views::MenuItemView* parent, ui::MenuModel* model);
  views::MenuItemView* AddMenuItem(views::MenuItemView* parent,
                                   int menu_index,
                                   ui::MenuModel* model,
                                   int model_index,
                                   ui::MenuModel::ItemType menu_type);
  void RequestFavicon(int id, int menu_id, const std::string& url);
  void OnFaviconAvailable(int id,
                          int menu_id,
                          const favicon_base::FaviconImageResult& image_result);


  views::MenuItemView*  bookmark_menu_ = nullptr;
  std::unique_ptr<BookmarkMenuDelegate> bookmark_menu_delegate_;

  // The views menu. Owned by menu_runner_.
  //views::MenuItemView* root_ = nullptr;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  Browser* browser_;
  MenubarMenuParams* params_;
  favicon::FaviconService* favicon_service_ = nullptr;
  base::CancelableTaskTracker cancelable_task_tracker_;
  int run_types_;
  int active_menu_id_ = -1;
  IdToMenuMap id_to_menu_map_;
};

}  // vivaldi

#endif