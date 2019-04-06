// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/renderer_context_menu/views/toolkit_delegate_views.h"

#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"

views::MenuItemView* ToolkitDelegateViews::VivaldiInit(
  ui::SimpleMenuModel* menu_model) {
  // NOTE(espen): Replicate ToolkitDelegateViews::Init, but without
  // views::MenuRunner::ASYNC. That flag does not work when we want to manage a
  // menu and execute its selected action from an extension. The extension
  // instance will deallocate while the menu is open with ASYNC set but we need
  // that instance alive when sending a reply after the menu closes. The flag
  // was added with Chrome 55.
  menu_adapter_.reset(new views::MenuModelAdapter(menu_model));
  menu_view_ = menu_adapter_->CreateMenu();
  menu_runner_.reset(
    new views::MenuRunner(menu_view_, views::MenuRunner::HAS_MNEMONICS |
      views::MenuRunner::CONTEXT_MENU));


  // Middle mouse button allows opening bookmarks in background.
  menu_adapter_->set_triggerable_event_flags(
    menu_adapter_->triggerable_event_flags() | ui::EF_MIDDLE_MOUSE_BUTTON);
  return menu_view_;
}

void ToolkitDelegateViews::VivaldiUpdateMenu(views::MenuItemView* view,
  ui::SimpleMenuModel* menu_model) {
  menu_adapter_->VivaldiUpdateMenu(view, menu_model);
}
