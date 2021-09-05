// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.
//
// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/vivaldi_native_app_window_views_aura.h"

#include <string>
#include <utility>

#include "base/macros.h"
#include "build/build_config.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/vivaldi_window_frame_view_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/vivaldi_browser_window.h"
#include "ui/wm/core/easy_resize_window_targeter.h"

#if defined(OS_LINUX)
#include "chrome/browser/shell_integration_linux.h"
#endif

namespace {
// The class is copied from app_window_easy_resize_window_targeter.cc
// in chromium/chrome/browser/ui/views/apps.
// An EasyResizeEventTargeter whose behavior depends on the state of the app
// window.
class VivaldiWindowEasyResizeWindowTargeter
    : public wm::EasyResizeWindowTargeter {
 public:
  VivaldiWindowEasyResizeWindowTargeter(const gfx::Insets& insets,
                                        VivaldiBrowserWindow* window)
      : wm::EasyResizeWindowTargeter(insets, insets),
        window_(window) {}

  ~VivaldiWindowEasyResizeWindowTargeter() override = default;

 protected:
  // aura::WindowTargeter:
  bool GetHitTestRects(aura::Window* window,
                       gfx::Rect* rect_mouse,
                       gfx::Rect* rect_touch) const override {
    // EasyResizeWindowTargeter intercepts events landing at the edges of the
    // window. Since maximized and fullscreen windows can't be resized anyway,
    // skip EasyResizeWindowTargeter so that the web contents receive all mouse
    // events.
    if (window_->IsMaximized() || window_->IsFullscreen())
      return WindowTargeter::GetHitTestRects(window, rect_mouse, rect_touch);

    return EasyResizeWindowTargeter::GetHitTestRects(window, rect_mouse,
                                                     rect_touch);
  }

 private:
  VivaldiBrowserWindow* window_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiWindowEasyResizeWindowTargeter);
};

}  // namespace

VivaldiNativeAppWindowViewsAura::VivaldiNativeAppWindowViewsAura() {}

VivaldiNativeAppWindowViewsAura::~VivaldiNativeAppWindowViewsAura() {}

ui::WindowShowState VivaldiNativeAppWindowViewsAura::GetRestorableState(
    const ui::WindowShowState restore_state) const {
  // Whitelist states to return so that invalid and transient states
  // are not saved and used to restore windows when they are recreated.
  switch (restore_state) {
    case ui::SHOW_STATE_NORMAL:
    case ui::SHOW_STATE_MAXIMIZED:
    case ui::SHOW_STATE_FULLSCREEN:
      return restore_state;

    case ui::SHOW_STATE_DEFAULT:
    case ui::SHOW_STATE_MINIMIZED:
    case ui::SHOW_STATE_INACTIVE:
    case ui::SHOW_STATE_END:
      return ui::SHOW_STATE_NORMAL;
  }

  return ui::SHOW_STATE_NORMAL;
}

void VivaldiNativeAppWindowViewsAura::OnBeforeWidgetInit(
    views::Widget::InitParams& init_params) {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  init_params.wm_class_name = shell_integration_linux::GetProgramClassName();
  init_params.wm_class_class = shell_integration_linux::GetProgramClassClass();
  const char kX11WindowRoleBrowser[] = "browser";
  const char kX11WindowRolePopup[] = "pop-up";
  init_params.wm_role_name =
      window()->type() == VivaldiBrowserWindow::WindowType::SETTINGS
          ? std::string(kX11WindowRolePopup)
          : std::string(kX11WindowRoleBrowser);
#endif
}

std::unique_ptr<views::NonClientFrameView>
VivaldiNativeAppWindowViewsAura::CreateNonClientFrameView(
      views::Widget* widget) {
  if (!is_frameless())
    return views::WidgetDelegateView::CreateNonClientFrameView(widget);

  std::unique_ptr <VivaldiWindowFrameViewAura> frame =
      std::make_unique<VivaldiWindowFrameViewAura>(this);

  // Install an easy resize window targeter, which ensures that the root window
  // (not the app) receives mouse events on the edges.
  aura::Window* native_window = widget->GetNativeWindow();
  // Add the VivaldiWindowEasyResizeWindowTargeter on the window, not its root
  // window. The root window does not have a delegate, which is needed to
  // handle the event in Linux.
  native_window->SetEventTargeter(
      std::make_unique<VivaldiWindowEasyResizeWindowTargeter>(
          gfx::Insets(frame->resize_inside_bounds_size()), window()));

  return frame;
}

ui::WindowShowState VivaldiNativeAppWindowViewsAura::GetRestoredState() const {
  // First normal states are checked.
  if (IsFullscreen())
    return ui::SHOW_STATE_FULLSCREEN;
  if (IsMaximized())
    return ui::SHOW_STATE_MAXIMIZED;

  // Use kPreMinimizedShowStateKey in case a window is minimized/hidden.
  ui::WindowShowState restore_state = widget()->GetNativeWindow()->GetProperty(
      aura::client::kPreMinimizedShowStateKey);
  return GetRestorableState(restore_state);
}
