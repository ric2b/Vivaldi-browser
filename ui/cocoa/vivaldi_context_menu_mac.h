//
// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
//
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Replacement for RenderViewContextMenuMac in chrome. That class will pull
// the entire chain of render_view_context_menu which we do not want because
// of limited support for accelerators and no support for icons.

#ifndef UI_COCOA_VIVALDI_CONTEXT_MENU_MAC_H_
#define UI_COCOA_VIVALDI_CONTEXT_MENU_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "chromium/content/public/common/context_menu_params.h"
#include "ui/vivaldi_context_menu.h"

class ToolkitDelegateMac;

@class MenuControllerCocoa;

// Mac implementation of the context menu display code. Uses a Cocoa NSMenu
// to display the context menu. Internally uses an obj-c object as the
// target of the NSMenu, bridging back to this C++ class.
class VivaldiContextMenuMac : public vivaldi::VivaldiContextMenu {
 public:
  ~VivaldiContextMenuMac() override;
  VivaldiContextMenuMac(content::WebContents* web_contents,
                        ui::SimpleMenuModel* menu_model,
                        const content::ContextMenuParams& params);
  void Show() override;
  void SetIcon(const gfx::Image& icon, int id) override;

 private:
  NSView* GetActiveNativeView();
  // The Cocoa menu controller for this menu.
  base::scoped_nsobject<MenuControllerCocoa> menu_controller_;
  content::WebContents* web_contents_;
  ui::SimpleMenuModel* menu_model_;
  content::ContextMenuParams params_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiContextMenuMac);
};

#endif  // UI_COCOA_VIVALDI_CONTEXT_MENU_MAC_H_
