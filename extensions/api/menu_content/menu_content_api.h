//
// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved.
//
#ifndef EXTENSIONS_API_MENU_CONTENT_MENU_CONTENT_API_H_
#define EXTENSIONS_API_MENU_CONTENT_MENU_CONTENT_API_H_

#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "extensions/schema/menu_content.h"
#include "menus/menu_model_observer.h"

namespace menus {
class Menu_Model;
class Menu_Node;
}

namespace extensions {

class MenuContentAPI : public BrowserContextKeyedAPI,
                       public EventRouter::Observer,
                       public ::menus::MenuModelObserver {
 public:
  explicit MenuContentAPI(content::BrowserContext* context);
  ~MenuContentAPI() override = default;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<MenuContentAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override {}

  // menus::MenuModelObserver
  void MenuModelChanged(menus::Menu_Model* model,
                        int64_t select_id,
                        const std::string& menu_name) override;

  // Functions that will send events to JS.
  static void SendOnChanged(content::BrowserContext* context,
                            menus::Menu_Model* model,
                            int64_t select_id,
                            const std::string& named_menu);

  // KeyedService implementation.
  void Shutdown() override;

 private:
  friend class BrowserContextKeyedAPIFactory<MenuContentAPI>;

  content::BrowserContext* browser_context_;
  menus::Menu_Model* main_menu_model_ = nullptr;
  menus::Menu_Model* context_menu_model_ = nullptr;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "MenuContentAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;
};

class MenuContentGetFunction : public ExtensionFunction,
                               public ::menus::MenuModelObserver {
 public:
  DECLARE_EXTENSION_FUNCTION("menuContent.get", MENUCONTENT_GET)
  MenuContentGetFunction() = default;

  // ::menus::MenuModelObserver
  void MenuModelLoaded(menus::Menu_Model* model) override;

 private:
  ~MenuContentGetFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

  void SendResponse(menus::Menu_Model* model, const std::string& named_menu);

  DISALLOW_COPY_AND_ASSIGN(MenuContentGetFunction);
};

class MenuContentMoveFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("menuContent.move", MENUCONTENT_MOVE)
  MenuContentMoveFunction() = default;

 private:
  ~MenuContentMoveFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(MenuContentMoveFunction);
};

class MenuContentCreateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("menuContent.create", MENUCONTENT_CREATE)
  MenuContentCreateFunction() = default;

 private:
  ~MenuContentCreateFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(MenuContentCreateFunction);
};

class MenuContentRemoveFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("menuContent.remove", MENUCONTENT_REMOVE)
  MenuContentRemoveFunction() = default;

 private:
  ~MenuContentRemoveFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(MenuContentRemoveFunction);
};

class MenuContentRemoveActionFunction : public ExtensionFunction,
                                        public ::menus::MenuModelObserver {
 public:
  DECLARE_EXTENSION_FUNCTION("menuContent.removeAction",
                             MENUCONTENT_REMOVE_ACTION)
  MenuContentRemoveActionFunction() = default;

  // ::menus::MenuModelObserver
  void MenuModelLoaded(menus::Menu_Model* model) override;

 private:
  ~MenuContentRemoveActionFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
  bool HandleRemoval();

  DISALLOW_COPY_AND_ASSIGN(MenuContentRemoveActionFunction);
};

class MenuContentUpdateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("menuContent.update", MENUCONTENT_UPDATE)
  MenuContentUpdateFunction() = default;

 private:
  ~MenuContentUpdateFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(MenuContentUpdateFunction);
};

class MenuContentResetFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("menuContent.reset", MENUCONTENT_RESET)
  MenuContentResetFunction() = default;

 private:
  ~MenuContentResetFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(MenuContentResetFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_MENU_CONTENT_MENU_CONTENT_API_H_
