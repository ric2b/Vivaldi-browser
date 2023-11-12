//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
#ifndef BROWSER_MENUS_VIVALDI_MENUBAR_H_
#define BROWSER_MENUS_VIVALDI_MENUBAR_H_

#include <map>

#include "base/task/cancelable_task_tracker.h"
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
}  // namespace views

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

  // MenuDelegate overrides:
  bool ShouldExecuteCommandWithoutClosingMenu(int id,
                                              const ui::Event& e) override;
  bool ShouldTryPositioningBesideAnchor() const override;
  bool VivaldiShouldTryPositioningInMenuBar() const override;
  void ExecuteCommand(int id, int mouse_event_flags) override;
  bool IsItemChecked(int id) const override;
  bool IsCommandEnabled(int id) const override;
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

  views::MenuItemView* GetVivaldiSiblingMenu(
      views::MenuItemView* menu,
      const gfx::Point& screen_point,
      gfx::Rect* rect,
      views::MenuAnchorPosition* anchor) override;
  views::MenuItemView* GetNextSiblingMenu(
      bool next,
      bool* has_mnemonics,
      gfx::Rect* rect,
      views::MenuAnchorPosition* anchor) override;

 private:
  typedef std::map<int, views::MenuItemView*> IdToMenuMap;
  typedef std::map<int, ui::MenuModel*> IdToMenuModelMap;

  bool IsDarkTextColor(views::MenuItemView* menu);
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

  raw_ptr<views::MenuItemView> bookmark_menu_ = nullptr;
  std::unique_ptr<BookmarkMenuDelegate> bookmark_menu_delegate_;

  std::unique_ptr<views::MenuRunner> menu_runner_;
  const raw_ptr<Browser> browser_;
  const raw_ptr<MenubarMenuParams> params_;
  raw_ptr<favicon::FaviconService> favicon_service_ = nullptr;
  base::CancelableTaskTracker cancelable_task_tracker_;
  int run_types_;
  int active_menu_id_ = -1;
  IdToMenuMap id_to_menu_map_;
  IdToMenuModelMap id_to_menumodel_map_;
  bool has_been_shown_ = false;
};

}  // namespace vivaldi

#endif