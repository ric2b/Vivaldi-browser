// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.
//
// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/vivaldi_native_app_window_views_aura.h"

#include <string>
#include <utility>

#include "apps/ui/views/app_window_frame_view.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/apps/app_window_easy_resize_window_targeter.h"
#include "chrome/browser/ui/views/apps/shaped_app_window_targeter.h"
#include "chrome/browser/web_applications/web_app.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/widget/widget.h"
#include "ui/vivaldi_browser_window.h"

#if defined(OS_LINUX)
#include "chrome/browser/shell_integration_linux.h"
#endif

using extensions::AppWindow;

VivaldiNativeAppWindowViewsAura::VivaldiNativeAppWindowViewsAura() {
}

VivaldiNativeAppWindowViewsAura::~VivaldiNativeAppWindowViewsAura() {
}

ui::WindowShowState
VivaldiNativeAppWindowViewsAura::GetRestorableState(
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
    const AppWindow::CreateParams& create_params,
    views::Widget::InitParams* init_params,
    views::Widget* widget) {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  init_params->wm_class_name =
      shell_integration_linux::GetProgramClassName();
  init_params->wm_class_class =
      shell_integration_linux::GetProgramClassClass();
  const char kX11WindowRoleBrowser[] = "browser";
  const char kX11WindowRolePopup[] = "pop-up";
  init_params->wm_role_name =
      window()->type() == VivaldiBrowserWindow::WindowType::SETTINGS
                            ? std::string(kX11WindowRolePopup)
                            : std::string(kX11WindowRoleBrowser);
#endif

  VivaldiNativeAppWindowViews::OnBeforeWidgetInit(create_params, init_params,
                                                  widget);
}

views::NonClientFrameView*
VivaldiNativeAppWindowViewsAura::CreateNonStandardAppFrame() {
  apps::AppWindowFrameView* frame =
      new apps::AppWindowFrameView(widget(), this, HasFrameColor(),
                                   ActiveFrameColor(), InactiveFrameColor());
  frame->Init();

  // Install an easy resize window targeter, which ensures that the root window
  // (not the app) receives mouse events on the edges.
  aura::Window* window = widget()->GetNativeWindow();
  // Add the AppWindowEasyResizeWindowTargeter on the window, not its root
  // window. The root window does not have a delegate, which is needed to
  // handle the event in Linux.
  window->SetEventTargeter(
      std::unique_ptr<ui::EventTargeter>(new AppWindowEasyResizeWindowTargeter(
          window, gfx::Insets(frame->resize_inside_bounds_size()), this)));

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

bool VivaldiNativeAppWindowViewsAura::IsAlwaysOnTop() const {
  return widget()->IsAlwaysOnTop();
}

void VivaldiNativeAppWindowViewsAura::UpdateShape(
    std::unique_ptr<ShapeRects> rects) {
  // Not used in Vivaldi.
}
