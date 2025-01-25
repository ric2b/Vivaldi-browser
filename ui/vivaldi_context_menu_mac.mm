//
// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
//
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/vivaldi_context_menu_mac.h"

#include "base/compiler_specific.h"
#import "base/mac/scoped_sending_event.h"
#include "base/task/current_thread.h"
#include "browser/menus/vivaldi_render_view_context_menu.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "components/renderer_context_menu/render_view_context_menu_base.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "prefs/native_settings_helper_mac.h"
#import "ui/menus/cocoa/menu_controller.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/vivaldi_context_menu_views.h"

using content::WebContents;

namespace {

// Retrieves an NSMenuItem which has the specified command_id. This function
// traverses the given |model| in the depth-first order. When this function
// finds an item whose command_id is the same as the given |command_id|, it
// returns the NSMenuItem associated with the item. This function emulates
// views::MenuItemViews::GetMenuItemByID() for Mac.
NSMenuItem* VivaldiGetMenuItemByID(ui::MenuModel* model,
                            NSMenu* menu,
                            int command_id) {
  for (size_t i = 0; i < model->GetItemCount(); ++i) {
    NSMenuItem* item = [menu itemAtIndex:i];
    if (model->GetCommandIdAt(i) == command_id)
      return item;

    ui::MenuModel* submenu = model->GetSubmenuModelAt(i);
    if (submenu && [item hasSubmenu]) {
      NSMenuItem* subitem = VivaldiGetMenuItemByID(submenu,
                                            [item submenu],
                                            command_id);
      if (subitem)
        return subitem;
    }
  }
  return nil;
}

class VivaldiToolkitDelegateMac : public RenderViewContextMenu::ToolkitDelegate {
 public:
  explicit VivaldiToolkitDelegateMac(vivaldi::VivaldiContextMenuMac* menu)
      : menu_(menu) {}

  VivaldiToolkitDelegateMac(const VivaldiToolkitDelegateMac&) = delete;
  VivaldiToolkitDelegateMac& operator=(const VivaldiToolkitDelegateMac&) = delete;

  ~VivaldiToolkitDelegateMac() override {}

 private:
  // RenderViewContextMenu::ToolkitDelegate
  void Init(ui::SimpleMenuModel* menu_model) override {}
  void Cancel() override {}
  void UpdateMenuItem(int command_id,
                      bool enabled,
                      bool hidden,
                      const std::u16string& title) override {
    menu_->UpdateItem(command_id, enabled, hidden, title);
  }

  vivaldi::VivaldiContextMenuMac* menu_;
};

}  // namespace

namespace vivaldi {
VivaldiContextMenu* CreateVivaldiContextMenu(
    content::WebContents* web_contents,
    ui::SimpleMenuModel* menu_model,
    const gfx::Rect& rect,
    bool force_views,
    VivaldiRenderViewContextMenu* render_view_context_menu
    ) {
  if (force_views) {
    return new VivaldiContextMenuViews(web_contents, menu_model, rect,
                                       render_view_context_menu);
  } else {
    return new VivaldiContextMenuMac(web_contents, menu_model, rect,
                                     render_view_context_menu);
  }
}

VivaldiContextMenuMac::VivaldiContextMenuMac(
    content::WebContents* web_contents,
    ui::SimpleMenuModel* menu_model,
    const gfx::Rect& rect,
    vivaldi::VivaldiRenderViewContextMenu* render_view_context_menu)
  :web_contents_(web_contents),
   menu_model_(menu_model),
   rect_(rect) {
  if (render_view_context_menu) {
    std::unique_ptr<RenderViewContextMenu::ToolkitDelegate> delegate(
        new VivaldiToolkitDelegateMac(this));
    render_view_context_menu->set_toolkit_delegate(std::move(delegate));
    parent_view_ = render_view_context_menu->parent_view().GetNativeNSView();
  }
}

VivaldiContextMenuMac::~VivaldiContextMenuMac() {
}

void VivaldiContextMenuMac::Init(
    ui::SimpleMenuModel* menu_model,
    base::WeakPtr<ContextMenuPostitionDelegate> delegate) {
  menu_model_ = menu_model;
}

bool VivaldiContextMenuMac::Show() {
  NSView* parent_view = GetActiveNativeView();
  if (!parent_view)
    return false;

  menu_controller_ =
      [[MenuControllerCocoa alloc] initWithModel:menu_model_
                                        delegate:nil
                          useWithPopUpButtonCell:NO];

  // Synthesize an event for the click, as there is no certainty that
  // [NSApp currentEvent] will return a valid event.
  gfx::Point params_position(rect_.bottom_left());
  NSEvent* currentEvent = [NSApp currentEvent];
  NSWindow* window = [parent_view window];
  NSPoint position =
      NSMakePoint(params_position.x(),
                  NSHeight([parent_view bounds]) - params_position.y());
  position = [parent_view convertPoint:position toView:nil];
  NSTimeInterval eventTime = [currentEvent timestamp];
  NSEvent* clickEvent = [NSEvent mouseEventWithType:NSEventTypeRightMouseDown
                                           location:position
                                      modifierFlags:NSEventMaskRightMouseDown
                                          timestamp:eventTime
                                       windowNumber:[window windowNumber]
                                            context:nil
                                        eventNumber:0
                                         clickCount:1
                                           pressure:1.0];

  {
    // Make sure events can be pumped while the menu is up.
    base::CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop allow;

    // One of the events that could be pumped is |window.close()|.
    // User-initiated event-tracking loops protect against this by
    // setting flags in -[CrApplication sendEvent:], but since
    // web-content menus are initiated by IPC message the setup has to
    // be done manually.
    base::mac::ScopedSendingEvent sendingEventScoper;

    // Show the menu.
    [NSMenu popUpContextMenu:[menu_controller_ menu]
                   withEvent:clickEvent
                     forView:parent_view];
  }
  return true;
}

void VivaldiContextMenuMac::SetIcon(const gfx::Image& icon, int id) {
  NSMenuItem* item = VivaldiGetMenuItemByID(menu_model_,
                                            [menu_controller_ menu],
                                            id);
  item.image = icon.ToNSImage();
}

void VivaldiContextMenuMac::UpdateItem(int id,
                                       bool enabled,
                                       bool hidden,
                                       const std::u16string& title) {
  NSMenuItem* item = VivaldiGetMenuItemByID(menu_model_,
                                            [menu_controller_ menu],
                                            id);
  if (!item)
    return;
  [item setEnabled:enabled];
  [item setTitle:base::SysUTF16ToNSString(title)];
  [item setHidden:hidden];
  [[item menu] itemChanged:item];
}

NSView* VivaldiContextMenuMac::GetActiveNativeView() {
  // parent_view_ is set for document context menus.
  if (parent_view_) {
    return parent_view_;
  } else {
    return web_contents_->GetNativeView().GetNativeNSView();
  }
}

bool VivaldiContextMenuMac::HasDarkTextColor() {
  return vivaldi::getSystemDarkMode() == 0;
}

}  // namespace vivialdi
