// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/renderer_context_menu/views/toolkit_delegate_views.h"

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"

ToolkitDelegateViews::ToolkitDelegateViews() : menu_view_(NULL) {}

ToolkitDelegateViews::~ToolkitDelegateViews() {}

void ToolkitDelegateViews::RunMenuAt(views::Widget* parent,
                                     const gfx::Point& point,
                                     ui::MenuSourceType type) {
  views::MenuAnchorPosition anchor_position =
      (type == ui::MENU_SOURCE_TOUCH ||
       type == ui::MENU_SOURCE_TOUCH_EDIT_MENU)
      ? views::MENU_ANCHOR_BOTTOMCENTER
      : views::MENU_ANCHOR_TOPLEFT;
  menu_runner_->RunMenuAt(parent, NULL, gfx::Rect(point, gfx::Size()),
                          anchor_position, type);
}

void ToolkitDelegateViews::Init(ui::SimpleMenuModel* menu_model) {
  menu_adapter_.reset(new views::MenuModelAdapter(menu_model));
  menu_view_ = menu_adapter_->CreateMenu();
  menu_runner_.reset(new views::MenuRunner(
      menu_view_,
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU));
}

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

void ToolkitDelegateViews::Cancel() {
  DCHECK(menu_runner_.get());
  menu_runner_->Cancel();
}

void ToolkitDelegateViews::UpdateMenuItem(int command_id,
                                          bool enabled,
                                          bool hidden,
                                          const base::string16& title) {
  views::MenuItemView* item = menu_view_->GetMenuItemByID(command_id);
  if (!item)
    return;

  item->SetEnabled(enabled);
  item->SetTitle(title);
  item->SetVisible(!hidden);

  views::MenuItemView* parent = item->GetParentMenuItem();
  if (!parent)
    return;

  parent->ChildrenChanged();
}

#if defined(OS_CHROMEOS)
void ToolkitDelegateViews::UpdateMenuIcon(int command_id,
                                          const gfx::Image& image) {
  views::MenuItemView* item = menu_view_->GetMenuItemByID(command_id);
  if (!item)
    return;

  item->SetIcon(*image.ToImageSkia());

  views::MenuItemView* parent = item->GetParentMenuItem();
  if (!parent)
    return;

  parent->ChildrenChanged();
}
#endif
