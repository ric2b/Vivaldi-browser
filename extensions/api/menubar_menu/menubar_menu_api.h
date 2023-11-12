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
#include "ui/vivaldi_context_menu.h"

namespace content {
class WebContents;
}

namespace ui {
class Accelerator;
}

namespace vivaldi {
struct BookmarkMenuContainer;
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
                         int event_state);
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

#if BUILDFLAG(IS_MAC)
// Not used on Mac but we still need a stub
class MenubarMenuShowFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("menubarMenu.show", MENUBARMENU_SHOW)
  MenubarMenuShowFunction();

 private:
  ~MenubarMenuShowFunction() override;
  // ExtensionFunction:
  ResponseAction Run() override;
};
#else
class MenubarMenuShowFunction
    : public ExtensionFunction,
      public ::vivaldi::MenubarMenuParams::Delegate,
      public ::vivaldi::BookmarkMenuContainer::Delegate {
 public:
  using Element = extensions::vivaldi::menubar_menu::Element;
  using Item = extensions::vivaldi::menubar_menu::Item;
  DECLARE_EXTENSION_FUNCTION("menubarMenu.show", MENUBARMENU_SHOW)
  MenubarMenuShowFunction();

 private:
  typedef std::map<int, bool> IdToBoolMap;
  typedef std::map<int, ui::Accelerator> IdToAcceleratorMap;
  typedef std::map<int, std::string> IdToUrlMap;
  typedef std::map<int, const std::vector<Element>*> IdToElementVectorMap;

  ~MenubarMenuShowFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  std::string PopulateModel(absl::optional<vivaldi::menubar_menu::Show::Params> &params,
                            int menu_id,
                            bool dark_text_color,
                            const std::vector<Element>& list,
                            ui::SimpleMenuModel* menu_model);
  void SanitizeModel(ui::SimpleMenuModel* menu_model);
  std::string Open(content::WebContents* web_contents, int id);

  // vivaldi::MenubarMenuParams::Delegate
  void PopulateModel(int menu_id,
                     bool dark_text_color,
                     ui::MenuModel** menu_model) override;
  void PopulateSubmodel(int menu_id,
                        bool dark_text_color,
                        ui::MenuModel* menu_model) override;
  void OnMenuOpened(int menu_id) override;
  void OnMenuClosed() override;
  void OnAction(int command, int event_state) override;
  bool IsBookmarkMenu(int menu_id) override;
  int GetSelectedMenuId() override;
  bool IsItemChecked(int id) override;
  bool IsItemEnabled(int id) override;
  bool IsItemPersistent(int id) override;
  bool GetAccelerator(int id, ui::Accelerator* accelerator) override;
  bool GetUrl(int id, std::string* url) override;
  ::vivaldi::BookmarkMenuContainer* GetBookmarkMenuContainer() override;
  // vivaldi::BookmarkMenuContainer::Delegate
  void OnHover(const std::string& url) override;
  void OnOpenBookmark(int64_t bookmark_id, int event_state) override;
  void OnBookmarkAction(int64_t bookmark_id, int command) override;

  // Data used by the menu code outside this object.
  ::vivaldi::MenubarMenuParams menuParams_;
  // Parsed data from the JS layer.
  absl::optional<vivaldi::menubar_menu::Show::Params> params_;
  std::vector<std::unique_ptr<ui::SimpleMenuModel>> models_;
  std::unique_ptr<::vivaldi::BookmarkMenuContainer> bookmark_menu_container_;
  std::unique_ptr<::vivaldi::VivaldiMenubarMenu> menu_;

  IdToBoolMap id_to_checked_map_;
  IdToBoolMap id_to_disabled_map_;
  IdToBoolMap id_to_persistent_map_;
  IdToAcceleratorMap id_to_accelerator_map_;
  IdToUrlMap id_to_url_map_;
  // Vectors of parsed parameters. Each vector contains data for one sub menu.
  // Data is owned by menuParams_
  IdToElementVectorMap id_to_elementvector_map_;
  int bookmark_menu_id_ = -1;
  int selected_menu_id_ = -1;
};
#endif

}  // namespace extensions

#endif  // EXTENSIONS_API_MENUBAR_MENU_API_H_