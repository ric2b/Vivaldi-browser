// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/main_menu_builder.h"

#include "browser/mac/vivaldi_main_menu_builder.h"

#include "app/vivaldi_resources.h"
#include "app/vivaldi_commands.h"
#include "chrome/app/chrome_command_ids.h"
#include "ui/base/l10n/l10n_util.h"

namespace vivaldi {
namespace {
using Item = chrome::internal::MenuItemBuilder;

base::scoped_nsobject<NSMenuItem> BuildAppMenu(NSApplication* nsapp,
                                               AppController* app_controller) {

  base::string16 appname(l10n_util::GetStringUTF16(IDS_VIVALDI_APP_NAME));

  base::scoped_nsobject<NSMenuItem> item =
      Item(IDS_VIVALDI_APP_NAME)
          .tag(IDC_CHROME_MENU)
          .submenu({
            Item(IDS_ABOUT_MAC)
                .string_format_1(appname)
                .tag(IDC_ABOUT)
                .target(app_controller)
                .action(@selector(orderFrontStandardAboutPanel:)),
            Item(IDS_HIDE_APP_MAC)
                .string_format_1(appname)
                .tag(IDC_HIDE_APP)
                .target(app_controller)
                .action(@selector(hide:)),
            Item(IDS_EXIT_MAC)
                .string_format_1(appname)
                .tag(IDC_EXIT)
                .target(nsapp)
                .action(@selector(terminate:)),
              })
          .Build();
  NSMenuItem* services_item = [[item submenu] itemWithTag:-1];
  [services_item setTag:0];

  [nsapp setServicesMenu:[services_item submenu]];

  return item;
}

base::scoped_nsobject<NSMenuItem> BuildFileMenu(NSApplication* nsapp,
                                                AppController* app_controller) {
  base::scoped_nsobject<NSMenuItem> item =
      Item(IDS_FILE_MENU_MAC)
          .tag(IDC_FILE_MENU)
          .submenu({
            Item(IDS_NEW_TAB_MAC).command_id(IDC_NEW_TAB),
            Item(IDS_NEW_WINDOW_MAC).command_id(IDC_NEW_WINDOW),
            Item(IDS_VIV_NEW_PRIVATE_WINDOW)
                .command_id(IDC_NEW_INCOGNITO_WINDOW),
            Item(IDS_OPEN_FILE_MAC).command_id(IDC_OPEN_FILE),
            Item(IDS_OPEN_LOCATION_MAC).command_id(IDC_FOCUS_LOCATION),
            Item().is_separator(),
            // AppKit inserts "Close All" as an alternate item automatically
            // by using the -performClose: action.
            Item(IDS_CLOSE_TAB_MAC).command_id(IDC_CLOSE_TAB),
            Item(IDS_CLOSE_WINDOW_MAC)
                .tag(IDC_CLOSE_WINDOW)
                .action(@selector(performClose:)),
            Item(IDS_SAVE_PAGE_MAC).command_id(IDC_SAVE_PAGE),
            Item().is_separator(),
            Item().is_separator(),
            Item(IDS_PRINT).command_id(IDC_PRINT),
          })
          .Build();

  return item;
}

base::scoped_nsobject<NSMenuItem> BuildEditMenu(NSApplication* nsapp,
                                                AppController* app_controller) {
  base::scoped_nsobject<NSMenuItem> item =
      Item(IDS_EDIT_MENU_MAC)
          .tag(IDC_EDIT_MENU)
          .submenu({
            Item(IDS_EDIT_UNDO_MAC)
                .tag(IDC_CONTENT_CONTEXT_UNDO)
                .action(@selector(undo:)),
            Item(IDS_EDIT_REDO_MAC)
                .tag(IDC_CONTENT_CONTEXT_REDO)
                .action(@selector(redo:)),
            Item().is_separator(),
            Item(IDS_CUT_MAC)
                .tag(IDC_CONTENT_CONTEXT_CUT)
                .action(@selector(cut:)),
            Item(IDS_COPY_MAC)
                .tag(IDC_CONTENT_CONTEXT_COPY)
                .action(@selector(copy:)),
            Item(IDS_PASTE_MAC)
                .tag(IDC_CONTENT_CONTEXT_PASTE)
                .action(@selector(paste:)),
            Item(IDS_PASTE_MATCH_STYLE_MAC)
                .tag(IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE)
                .action(@selector(pasteAndMatchStyle:)),
            Item(IDS_EDIT_DELETE_MAC)
                .tag(IDC_CONTENT_CONTEXT_DELETE)
                .action(@selector(delete:)),
            Item(IDS_EDIT_SELECT_ALL_MAC)
                .tag(IDC_CONTENT_CONTEXT_SELECTALL)
                .action(@selector(selectAll:)),
            // The "Start Dictation..." and "Emoji & Symbols" items are
            // inserted by AppKit.
          })
          .Build();
  return item;
}

base::scoped_nsobject<NSMenuItem> BuildViewMenu(NSApplication* nsapp,
                                                AppController* app_controller) {
  base::scoped_nsobject<NSMenuItem> item =
      Item(IDS_VIEW_MENU_MAC)
          .tag(IDC_VIV_VIEW_MENU)
          .submenu({
            Item(IDS_TEXT_BIGGER_MAC).command_id(IDC_ZOOM_PLUS),
            Item(IDS_TEXT_SMALLER_MAC).command_id(IDC_ZOOM_MINUS),
            Item(IDS_TEXT_DEFAULT_MAC).command_id(IDC_ZOOM_NORMAL),
            Item().is_separator(),
            Item(IDS_ENTER_FULLSCREEN_MAC)
                .tag(IDC_FULLSCREEN)
                .action(@selector(toggleFullScreen:)),
          })
          .Build();
  return item;
}

base::scoped_nsobject<NSMenuItem> BuildBookmarksMenu(
    NSApplication* nsapp,
    AppController* app_controller) {
  base::scoped_nsobject<NSMenuItem> item =
      Item(IDS_BOOKMARKS_MENU)
          .tag(IDC_BOOKMARKS_MENU)
          .submenu({
            // Empty. Contents added elsewhere.
          })
          .Build();
  return item;
}

base::scoped_nsobject<NSMenuItem> BuildToolsMenu(
    NSApplication* nsapp,
    AppController* app_controller) {
  base::scoped_nsobject<NSMenuItem> item = Item(IDS_VIV_TOOLS_MENU)
                                               .tag(IDC_VIV_TOOLS_MENU)
                                               .submenu({})
                                               .Build();
  return item;
}

base::scoped_nsobject<NSMenuItem> BuildWindowMenu(
    NSApplication* nsapp,
    AppController* app_controller) {
  base::scoped_nsobject<NSMenuItem> item =
      Item(IDS_WINDOW_MENU_MAC)
          .tag(IDC_WINDOW_MENU)
          .submenu({
            Item(IDS_MINIMIZE_WINDOW_MAC)
                .tag(IDC_MINIMIZE_WINDOW)
                .action(@selector(performMiniaturize:)),
                Item(IDS_ZOOM_WINDOW_MAC)
                    .tag(IDC_MAXIMIZE_WINDOW)
                    .action(@selector(performZoom:)),
                Item().is_separator(),
                Item(IDS_ALL_WINDOWS_FRONT_MAC)
                    .tag(IDC_ALL_WINDOWS_FRONT)
                    .action(@selector(arrangeInFront:)),
                Item().is_separator(),
          })
          .Build();
  [nsapp setWindowsMenu:[item submenu]];
  return item;
}

base::scoped_nsobject<NSMenuItem> BuildHelpMenu(NSApplication* nsapp,
                                                AppController* app_controller) {
  base::string16 productname(l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));

  base::scoped_nsobject<NSMenuItem> item =
      Item(IDS_HELP_MENU_MAC)
          .tag(IDC_VIV_HELP_MENU)
          .submenu({
                Item(IDS_HELP_MAC)
                    .string_format_1(productname)
                    .command_id(IDC_HELP_PAGE_VIA_MENU),
          })
          .Build();
  [nsapp setHelpMenu:[item submenu]];
  return item;
}

}

void BuildVivaldiMainMenu(NSApplication* nsapp, AppController* app_controller) {
  base::scoped_nsobject<NSMenu> main_menu([[NSMenu alloc] initWithTitle:@""]);

  using Builder =
      base::scoped_nsobject<NSMenuItem> (*)(NSApplication*, AppController*);
  static const Builder kBuilderFuncs[] = {
      &BuildAppMenu,    &BuildFileMenu,    &BuildEditMenu,
      &BuildViewMenu,   &BuildBookmarksMenu, &BuildToolsMenu,
      &BuildWindowMenu,  &BuildHelpMenu,
  };

  for (auto* builder : kBuilderFuncs) {
    [main_menu addItem:builder(nsapp, app_controller)];
  }

  [nsapp setMainMenu:main_menu];
}

}  // namespace vivaldi
