// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.
//
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/vivaldi_app_window_desktop_native_widget_aura_win.h"

#include "ui/vivaldi_app_window_desktop_window_tree_host_win.h"
#include "ui/vivaldi_native_app_window_views_win.h"
#include "ui/aura/window.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host.h"

VivaldiAppWindowDesktopNativeWidgetAuraWin::VivaldiAppWindowDesktopNativeWidgetAuraWin(
    VivaldiNativeAppWindowViewsWin* app_window)
    : views::DesktopNativeWidgetAura(app_window->widget()),
      app_window_(app_window) {
  GetNativeWindow()->SetName("AppWindowAura");
}

VivaldiAppWindowDesktopNativeWidgetAuraWin::
    ~VivaldiAppWindowDesktopNativeWidgetAuraWin() {}

void VivaldiAppWindowDesktopNativeWidgetAuraWin::InitNativeWidget(
    const views::Widget::InitParams& params) {
  views::Widget::InitParams modified_params = params;
  tree_host_ = new VivaldiAppWindowDesktopWindowTreeHostWin(app_window_, this);
  modified_params.desktop_window_tree_host = tree_host_;
  DesktopNativeWidgetAura::InitNativeWidget(modified_params);
}

void VivaldiAppWindowDesktopNativeWidgetAuraWin::Maximize() {
  // Maximizing on Windows causes the window to be shown. Call Show() first to
  // ensure the content view is also made visible. See http://crbug.com/436867.
  // TODO(jackhou): Make this behavior the same as other platforms, i.e. calling
  // Maximize() does not also show the window.
  if (!tree_host_->IsVisible())
    DesktopNativeWidgetAura::Show();
  DesktopNativeWidgetAura::Maximize();
}

void VivaldiAppWindowDesktopNativeWidgetAuraWin::Minimize() {
  // Minimizing on Windows causes the window to be shown. Call Show() first to
  // ensure the content view is also made visible. See http://crbug.com/436867.
  // TODO(jackhou): Make this behavior the same as other platforms, i.e. calling
  // Minimize() does not also show the window.
  if (!tree_host_->IsVisible())
    DesktopNativeWidgetAura::Show();
  DesktopNativeWidgetAura::Minimize();
}
