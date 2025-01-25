//
// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
//
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Replacement for RenderViewContextMenuMac in chrome. That class will pull
// the entire chain of render_view_context_menu which we do not want because
// of limited support for accelerators and no support for icons.

#ifndef UI_VIVALDI_CONTEXT_MENU_MAC_H_
#define UI_VIVALDI_CONTEXT_MENU_MAC_H_

#import <Cocoa/Cocoa.h>

#include "ui/vivaldi_context_menu.h"

namespace vivaldi {
class VivaldiRenderViewContextMenu;
}
class ToolkitDelegateMac;

@class MenuControllerCocoa;

// Mac implementation of the context menu display code. Uses a Cocoa NSMenu
// to display the context menu. Internally uses an obj-c object as the
// target of the NSMenu, bridging back to this C++ class.
namespace vivaldi {
class VivaldiContextMenuMac : public vivaldi::VivaldiContextMenu {
 public:
  ~VivaldiContextMenuMac() override;
  VivaldiContextMenuMac(content::WebContents* web_contents,
              ui::SimpleMenuModel* menu_model,
              const gfx::Rect& rect,
              vivaldi::VivaldiRenderViewContextMenu* render_view_context_menu);
  VivaldiContextMenuMac(const VivaldiContextMenuMac&) = delete;
  VivaldiContextMenuMac& operator=(const VivaldiContextMenuMac&) = delete;

  void Init(ui::SimpleMenuModel* menu_model,
            base::WeakPtr<ContextMenuPostitionDelegate> delegate) override;
  bool Show() override;
  void SetIcon(const gfx::Image& icon, int id) override;
  bool HasDarkTextColor() override;
  bool IsViews() override { return false; }
  void UpdateItem(int command_id,
                  bool enabled,
                  bool hidden,
                  const std::u16string& title);
 private:
  NSView* GetActiveNativeView();
  // The Cocoa menu controller for this menu.
  MenuControllerCocoa* __strong menu_controller_;
  content::WebContents* web_contents_;
  ui::SimpleMenuModel* menu_model_;
  gfx::Rect rect_;
  NSView* parent_view_ = nullptr;
};
}  // namespace vivaldi

#endif  // UI_VIVALDI_CONTEXT_MENU_MAC_H_
