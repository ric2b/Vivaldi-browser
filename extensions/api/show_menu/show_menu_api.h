//
// Copyright (c) 2014-2015 Vivaldi Technologies AS. All rights reserved.
//

#ifndef EXTENSIONS_API_SHOW_MENU_SHOW_MENU_API_H_
#define EXTENSIONS_API_SHOW_MENU_SHOW_MENU_API_H_

#include <string>
#include <vector>

#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "components/favicon_base/favicon_types.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/schema/show_menu.h"

namespace content {
class BrowserContext;
}

namespace favicon {
class FaviconService;
}

namespace favicon_base {
struct FaviconImageResult;
}

namespace vivaldi {
class VivaldiContextMenu;
}

namespace extensions {

class ShowMenuCreateFunction;

class VivaldiMenuController : public ui::SimpleMenuModel::Delegate {
 public:
  class Delegate {
   public:
    virtual void OnMenuItemActivated(int command_id, int event_flags) = 0;
  };

  VivaldiMenuController(Delegate* delegate,
      std::vector<linked_ptr<vivaldi::show_menu::MenuItem>>* menu_items);
  ~VivaldiMenuController() override;

  void Show(content::WebContents* web_contents,
      const content::ContextMenuParams& menu_params);

  // ui::SimpleMenuModel::Delegate
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsItemForCommandIdDynamic(int command_id) const override;
  base::string16 GetLabelForCommandId(int command_id) const override;
  bool GetAcceleratorForCommandId(int command_id,
      ui::Accelerator* accelerator) override;
  bool GetIconForCommandId(int command_id, gfx::Image* icon) const override;
  void CommandIdHighlighted(int command_id) override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void MenuWillShow(ui::SimpleMenuModel* source) override;
  void MenuClosed(ui::SimpleMenuModel* source) override;

 private:
  const vivaldi::show_menu::MenuItem* getItemByCommandId(int command_id) const;
  void PopulateModel(const vivaldi::show_menu::MenuItem* menuitem,
      ui::SimpleMenuModel* menu_model);
  bool HasDeveloperTools();
  bool IsDeveloperTools(int command_id) const;
  void HandleDeveloperToolsCommand(int command_id);
  const Extension* GetExtension() const;
  void LoadFavicon(int command_id, const std::string& url);
  void OnFaviconDataAvailable(int command_id,
      const favicon_base::FaviconImageResult& image_result);

  // Loading favicons
  base::CancelableTaskTracker cancelable_task_tracker_;
  favicon::FaviconService* favicon_service_;

  Delegate* delegate_;
  content::ContextMenuParams menu_params_;
  std::vector<linked_ptr<vivaldi::show_menu::MenuItem>>*
      menu_items_;  // Not owned by us.
  ui::SimpleMenuModel menu_model_;
  scoped_ptr<::vivaldi::VivaldiContextMenu> menu_;
  ScopedVector<ui::SimpleMenuModel> models_;
  content::WebContents* web_contents_;  // Not owned by us.
  Profile* profile_;
  std::map<int,std::string*> url_map_;
  // State variables to reduce lookups in url_map_
  int current_highlighted_id_;
  bool is_url_highlighted_;
  // Initial selection
  int initial_selected_id_;
  bool is_shown_;
};

// Based on BookmarkEventRouter, send command events to the javascript.
class CommandEventRouter {
 public:
    explicit CommandEventRouter(Profile* profile);
    ~CommandEventRouter();

    // Helper to actually dispatch an event to extension listeners.
    void DispatchEvent(const std::string& event_name,
                       scoped_ptr<base::ListValue> event_args);
private:
    content::BrowserContext* browser_context_;
    DISALLOW_COPY_AND_ASSIGN(CommandEventRouter);
};

class ShowMenuAPI : public BrowserContextKeyedAPI,
public EventRouter::Observer {
 public:
    explicit ShowMenuAPI(content::BrowserContext* context);
    ~ShowMenuAPI() override;

    // KeyedService implementation.
    void Shutdown() override;

    // BrowserContextKeyedAPI implementation.
    static BrowserContextKeyedAPIFactory<ShowMenuAPI>* GetFactoryInstance();

    // EventRouter::Observer implementation.
    void OnListenerAdded(const EventListenerInfo& details) override;

    void CommandExecuted(int command_id, const std::string& parameter = "");
    void OnOpen();
    void OnUrlHighlighted(const std::string& url);

 private:
    friend class BrowserContextKeyedAPIFactory<ShowMenuAPI>;

    content::BrowserContext* browser_context_;

    // BrowserContextKeyedAPI implementation.
    static const char* service_name() {
      return "ShowMenuAPI";
    }
    static const bool kServiceIsNULLWhileTesting = true;

    // Created lazily upon OnListenerAdded.
    scoped_ptr<CommandEventRouter> command_event_router_;
};

class ShowMenuCreateFunction : public ChromeAsyncExtensionFunction,
                               public VivaldiMenuController::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION("showMenu.create", SHOWMENU_CREATE)
  ShowMenuCreateFunction();

  // VivaldiMenuController::Delegate
  void OnMenuItemActivated(int command_id, int event_flags) override;

 protected:
  ~ShowMenuCreateFunction() override;

  // ChromeAsyncExtensionFunction
  bool RunAsync() override;

 private:
  // If the user click outside the menu we send -1 as result.
  bool menu_cancelled_;

  DISALLOW_COPY_AND_ASSIGN(ShowMenuCreateFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_SHOW_MENU_SHOW_MENU_API_H_
