//
// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "browser/menus/vivaldi_extensions_menu_controller.h"

#include "browser/menus/vivaldi_render_view_context_menu.h"
#include "chrome/browser/browser_process.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "ui/base/l10n/l10n_util.h"

namespace vivaldi {

ExtensionsMenuController::ExtensionsMenuController(
    VivaldiRenderViewContextMenu* rv_context_menu)
    : rv_context_menu_(rv_context_menu) {}

ExtensionsMenuController::~ExtensionsMenuController() {}

void ExtensionsMenuController::Populate(
    ui::SimpleMenuModel* menu_model,
    ui::SimpleMenuModel::Delegate* delegate,
    const extensions::Extension* extension,
    content::WebContents* source_web_contents,
    std::u16string printable_selection_text,
    base::RepeatingCallback<bool(const extensions::MenuItem*)> filter) {
  extension_items_.reset(new extensions::ContextMenuMatcher(
      rv_context_menu_->GetBrowserContext(), delegate, menu_model,
      std::move(filter)));
  AppendAllExtensionItems(printable_selection_text);
  AppendCurrentExtensionItems(extension, source_web_contents,
                              printable_selection_text);
}

void ExtensionsMenuController::AppendAllExtensionItems(
    std::u16string printable_selection_text) {
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(rv_context_menu_->GetBrowserContext());

  extensions::MenuManager* menu_manager =
      extensions::MenuManager::Get(rv_context_menu_->GetBrowserContext());
  if (!menu_manager)
    return;

  // Get a list of extension id's that have context menu items, and sort by the
  // top level context menu title of the extension.
  std::vector<std::u16string> sorted_menu_titles;
  std::map<std::u16string, std::vector<const extensions::Extension*>>
      title_to_extensions_map;
  for (const auto& id : menu_manager->ExtensionIds()) {
    const extensions::Extension* extension = registry->GetExtensionById(
        id.extension_id, extensions::ExtensionRegistry::ENABLED);
    // Platform apps have their context menus created directly in
    // AppendPlatformAppItems.
    // andre@vivaldi.com, that is not true as of now so lets use this.
    std::u16string menu_title = extension_items_->GetTopLevelContextMenuTitle(
        id, printable_selection_text);
    title_to_extensions_map[menu_title].push_back(extension);
    sorted_menu_titles.push_back(menu_title);
  }
  if (sorted_menu_titles.empty())
    return;

  const std::string app_locale = g_browser_process->GetApplicationLocale();
  l10n_util::SortStrings16(app_locale, &sorted_menu_titles);
  sorted_menu_titles.erase(
      std::unique(sorted_menu_titles.begin(), sorted_menu_titles.end()),
      sorted_menu_titles.end());

  int index = 0;
  auto ids = menu_manager->ExtensionIds();
  std::set<extensions::MenuItem::ExtensionKey>::iterator iter = ids.begin();
  for (const auto& title : sorted_menu_titles) {
    DCHECK(iter != ids.end());
    const std::vector<const extensions::Extension*>& extensions =
        title_to_extensions_map[title];
    for (const extensions::Extension* extension : extensions) {
      extensions::MenuItem::ExtensionKey extension_key(extension->id());
      const extensions::MenuItem::ExtensionKey extension_key_w_webviewid =
          *iter++;
      bool has_webview = (extension_key_w_webviewid.webview_instance_id !=
                          guest_view::kInstanceIDNone);
      extension_items_->AppendExtensionItems(
          has_webview ? extension_key_w_webviewid : extension_key,
          printable_selection_text, &index,
          /*is_action_menu=*/false);
    }
  }
}

void ExtensionsMenuController::AppendCurrentExtensionItems(
    const extensions::Extension* extension,
    content::WebContents* source_web_contents,
    std::u16string printable_selection_text) {
  // Avoid appending extension related items when |extension| is null.
  // For Panel, this happens when the panel is navigated to a url outside of the
  // extension's package.
  if (!extension)
    return;

  extensions::WebViewGuest* web_view_guest =
      extensions::WebViewGuest::FromWebContents(source_web_contents);
  extensions::MenuItem::ExtensionKey key;
  if (web_view_guest) {
    key =
        extensions::MenuItem::ExtensionKey(extension->id(),
                                           web_view_guest->owner_web_contents()
                                               ->GetPrimaryMainFrame()
                                               ->GetProcess()
                                               ->GetID(),
                                           web_view_guest->owner_web_contents()
                                               ->GetPrimaryMainFrame()
                                               ->GetRoutingID(),
                                           web_view_guest->view_instance_id());
  } else {
    key = extensions::MenuItem::ExtensionKey(extension->id());
  }

  // Only add extension items from this extension.
  int index = 0;
  extension_items_->AppendExtensionItems(key, printable_selection_text, &index,
                                         /*is_action_menu=*/false);
}

}  // namespace vivaldi