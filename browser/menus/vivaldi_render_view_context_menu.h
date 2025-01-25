//
// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.
//
#ifndef BROWSER_MENUS_VIVALDI_RENDER_VIEW_CONTEXT_MENU_H_
#define BROWSER_MENUS_VIVALDI_RENDER_VIEW_CONTEXT_MENU_H_

#include <map>
#include <vector>

#include "browser/menus/vivaldi_developertools_menu_controller.h"
#include "build/build_config.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "extensions/schema/context_menu.h"

namespace content {
class RenderFrameHost;
}

class NotesSubMenuObserver;

namespace vivaldi {

class DeviceMenuController;
class ExtensionsMenuController;
class PWALinkMenuController;
class ProfileMenuController;
class SpeechMenuController;

class VivaldiRenderViewContextMenu : public RenderViewContextMenu {
protected:
  friend class VivaldiContextMenuMac;
  friend class VivaldiContextMenuViews;
  friend class ContextMenuController;
 public:
  using Container = extensions::vivaldi::context_menu::Container;

  VivaldiRenderViewContextMenu(content::RenderFrameHost& render_frame_host,
                               const content::ContextMenuParams& params,
                               gfx::NativeView parent_view);
  ~VivaldiRenderViewContextMenu() override;

  class Delegate {
   public:
    virtual void OnDestroyed(VivaldiRenderViewContextMenu* menu) = 0;
    virtual bool GetShowShortcuts() = 0;
  };

  // Called by the owner of this controller.
  static VivaldiRenderViewContextMenu* Create(
      content::RenderFrameHost& render_frame_host,
      const content::ContextMenuParams& params,
      gfx::NativeView parent_view);
  // Called by the JS api request.
  static VivaldiRenderViewContextMenu* GetActive(int id);
  // Returns true for the context menu requests we want to set up from UI.
  static bool Supports(const content::ContextMenuParams& params);

  // Enables extra menu items for internal use.
  bool SupportsInspectTools();

  // RenderViewContextMenuProxy. To override location in tree.
  void AddMenuItem(int command_id, const std::u16string& title) override;
  void AddMenuItemWithIcon(int command_id,
                           const std::u16string& title,
                           const ui::ImageModel& icon) override;
  void AddCheckItem(int command_id, const std::u16string& title) override;
  void AddSeparator() override;
  void AddSubMenu(int command_id,
                  const std::u16string& label,
                  ui::MenuModel* model) override;
  void AddSubMenuWithStringIdAndIcon(int command_id,
                                     int message_id,
                                     ui::MenuModel* model,
                                     const ui::ImageModel& icon) override;
  void UpdateMenuItem(int command_id,
                      bool enabled,
                      bool hidden,
                      const std::u16string& title) override;
  void UpdateMenuIcon(int command_id, const ui::ImageModel& icon) override;

  void RemoveMenuItem(int command_id) override;
  void AddSpellCheckServiceItem(bool is_checked) override;
  void AddAccessibilityLabelsServiceItem(bool is_checked) override;

  // SimpleMenuModel::Delegate implementation.
  bool IsCommandIdVisible(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdChecked(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void OnMenuWillShow(ui::SimpleMenuModel* source) override;
  void MenuClosed(ui::SimpleMenuModel* source) override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;
  void VivaldiCommandIdHighlighted(int command_id) override;

  int GetStaticIdForAction(std::string command);
  ui::ImageModel GetImageForAction(std::string command);
  ui::ImageModel GetImageForContainer(const Container& container);

  void ContainerWillOpen(ui::SimpleMenuModel* menu_model);
  bool HasContainerContent(const Container& container);
  void PopulateContainer(const Container& container,
                         int id,
                         bool dark_text_color,
                         ui::SimpleMenuModel* menu_model);

  // RenderViewContextMenu (we do not use the menu part, just handler functions)
  void Show() override;
  // RenderViewContextMenuBase
  void InitMenu() override;
  void RecordShownItem(int id, bool is_submenu) override {} // No need for this

  // Access to private members of RenderViewContextMenu for device controller
  const GURL& GetLinkUrl() const { return params_.link_url; }
  void SetLinkUrl(const GURL& url) { params_.link_url = url; }
  // Access to private members of RenderViewContextMenu for speech controller
  const std::u16string& GetSelectedText() const {
    return params_.selection_text;
  }

  // Access to the model to allow setting up the menu in an external builder.
  ui::SimpleMenuModel* root_menu_model() { return &menu_model_; }

  gfx::NativeView parent_view() { return parent_view_; }

  void SetCommandId(int command_id, int api_id);
  // Returns true if command id is mapped to an IDC value.
  bool IsCommandIdStatic(int command_id) const;
  void SetModelDelegate(ui::SimpleMenuModel::Delegate* delegate) {
    model_delegate_ = delegate;
  }
  void SetMenuDelegate(Delegate* delegate) { menu_delegate_ = delegate; }
  void SetWindowId(int window_id) { window_id_ = window_id; }

  void OnGetMobile();

 private:
  enum ActionChain {
    kStop,
    kContinue,
  };

  typedef std::map<int, int> IdToIntMap;
  typedef std::map<int, ui::SimpleMenuModel*> IdToSimpleMenuModelMap;

  static VivaldiRenderViewContextMenu* active_controller_;
  static int active_id_counter_;

  int id_;
  gfx::NativeView parent_view_;
  const raw_ptr<content::WebContents> embedder_web_contents_;
  IdToIntMap command_id_map_;
  IdToSimpleMenuModelMap menu_model_map_;
  // Delegate that handles commands with dynamic id.
  raw_ptr<ui::SimpleMenuModel::Delegate> model_delegate_ = nullptr;
  raw_ptr<Delegate> menu_delegate_ = nullptr;
  bool is_executing_command_ = false;
  bool is_webpage_widget_ = false;
  raw_ptr<ui::SimpleMenuModel> populating_menu_model_ = nullptr;
  int window_id_ = -1;

  std::vector<std::unique_ptr<ui::SimpleMenuModel>> models_;
  std::unique_ptr<ProfileMenuController> link_profile_controller_;
  std::unique_ptr<ProfileMenuController> image_profile_controller_;
  std::unique_ptr<ExtensionsMenuController> extensions_controller_;
  std::unique_ptr<DeviceMenuController> sendtopage_controller_;
  std::unique_ptr<DeviceMenuController> sendtolink_controller_;
  std::unique_ptr<NotesSubMenuObserver> note_submenu_observer_;
  std::unique_ptr<PWALinkMenuController> pwa_link_controller_;
#if BUILDFLAG(IS_MAC)
  std::unique_ptr<SpeechMenuController> speech_controller_;
#endif
  std::unique_ptr<DeveloperToolsMenuController> developertools_controller_;

  ActionChain HandleCommand(int command_id, int event_state);

  void AddMenuModelToMap(int command_id, ui::SimpleMenuModel* menu_model);
  ui::SimpleMenuModel* GetMappedMenuModel(int command_id);

  void AddNotesController(ui::SimpleMenuModel* menu_model,
                          int id,
                          bool is_folder);
};

}  // namespace vivaldi

#endif  // BROWSER_MENUS_VIVALDI_RENDER_VIEW_CONTEXT_MENU_H_
