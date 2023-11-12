// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/renderer_context_menu/views/toolkit_delegate_views.h"

#include "browser/menus/vivaldi_context_menu_controller.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"

class VivaldiMenuModelAdapterViews : public views::MenuModelAdapter {
 public:
  VivaldiMenuModelAdapterViews(ui::MenuModel* menu_model,
                               vivaldi::ContextMenuPostitionDelegate* delegate)
      : views::MenuModelAdapter(menu_model), delegate_(delegate) {}

  // views::MenuDelegate
  bool VivaldiShouldTryPositioningContextMenu() const override {
    return delegate_ != nullptr;
  }

  void VivaldiGetContextMenuPosition(
      gfx::Rect* menu_bounds,
      const gfx::Rect& monitor_bounds,
      const gfx::Rect& anchor_bounds) const override {
    delegate_->SetPosition(menu_bounds, monitor_bounds, anchor_bounds);
  }

 private:
  const raw_ptr<vivaldi::ContextMenuPostitionDelegate> delegate_;
};

views::MenuItemView* ToolkitDelegateViews::VivaldiInit(
    ui::SimpleMenuModel* menu_model,
    vivaldi::ContextMenuPostitionDelegate* delegate) {
  // NOTE(espen): Replicate ToolkitDelegateViews::Init, but without
  // views::MenuRunner::ASYNC. That flag does not work when we want to manage a
  // menu and execute its selected action from an extension. The extension
  // instance will deallocate while the menu is open with ASYNC set but we need
  // that instance alive when sending a reply after the menu closes. The flag
  // was added with Chrome 55.
  menu_adapter_.reset(new VivaldiMenuModelAdapterViews(menu_model, delegate));
  menu_view_ = menu_adapter_->CreateMenu();
  menu_runner_.reset(new views::MenuRunner(
      menu_view_,
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU));

  // Middle mouse button allows opening bookmarks in background.
  menu_adapter_->set_triggerable_event_flags(
      menu_adapter_->triggerable_event_flags() | ui::EF_MIDDLE_MOUSE_BUTTON);
  return menu_view_;
}

void ToolkitDelegateViews::VivaldiUpdateMenu(views::MenuItemView* view,
                                             ui::SimpleMenuModel* menu_model) {
  menu_adapter_->VivaldiUpdateMenu(view, menu_model);
}

void ToolkitDelegateViews::VivaldiSetMenu(views::MenuItemView* view,
                                          ui::MenuModel* menu_model) {
  menu_adapter_->VivaldiSetModel(view, menu_model);
}

void ToolkitDelegateViews::VivaldiRunMenuAt(views::Widget* parent,
                                            const gfx::Rect& rect,
                                            ui::MenuSourceType type) {
  using Position = views::MenuAnchorPosition;
  Position anchor_position =
      (type == ui::MENU_SOURCE_TOUCH || type == ui::MENU_SOURCE_TOUCH_EDIT_MENU)
          ? Position::kBottomCenter
          : Position::kTopLeft;
  menu_runner_->RunMenuAt(parent, nullptr, rect, anchor_position, type);
}
