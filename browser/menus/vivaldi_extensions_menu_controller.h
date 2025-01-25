//
// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef BROWSER_MENUS_VIVALDI_EXTENSIONS_MENU_CONTROLLER_H_
#define BROWSER_MENUS_VIVALDI_EXTENSIONS_MENU_CONTROLLER_H_

#include "chrome/browser/extensions/menu_manager.h"
#include "ui/menus/simple_menu_model.h"

namespace extensions {
class ContextMenuMatcher;
class Extension;
}  // namespace extensions

namespace vivaldi {

class VivaldiRenderViewContextMenu;

class ExtensionsMenuController {
 public:
  ExtensionsMenuController(VivaldiRenderViewContextMenu* rv_context_menu);
  ~ExtensionsMenuController();

  void Populate(
      ui::SimpleMenuModel* menu_model,
      ui::SimpleMenuModel::Delegate* delegate,
      const extensions::Extension* extension,
      content::WebContents* source_web_contents,
      std::u16string printable_selection_text,
      base::RepeatingCallback<bool(const extensions::MenuItem*)> filter);
  extensions::ContextMenuMatcher* get_extension_items() const {
    return extension_items_.get();
  }

 private:
  const raw_ptr<VivaldiRenderViewContextMenu> rv_context_menu_;
  std::unique_ptr<extensions::ContextMenuMatcher> extension_items_;

  void AppendAllExtensionItems(std::u16string printable_selection_text);
  void AppendCurrentExtensionItems(const extensions::Extension* extension,
                                   content::WebContents* source_web_contents,
                                   std::u16string printable_selection_text);
};

}  // namespace vivaldi

#endif  // BROWSER_MENUS_VIVALDI_EXTENSIONS_CONTROLLER_H_