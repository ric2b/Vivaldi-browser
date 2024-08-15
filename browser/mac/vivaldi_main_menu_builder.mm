// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/main_menu_builder.h"

#include "browser/mac/vivaldi_main_menu_builder.h"

#include "app/vivaldi_resources.h"
#include "app/vivaldi_commands.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace vivaldi {
namespace {
using Item = chrome::internal::MenuItemBuilder;

NSMenuItem* BuildAppMenu(NSApplication* nsapp,
                         AppController* app_controller) {

  std::u16string appname(l10n_util::GetStringUTF16(IDS_VIVALDI_APP_NAME));

  NSMenuItem* item =
      Item(IDS_VIVALDI_APP_NAME)
          .tag(IDC_CHROME_MENU)
          .submenu({
            Item(IDS_ABOUT_MAC)
                .string_format_1(appname)
                .tag(IDC_ABOUT)
                .target(app_controller)
                .action(@selector(orderFrontStandardAboutPanel:)),
            Item(IDS_SERVICES_MAC).tag(-1).submenu({}),
            Item(IDS_HIDE_APP_MAC)
                .string_format_1(appname)
                .tag(IDC_HIDE_APP)
                .target(app_controller)
                .action(@selector(hide:)),
            Item(IDS_SHOW_ALL_MAC)
                .action(@selector(unhideAllApplications:)),
            Item(IDS_EXIT_MAC)
                .string_format_1(appname)
                .tag(IDC_EXIT)
                .target(nsapp)
                .action(@selector(terminate:)),
              })
          .Build();
  NSMenuItem* services_item = [item.submenu itemWithTag:-1];
  [services_item setTag:0];

  [nsapp setServicesMenu:services_item.submenu];

  return item;
}

NSMenuItem* BuildFileMenu(NSApplication* nsapp,
                          AppController* app_controller) {
  NSMenuItem* item =
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
            Item(IDS_SHARE_MAC).tag(-1).submenu({}),
            Item().is_separator(),
            Item(IDS_PRINT).command_id(IDC_PRINT),
          })
          .Build();

  return item;
}

NSMenuItem* BuildEditMenu(NSApplication* nsapp,
                          AppController* app_controller) {
  NSMenuItem* item =
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
            // Content below is not configurable. It will be tested for and kept
            // when setting up menus with data from UI.
            Item(IDS_EDIT_SPELLING_GRAMMAR_MAC)
                .tag(IDC_SPELLCHECK_MENU)
                .submenu({
                    Item(IDS_EDIT_SHOW_SPELLING_GRAMMAR_MAC)
                        .action(@selector(showGuessPanel:))
                        .key_equivalent(@":", NSEventModifierFlagCommand),
                    Item(IDS_EDIT_CHECK_DOCUMENT_MAC)
                        .action(@selector(checkSpelling:))
                        .key_equivalent(@";", NSEventModifierFlagCommand),
                    Item(IDS_EDIT_CHECK_SPELLING_TYPING_MAC)
                        .action(@selector(toggleContinuousSpellChecking:)),
                    Item(IDS_EDIT_CHECK_GRAMMAR_MAC)
                        .action(@selector(toggleGrammarChecking:)),
                }),
            Item(IDS_EDIT_SUBSTITUTIONS_MAC)
                .tag(IDC_VIV_SUBSTITUTIONS_MENU_MAC)
                .submenu({
                    Item(IDS_EDIT_SHOW_SUBSTITUTIONS_MAC)
                        .action(@selector(orderFrontSubstitutionsPanel:)),
                    Item().is_separator(),
                    Item(IDS_EDIT_SMART_QUOTES_MAC)
                        .action(@selector(toggleAutomaticQuoteSubstitution:)),
                    Item(IDS_EDIT_SMART_DASHES_MAC)
                        .action(@selector(toggleAutomaticDashSubstitution:)),
                    Item(IDS_EDIT_TEXT_REPLACEMENT_MAC)
                        .action(@selector(toggleAutomaticTextReplacement:)),
                }),
            Item(IDS_SPEECH_MAC)
                .tag(IDC_VIV_SPEECH_MENU_MAC)
                .submenu({
                    Item(IDS_SPEECH_START_SPEAKING_MAC)
                        .action(@selector(startSpeaking:)),
                    Item(IDS_SPEECH_STOP_SPEAKING_MAC)
                        .action(@selector(stopSpeaking:)),
                }),
             Item().is_separator()
                .tag(IDC_VIV_EDIT_SEPARATOR_MAC),
            // The "Start Dictation..." and "Emoji & Symbols" items are
            // inserted by AppKit.
          })
          .Build();
  return item;
}

NSMenuItem* BuildViewMenu(NSApplication* nsapp,
                          AppController* app_controller) {
  NSMenuItem* item =
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

NSMenuItem* BuildBookmarksMenu(NSApplication* nsapp,
                               AppController* app_controller) {
  NSMenuItem* item =
      Item(IDS_BOOKMARKS_MENU)
          .tag(IDC_BOOKMARKS_MENU)
          .submenu({
            // Empty. Contents added elsewhere.
          })
          .Build();
  return item;
}

NSMenuItem* BuildToolsMenu(NSApplication* nsapp,
                           AppController* app_controller) {
  NSMenuItem* item =
      Item(IDS_VIV_TOOLS_MENU)
          .tag(IDC_VIV_TOOLS_MENU)
          .submenu({})
          .Build();
  return item;
}

NSMenuItem* BuildWindowMenu(NSApplication* nsapp,
                            AppController* app_controller) {
  NSMenuItem* item =
      Item(IDS_WINDOW_MENU_MAC)
          .tag(IDC_WINDOW_MENU)
          .submenu({
            Item(IDS_ZOOM_WINDOW_MAC)
                .tag(IDC_MAXIMIZE_WINDOW)
                .action(@selector(performZoom:)),
            Item().is_separator(),
            Item(IDS_ALL_WINDOWS_FRONT_MAC)
                .tag(IDC_ALL_WINDOWS_FRONT)
                .action(@selector(arrangeInFront:)),
            Item().is_separator()
              .tag(IDC_VIV_WINDOW_SEPARATOR_MAC), // Start of dynamic tab list
              // configurable elements and tabs go here.
            Item().is_separator(), // seperator before open window list
          })
          .Build();
  [nsapp setWindowsMenu:item.submenu];
  return item;
}

NSMenuItem* BuildHelpMenu(NSApplication* nsapp,
                          AppController* app_controller) {
  std::u16string productname(l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));

  NSMenuItem* item =
      Item(IDS_HELP_MENU_MAC)
          .tag(IDC_VIV_HELP_MENU)
          .submenu({
                Item(IDS_HELP_MAC)
                    .string_format_1(productname)
                    .command_id(IDC_HELP_PAGE_VIA_MENU),
          })
          .Build();
  [nsapp setHelpMenu:item.submenu];
  return item;
}

}

void BuildVivaldiMainMenu(NSApplication* nsapp, AppController* app_controller) {
  NSMenu* main_menu = [[NSMenu alloc] initWithTitle:@""];

  for (auto* builder : {
            &BuildAppMenu,
            &BuildFileMenu,
            &BuildEditMenu,
            &BuildViewMenu,
            &BuildBookmarksMenu,
            &BuildToolsMenu,
            &BuildWindowMenu,
            &BuildHelpMenu,
       }) {
    auto item = builder(nsapp, app_controller);
    if (item)
      [main_menu addItem:item];
  }

  [nsapp setMainMenu:main_menu];
}

}  // namespace vivaldi
