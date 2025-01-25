// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/renderer_context_menu/views/toolkit_delegate_views.h"

#include "browser/menus/vivaldi_context_menu_controller.h"
#include "ui/base/mojom/menu_source_type.mojom-shared.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"

class VivaldiMenuModelAdapterViews : public views::MenuModelAdapter {
 public:
  VivaldiMenuModelAdapterViews(
      ui::MenuModel* menu_model,
      base::WeakPtr<vivaldi::ContextMenuPostitionDelegate> delegate)
      : views::MenuModelAdapter(menu_model), delegate_(delegate) {}

  // views::MenuDelegate
  bool VivaldiShouldTryPositioningContextMenu() const override {
    if (delegate_) {
      return delegate_->CanSetPosition();
    }
    return false;
  }

  void VivaldiGetContextMenuPosition(
      gfx::Rect* menu_bounds,
      const gfx::Rect& monitor_bounds,
      const gfx::Rect& anchor_bounds) const override {
    if (delegate_) {
      delegate_->SetPosition(menu_bounds, monitor_bounds, anchor_bounds);
    }
  }

  void VivaldiExecutePersistent(
      views::MenuItemView* menu_item,
      int event_flags,
      bool* success) override {
    if (delegate_) {
      delegate_->ExecuteIfPersistent(
        menu_item->GetCommand(), event_flags, success);
    } else {
      *success = false;
    }
  }

 private:
  base::WeakPtr<vivaldi::ContextMenuPostitionDelegate> delegate_;
};

views::MenuItemView* ToolkitDelegateViews::VivaldiInit(
    ui::SimpleMenuModel* menu_model,
    base::WeakPtr<vivaldi::ContextMenuPostitionDelegate> delegate) {
  // NOTE(espen): Replicate ToolkitDelegateViews::Init, but without
  // views::MenuRunner::ASYNC. That flag does not work when we want to manage a
  // menu and execute its selected action from an extension. The extension
  // instance will deallocate while the menu is open with ASYNC set but we need
  // that instance alive when sending a reply after the menu closes. The flag
  // was added with Chrome 55.
  menu_adapter_.reset(new VivaldiMenuModelAdapterViews(menu_model, delegate));
  std::unique_ptr<views::MenuItemView> menu_view = menu_adapter_->CreateMenu();
  menu_view_ = menu_view.get();
  menu_runner_= std::make_unique<views::MenuRunner>(
      std::move(menu_view),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);

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
                                            ui::mojom::MenuSourceType type) {
  using Position = views::MenuAnchorPosition;
  Position anchor_position = (type == ui::mojom::MenuSourceType::kTouch ||
                              type == ui::mojom::MenuSourceType::kTouchEditMenu)
                                 ? Position::kBottomCenter
                                 : Position::kTopLeft;
  menu_runner_->RunMenuAt(parent, nullptr, rect, anchor_position, type);
}
