//
// Copyright (c) 2014-2015 Vivaldi Technologies AS. All rights reserved.
//

#ifndef EXTENSIONS_API_CONTEXT_MENU_CONTEXT_MENU_API_H_
#define EXTENSIONS_API_CONTEXT_MENU_CONTEXT_MENU_API_H_

#include "extensions/browser/extension_function.h"
#include "browser/menus/vivaldi_context_menu_controller.h"

namespace extensions {

class ContextMenuAPI {
 public:
  static void RequestMenu(content::BrowserContext* browser_context,
                          int window_id,
                          int document_id,
                          const vivaldi::context_menu::DocumentParams& request);
};

class ContextMenuShowFunction :
    public ExtensionFunction,
    public ::vivaldi::ContextMenuController::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION("contextMenu.show",
                             CONTEXTMENU_SHOW)
  ContextMenuShowFunction();

  // vivaldi::ContextMenuController::Delegate
  void OnAction(int command, int event_state) override;
  void OnHover(const std::string& url) override;
  void OnOpened() override;
  void OnClosed() override;

 protected:
  ~ContextMenuShowFunction() override;

  // ExtensionFunction
  ExtensionFunction::ResponseAction Run() override;

 private:
  std::unique_ptr<::vivaldi::ContextMenuController> controller_;
  DISALLOW_COPY_AND_ASSIGN(ContextMenuShowFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_CONTEXT_MENU_CONTEXT_MENU_API_H_
