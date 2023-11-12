//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
#ifndef BROWSER_MENUS_CONTEXT_MENU_CONTROLLER_H_
#define BROWSER_MENUS_CONTEXT_MENU_CONTROLLER_H_

#include <map>
#include <vector>

#include "base/task/cancelable_task_tracker.h"
#include "base/timer/timer.h"
#include "browser/menus/vivaldi_developertools_menu_controller.h"
#include "browser/menus/vivaldi_pwa_menu_controller.h"
#include "browser/menus/vivaldi_render_view_context_menu.h"
#include "extensions/schema/context_menu.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/simple_menu_model.h"

class Profile;

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
  virtual void SetPosition(gfx::Rect* menu_bounds,
                           const gfx::Rect& monitor_bounds,
                           const gfx::Rect& anchor_bounds) const {}
};

class ContextMenuController : public ui::SimpleMenuModel::Delegate,
                              public ContextMenuPostitionDelegate,
                              public VivaldiRenderViewContextMenu::Delegate {
 public:
  using Element = extensions::vivaldi::context_menu::Element;
  using Item = extensions::vivaldi::context_menu::Item;
  using Container = extensions::vivaldi::context_menu::Container;
  using Params = extensions::vivaldi::context_menu::Show::Params;

  // When 'rv_context_menu' is a non-nullptr we will treat the root model of
  // that object as our root menu model. In that case it means it is the code of
  // VivaldiRenderViewContextMenu that will control how long this object will
  // live via MenuClosed.
  ContextMenuController(content::WebContents* window_web_contents,
                        VivaldiRenderViewContextMenu* rv_context_menu,
                        absl::optional<Params> params);
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

  // ContextMenuPostitionDelegate
  void SetPosition(gfx::Rect* menu_bounds,
                   const gfx::Rect& monitor_bounds,
                   const gfx::Rect& anchor_bounds) const override;

  // VivaldiRenderViewContextMenu::Delegate
  void OnDestroyed(VivaldiRenderViewContextMenu* menu) override;
  bool GetShowShortcuts() override;

 private:
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
  void Delete();

  typedef std::map<int, bool> IdToBoolMap;
  typedef std::map<int, std::string> IdToStringMap;
  typedef std::map<int, ui::Accelerator> IdToAcceleratorMap;

  const raw_ptr<content::WebContents> window_web_contents_;
  raw_ptr<VivaldiRenderViewContextMenu> rv_context_menu_;
  bool with_developer_tools_;
  bool has_shown_ = false;
  absl::optional<Params> params_;

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
  IdToAcceleratorMap id_to_accelerator_map_;
  bool show_shortcuts_ = true;
  IdToStringMap id_to_action_map_;
  gfx::Rect rect_;

  std::unique_ptr<DeveloperToolsMenuController> developertools_controller_;
  std::unique_ptr<PWAMenuController> pwa_controller_;
  std::unique_ptr<base::OneShotTimer> timer_;
};

}  // namespace vivaldi

#endif  // BROWSER_MENUS_CONTEXT_MENU_CONTROLLER_H_
