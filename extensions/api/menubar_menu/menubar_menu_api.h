//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
#ifndef EXTENSIONS_API_MENUBAR_MENU_API_H_
#define EXTENSIONS_API_MENUBAR_MENU_API_H_

#include <map>

#include "build/build_config.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "extensions/schema/menubar_menu.h"

namespace content {
class WebContents;
}

namespace ui {
class Accelerator;
}

namespace vivaldi {
struct BookmarkMenuContainer;
class MenubarController;
}

namespace extensions {

class MenubarMenuAPI : public BrowserContextKeyedAPI {
 public:
  explicit MenubarMenuAPI(content::BrowserContext* context);
  ~MenubarMenuAPI() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<MenubarMenuAPI>* GetFactoryInstance();

  // Functions that will send events to JS.
  static void SendAction(content::BrowserContext* browser_context,
                         int command,
                         int event_state,
                         bool persistent);
  static void SendOpenBookmark(content::BrowserContext* browser_context,
                               int window_id,
                               int64_t bookmark_id,
                               int event_state);
  static void SendBookmarkAction(content::BrowserContext* browser_context,
                                 int window_id,
                                 int64_t bookmark_id,
                                 int menu_id);
  static void SendOpen(content::BrowserContext* browser_context, int menu_id);
  static void SendClose(content::BrowserContext* browser_context);
  static void SendHover(content::BrowserContext* browser_context,
                        int window_id, const std::string& url);
  static void SendError(content::BrowserContext* browser_context,
                        const std::string& text);

 private:
  friend class BrowserContextKeyedAPIFactory<MenubarMenuAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "MenubarMenuAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // Hover url as reported by menu code. Cached here to avoid repeated events
  // with same value
  std::string hover_url_;
};

class MenubarMenuShowFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("menubarMenu.show", MENUBARMENU_SHOW)
  MenubarMenuShowFunction();

 private:
  ~MenubarMenuShowFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class MenubarMenuGetMaxIdFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("menubarMenu.getMaxId", MENUBARMENU_GET_MAX_ID)
  MenubarMenuGetMaxIdFunction() = default;

 private:
  ~MenubarMenuGetMaxIdFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};



}  // namespace extensions

#endif  // EXTENSIONS_API_MENUBAR_MENU_API_H_