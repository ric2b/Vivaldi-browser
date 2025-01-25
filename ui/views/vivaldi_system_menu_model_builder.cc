// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.
//
// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/vivaldi_system_menu_model_builder.h"

#include <string>
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/menus/simple_menu_model.h"

VivaldiSystemMenuModelBuilder::VivaldiSystemMenuModelBuilder(
    ui::AcceleratorProvider* provider,
    Browser* browser)
    : menu_delegate_(provider, browser) {}

VivaldiSystemMenuModelBuilder::~VivaldiSystemMenuModelBuilder() {}

void VivaldiSystemMenuModelBuilder::Init() {
  ui::SimpleMenuModel* model = new ui::SimpleMenuModel(&menu_delegate_);
  menu_model_.reset(model);
  BuildMenu(model);
#if BUILDFLAG(IS_WIN)
  // On Windows we put the menu items in the system menu (not at the end). Doing
  // this necessitates adding a trailing separator.
  model->AddSeparator(ui::NORMAL_SEPARATOR);
#endif
}

void VivaldiSystemMenuModelBuilder::BuildMenu(ui::SimpleMenuModel* model) {
  // We add the menu items in reverse order so that insertion_index never needs
  // to change.
  // TODO(pettern): Do we want a special menu for popups and settings?
  //  if (browser()->is_type_normal())
  BuildSystemMenuForBrowserWindow(model);
  //  else
  //    BuildSystemMenuForAppOrPopupWindow(model);
}

void VivaldiSystemMenuModelBuilder::BuildSystemMenuForBrowserWindow(
    ui::SimpleMenuModel* model) {
  //  model->AddItemWithStringId(IDC_NEW_TAB, IDS_NEW_TAB);
  model->AddItemWithStringId(IDC_RESTORE_TAB, IDS_RESTORE_TAB);
  if (chrome::CanOpenTaskManager()) {
    model->AddSeparator(ui::NORMAL_SEPARATOR);
    model->AddItemWithStringId(IDC_TASK_MANAGER, IDS_TASK_MANAGER);
  }
#if BUILDFLAG(IS_LINUX)
  model->AddSeparator(ui::NORMAL_SEPARATOR);
  model->AddCheckItemWithStringId(IDC_USE_SYSTEM_TITLE_BAR,
                                  IDS_SHOW_WINDOW_DECORATIONS_MENU);
#endif
  // If it's a regular browser window with tabs, we don't add any more items,
  // since it already has menus (Page, Chrome).
}

void VivaldiSystemMenuModelBuilder::BuildSystemMenuForAppOrPopupWindow(
    ui::SimpleMenuModel* model) {
  model->AddItemWithStringId(IDC_BACK, IDS_CONTENT_CONTEXT_BACK);
  model->AddItemWithStringId(IDC_FORWARD, IDS_CONTENT_CONTEXT_FORWARD);
  model->AddItemWithStringId(IDC_RELOAD, IDS_APP_MENU_RELOAD);
  model->AddSeparator(ui::NORMAL_SEPARATOR);
  if (browser()->is_type_app())
    model->AddItemWithStringId(IDC_NEW_TAB, IDS_APP_MENU_NEW_WEB_PAGE);
  else
    model->AddItemWithStringId(IDC_SHOW_AS_TAB, IDS_SHOW_AS_TAB);
  model->AddSeparator(ui::NORMAL_SEPARATOR);
  model->AddItemWithStringId(IDC_CUT, IDS_CUT);
  model->AddItemWithStringId(IDC_COPY, IDS_COPY);
  model->AddItemWithStringId(IDC_PASTE, IDS_PASTE);
  model->AddSeparator(ui::NORMAL_SEPARATOR);
  model->AddItemWithStringId(IDC_FIND, IDS_FIND);
  model->AddItemWithStringId(IDC_PRINT, IDS_PRINT);
  zoom_menu_contents_.reset(new ui::SimpleMenuModel(&menu_delegate_));
  model->AddSubMenuWithStringId(IDC_ZOOM_MENU, IDS_ZOOM_MENU,
                                zoom_menu_contents_.get());
  if (browser()->is_type_app() && chrome::CanOpenTaskManager()) {
    model->AddSeparator(ui::NORMAL_SEPARATOR);
    model->AddItemWithStringId(IDC_TASK_MANAGER, IDS_TASK_MANAGER);
  }
#if BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)
  model->AddSeparator(ui::NORMAL_SEPARATOR);
  model->AddItemWithStringId(IDC_CLOSE_WINDOW, IDS_CLOSE);
#endif
}
