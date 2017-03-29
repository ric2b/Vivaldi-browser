//
// Copyright (c) 2014-2015 Vivaldi Technologies AS. All rights reserved.
//

#ifndef CHROME_BROWSER_EXTENSIONS_API_SHOW_MENU_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SHOW_MENU_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/common/extensions/api/show_menu.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"

namespace content {
  class BrowserContext;
}

namespace extensions {

// Based on BookmarkEventRouter, send command events to the javascript.
class CommandEventRouter {
 public:
    explicit CommandEventRouter(Profile* profile);
    ~CommandEventRouter();
    void CommandExecuted(int command_id);

 private:
    // Helper to actually dispatch an event to extension listeners.
    void DispatchEvent(const std::string& event_name,
                       scoped_ptr<base::ListValue> event_args);
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

    void CommandExecuted(int command_id);

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


class ShowMenuCreateFunction;

class VivaldiMenuObserver : public RenderViewContextMenuObserver {
 public:
  VivaldiMenuObserver(RenderViewContextMenuBase* proxy,
    std::vector<linked_ptr<api::show_menu::MenuItem>>* menu_items,
    ShowMenuCreateFunction *ptr);
  ~VivaldiMenuObserver() override;

  void InitMenu(const content::ContextMenuParams& params) override;
  bool IsCommandIdSupported(int command_id) override;
  bool IsCommandIdEnabled(int command_id) override;
  bool IsCommandIdChecked(int command_id) override;
  void ExecuteCommand(int command_id) override;
  void OnMenuCancel() override;

 private:

   void AddMenuHelper(
     const api::show_menu::MenuItem *item,
     ui::SimpleMenuModel *menu_model);

  RenderViewContextMenuBase* proxy_;
  std::vector<linked_ptr<api::show_menu::MenuItem>>* menu_items_;
  ShowMenuCreateFunction *menuCreateCallback_;
};

class ShowMenuCreateFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("showMenu.create", SHOWMENU_CREATE)
  ShowMenuCreateFunction();
  void AddMenuHelper(const api::show_menu::MenuItem *item,
    ui::SimpleMenuModel* submenu,
    ui::SimpleMenuModel::Delegate* delegate,
    bool withSubmenu);
  void menuSelected(int command_id);

 protected:

  ~ShowMenuCreateFunction() override;

  // ExtensionFunction:
  bool RunAsync() override;
 private:
  // If the user click outside the menu we send -1 as result.
  bool menu_cancelled_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SHOW_MENU_API_H_
