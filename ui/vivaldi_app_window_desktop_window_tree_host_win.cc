// Copyright (c) 2017-2021 Vivaldi Technologies AS. All rights reserved.
//
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/vivaldi_app_window_desktop_window_tree_host_win.h"

#include <dwmapi.h>

#include "base/win/windows_version.h"
#include "chrome/browser/ui/views/frame/system_menu_insertion_delegate_win.h"
#include "ui/base/theme_provider.h"
#include "ui/views/controls/menu/native_menu_win.h"

#include "app/vivaldi_apptools.h"
#include "ui/vivaldi_native_app_window_views_win.h"

VivaldiAppWindowDesktopWindowTreeHostWin::
    VivaldiAppWindowDesktopWindowTreeHostWin(
        VivaldiNativeAppWindowViewsWin* window_view,
        views::DesktopNativeWidgetAura* desktop_native_widget_aura)
    : DesktopWindowTreeHostWin(window_view->widget(),
                               desktop_native_widget_aura),
      window_view_(window_view) {}

VivaldiAppWindowDesktopWindowTreeHostWin::
    ~VivaldiAppWindowDesktopWindowTreeHostWin() {}

void VivaldiAppWindowDesktopWindowTreeHostWin::HandleFrameChanged() {
  window_view_->OnCanHaveAlphaEnabledChanged();
  DesktopWindowTreeHostWin::HandleFrameChanged();
}

views::NativeMenuWin*
VivaldiAppWindowDesktopWindowTreeHostWin::GetSystemMenu() {
  if (!system_menu_.get()) {
    SystemMenuInsertionDelegateWin insertion_delegate;
    system_menu_.reset(new views::NativeMenuWin(
        window_view_->GetSystemMenuModel(), GetHWND()));
    system_menu_->Rebuild(&insertion_delegate);
  }
  return system_menu_.get();
}

bool VivaldiAppWindowDesktopWindowTreeHostWin::PreHandleMSG(UINT message,
                                                            WPARAM w_param,
                                                            LPARAM l_param,
                                                            LRESULT* result) {
  switch (message) {
    case WM_INITMENUPOPUP:
      GetSystemMenu()->UpdateStates();
      return true;
  }
  return DesktopWindowTreeHostWin::PreHandleMSG(message, w_param, l_param,
                                                result);
}

void VivaldiAppWindowDesktopWindowTreeHostWin::PostHandleMSG(UINT message,
                                                             WPARAM w_param,
                                                             LPARAM l_param) {
  switch (message) {
    case WM_DWMCOLORIZATIONCOLORCHANGED: {
      vivaldi::GetSystemColorsUpdatedCallbackList().Notify();
      break;
    }
  }
  return DesktopWindowTreeHostWin::PostHandleMSG(message, w_param, l_param);
}
