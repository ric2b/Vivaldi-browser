//
// Copyright (c) 2014-2015 Vivaldi Technologies AS. All rights reserved.
//

#ifndef EXTENSIONS_API_CONTEXT_MENU_CONTEXT_MENU_API_H_
#define EXTENSIONS_API_CONTEXT_MENU_CONTEXT_MENU_API_H_

#include "extensions/browser/extension_function.h"
#include "extensions/schema/context_menu.h"

namespace extensions {

class ContextMenuAPI {
 public:
  static void RequestMenu(content::BrowserContext* browser_context,
                          int window_id,
                          int document_id,
                          const vivaldi::context_menu::DocumentParams& request);
};

class ContextMenuShowFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contextMenu.show", CONTEXTMENU_SHOW)
  ContextMenuShowFunction();

 private:
  ~ContextMenuShowFunction() override;

  // ExtensionFunction
  ExtensionFunction::ResponseAction Run() override;
};

class ContextMenuUpdateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contextMenu.update", CONTEXTMENU_UPDATE)
  ContextMenuUpdateFunction() = default;

 private:
  ~ContextMenuUpdateFunction() override = default;

  // ExtensionFunction
  ExtensionFunction::ResponseAction Run() override;
};


}  // namespace extensions

#endif  // EXTENSIONS_API_CONTEXT_MENU_CONTEXT_MENU_API_H_
