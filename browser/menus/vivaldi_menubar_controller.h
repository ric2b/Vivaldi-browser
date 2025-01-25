//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
#ifndef BROWSER_MENUS_VIVALDI_MENUBAR_CONTROLLER_H_
#define BROWSER_MENUS_VIVALDI_MENUBAR_CONTROLLER_H_

#include <map>

#include "base/task/cancelable_task_tracker.h"
#include "extensions/schema/menubar_menu.h"
#include "ui/base/models/menu_model.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/vivaldi_context_menu.h"

class Browser;
class BookmarkMenuDelegate;
class Profile;
class VivaldiBrowserWindow;

namespace favicon {
class FaviconService;
}

namespace favicon_base {
struct FaviconImageResult;
}

namespace ui {
class SimpleMenuModel;
}  // namespace ui


namespace views {
class MenuItemView;
class MenuRunner;
}  // namespace views

namespace vivaldi {

struct BookmarkMenuContainer;
struct MenubarMenuParams;

class MenubarController : public views::MenuDelegate,
                          public views::WidgetObserver,
                          public BookmarkMenuContainer::Delegate {
 public:
  using Element = extensions::vivaldi::menubar_menu::Element;
  using Item = extensions::vivaldi::menubar_menu::Item;
  using Params = extensions::vivaldi::menubar_menu::Show::Params;

  static int GetMaximumId();

  static MenubarController* Create(
    VivaldiBrowserWindow* window,
    std::optional<Params> params);

  ~MenubarController() override;

  void SetActiveMenu(int id);

  // Opens the menu.
  void Show();

  // Closes the menu if it is open, otherwise does nothing.
  void CloseMenu();

  // Whether the menu is currently visible to the user.
  bool IsShowing() const;

  bool for_drop() const {
    return (run_types_ & views::MenuRunner::FOR_DROP) != 0;
  }

  void PopulateModel(int menu_id,
                     bool dark_text_color,
                     ui::MenuModel** menu_model);
  std::string PopulateModel(int menu_id,
                            bool dark_text_color,
                            const std::vector<Element>& list,
                            ui::SimpleMenuModel* menu_model);
  void PopulateSubmodel(int menu_id,
                        bool dark_text_color,
                        ui::MenuModel* menu_model);

  // views::MenuDelegate overrides.
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
                       ui::mojom::MenuSourceType source_type) override;
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

  // views::WidgetObserver overrides.
  void OnWidgetDestroying(views::Widget* widget) override;

  // BookmarkMenuContainer::Delegate overrides.
  void OnHover(const std::string& url) override;
  void OnOpenBookmark(int64_t bookmark_id, int event_state) override;
  void OnBookmarkAction(int64_t bookmark_id, int command) override;

  bool IsBookmarkMenu(int menu_id);
  int GetSelectedMenuId();
  bool IsItemEnabled(int id) const;
  bool IsItemPersistent(int id) const;
  bool GetUrl(int id, std::string* url) const;

  Browser* browser() const { return browser_.get(); }

 private:
  typedef std::map<int, views::MenuItemView*> IdToMenuMap;
  typedef std::map<int, ui::MenuModel*> IdToMenuModelMap;
  typedef std::map<int, bool> IdToBoolMap;
  typedef std::map<int, ui::Accelerator> IdToAcceleratorMap;
  typedef std::map<int, std::string> IdToUrlMap;
  typedef std::map<int, const std::vector<Element>*> IdToElementVectorMap;

  MenubarController(
    VivaldiBrowserWindow* window,
    std::optional<Params> params);

  Profile* GetProfile();
  bool IsDarkTextColor(views::MenuItemView* menu);
  void Populate(int id);
  void PopulateBookmarks();
  void PopulateMenu(views::MenuItemView* parent, ui::MenuModel* model);
  void SanitizeModel(ui::SimpleMenuModel* menu_model);
  views::MenuItemView* AddMenuItem(views::MenuItemView* parent,
                                   int menu_index,
                                   ui::MenuModel* model,
                                   int model_index,
                                   ui::MenuModel::ItemType menu_type);
  void RequestFavicon(int id, int menu_id, const std::string& url);
  void OnFaviconAvailable(int id,
                          int menu_id,
                          const favicon_base::FaviconImageResult& image_result);

  static std::unique_ptr<MenubarController> active_controller_;

  raw_ptr<views::MenuItemView> bookmark_menu_ = nullptr;
  std::unique_ptr<BookmarkMenuDelegate> bookmark_menu_delegate_;

  std::unique_ptr<views::MenuRunner> menu_runner_;
  raw_ptr<VivaldiBrowserWindow> browser_window_;
  const raw_ptr<content::WebContents> web_contents_;
  const raw_ptr<Browser> browser_;
  std::optional<Params> params_;
  MenubarMenuParams state_;
  raw_ptr<favicon::FaviconService> favicon_service_ = nullptr;
  base::CancelableTaskTracker cancelable_task_tracker_;
  int run_types_;

  std::vector<std::unique_ptr<ui::SimpleMenuModel>> models_;
  std::unique_ptr<::vivaldi::BookmarkMenuContainer> bookmark_menu_container_;

  IdToMenuMap id_to_menu_map_;
  IdToMenuModelMap id_to_menumodel_map_;
  IdToBoolMap id_to_checked_map_;
  IdToBoolMap id_to_disabled_map_;
  IdToBoolMap id_to_persistent_map_;
  IdToAcceleratorMap id_to_accelerator_map_;
  IdToUrlMap id_to_url_map_;
  // Vectors of parsed parameters. Each vector contains data for one sub menu.
  // Data is owned by menuParams_
  IdToElementVectorMap id_to_elementvector_map_;

  bool has_been_shown_ = false;
  int active_menu_id_ = -1;
  int bookmark_menu_id_ = -1;
  int selected_menu_id_ = -1;
};

}  // namespace vivaldi

#endif