//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
#ifndef BROWSER_MENUS_CONTEXT_MENU_CONTROLLER_H_
#define BROWSER_MENUS_CONTEXT_MENU_CONTROLLER_H_

#include <map>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/timer/timer.h"
#include "browser/menus/vivaldi_developertools_menu_controller.h"
#include "browser/menus/vivaldi_pwa_menu_controller.h"
#include "browser/menus/vivaldi_render_view_context_menu.h"
#include "extensions/schema/context_menu.h"
#include "ui/base/models/image_model.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/widget/widget_observer.h"

class Profile;
class VivaldiBrowserWindow;

namespace content {
class WebContents;
}

namespace favicon {
class FaviconService;
}

namespace favicon_base {
struct FaviconImageResult;
}

namespace ui {
class Accelerator;
}

namespace vivaldi {
class VivaldiContextMenu;

class ContextMenuPostitionDelegate {
 public:
  virtual bool CanSetPosition() const;
  virtual void SetPosition(gfx::Rect* menu_bounds,
                           const gfx::Rect& monitor_bounds,
                           const gfx::Rect& anchor_bounds) const {}
  virtual void ExecuteIfPersistent(int command_id,
                                   int event_flags,
                                   bool* success) {}
};

class ContextMenuController : public ui::SimpleMenuModel::Delegate,
                              public views::WidgetObserver,
                              public ContextMenuPostitionDelegate,
                              public VivaldiRenderViewContextMenu::Delegate {
 public:
  using Element = extensions::vivaldi::context_menu::Element;
  using Item = extensions::vivaldi::context_menu::Item;
  using Container = extensions::vivaldi::context_menu::Container;
  using Params = extensions::vivaldi::context_menu::Show::Params;
  using UpdateItem = extensions::vivaldi::context_menu::UpdateItem;

  static ContextMenuController* Create(
      VivaldiBrowserWindow* browser_window,
      VivaldiRenderViewContextMenu* rv_context_menu,
      std::optional<Params> params);

  static ContextMenuController* GetActive();

  ~ContextMenuController() override;

  Profile* GetProfile();

  bool Show();

  // ui::SimpleMenuModel::Delegate
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsItemForCommandIdDynamic(int command_id) const override;
  std::u16string GetLabelForCommandId(int command_id) const override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;
  void VivaldiCommandIdHighlighted(int command_id) override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void OnMenuWillShow(ui::SimpleMenuModel* source) override;
  void MenuClosed(ui::SimpleMenuModel* source) override;

  // views::WidgetObserver overrides.
  void OnWidgetDestroying(views::Widget* widget) override;

  // ContextMenuPostitionDelegate
  bool CanSetPosition() const override;
  void SetPosition(gfx::Rect* menu_bounds,
                   const gfx::Rect& monitor_bounds,
                   const gfx::Rect& anchor_bounds) const override;
  void ExecuteIfPersistent(int command_id,
                           int event_flags,
                           bool* success) override;

  // VivaldiRenderViewContextMenu::Delegate
  void OnDestroyed(VivaldiRenderViewContextMenu* menu) override;
  bool GetShowShortcuts() override;

  bool IsCommandIdPersistent(int command_id) const;
  bool Update(const std::vector<UpdateItem>& items);

 private:
  // When 'rv_context_menu' is a non-nullptr we will treat the root model of
  // that object as our root menu model. In that case it means it is the code of
  // VivaldiRenderViewContextMenu that will control how long this object will
  // live via MenuClosed.
  ContextMenuController(VivaldiBrowserWindow* browser_window,
                        VivaldiRenderViewContextMenu* rv_context_menu,
                        std::optional<Params> params);
  void InitModel();
  void PopulateModel(const Element& child,
                     bool dark_text_color,
                     ui::SimpleMenuModel* menu_model);
  ui::SimpleMenuModel* GetContainerModel(const Container& container,
                                         int id,
                                         ui::SimpleMenuModel* menu_model);
  void SanitizeModel(ui::SimpleMenuModel* menu_model);
  std::string GetEmptyIcon();
  void SetIcon(int command_id,
               const std::string& icon,
               ui::SimpleMenuModel* menu_model);
  void LoadFavicon(int command_id, const std::string& url, bool is_page);
  void OnFaviconDataAvailable(
      int command_id,
      const favicon_base::FaviconImageResult& image_result);
  void CleanupAndNotifyClose();

  typedef std::map<int, bool> IdToBoolMap;
  typedef std::map<int, std::string> IdToStringMap;
  typedef std::map<int, ui::Accelerator> IdToAcceleratorMap;

  static std::unique_ptr<ContextMenuController> active_controller_;

  raw_ptr<VivaldiBrowserWindow> browser_window_;
  const raw_ptr<content::WebContents> window_web_contents_;
  raw_ptr<VivaldiRenderViewContextMenu> rv_context_menu_;
  bool with_developer_tools_;
  bool has_shown_ = false;
  std::optional<Params> params_;

  // Loading favicons
  base::CancelableTaskTracker cancelable_task_tracker_;
  raw_ptr<favicon::FaviconService> favicon_service_ = nullptr;

  raw_ptr<ui::SimpleMenuModel> root_menu_model_ = nullptr;
  std::vector<std::unique_ptr<ui::SimpleMenuModel>> models_;
  std::vector<ui::SimpleMenuModel*> container_folder_models_;
  std::unique_ptr<VivaldiContextMenu> menu_;
  IdToStringMap id_to_url_map_;
  IdToBoolMap id_to_checked_map_;
  IdToBoolMap id_to_enabled_map_;
  IdToBoolMap id_to_persistent_map_;
  IdToAcceleratorMap id_to_accelerator_map_;
  bool has_closed_ = false;
  bool show_shortcuts_ = true;
  IdToStringMap id_to_action_map_;
  gfx::Rect rect_;

  std::unique_ptr<DeveloperToolsMenuController> developertools_controller_;
  std::unique_ptr<PWAMenuController> pwa_controller_;
  std::unique_ptr<base::OneShotTimer> timer_;

  base::WeakPtrFactory<ContextMenuController> weak_factory_{this};
};

}  // namespace vivaldi

#endif  // BROWSER_MENUS_CONTEXT_MENU_CONTROLLER_H_
