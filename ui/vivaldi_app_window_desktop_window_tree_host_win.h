// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.
//
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIVALDI_APP_WINDOW_DESKTOP_WINDOW_TREE_HOST_WIN_H_
#define UI_VIVALDI_APP_WINDOW_DESKTOP_WINDOW_TREE_HOST_WIN_H_

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"

namespace views {
class DesktopNativeWidgetAura;
class NativeMenuWin;
}  // namespace views

class VivaldiNativeAppWindowViewsWin;

// A sublass to ensure system menu initialization.
class VivaldiAppWindowDesktopWindowTreeHostWin
    : public views::DesktopWindowTreeHostWin {
 public:
  VivaldiAppWindowDesktopWindowTreeHostWin(
      VivaldiNativeAppWindowViewsWin* window_view,
      views::DesktopNativeWidgetAura* desktop_native_widget_aura);
  ~VivaldiAppWindowDesktopWindowTreeHostWin() override;
  VivaldiAppWindowDesktopWindowTreeHostWin(
      const VivaldiAppWindowDesktopWindowTreeHostWin&) = delete;
  VivaldiAppWindowDesktopWindowTreeHostWin& operator=(
      const VivaldiAppWindowDesktopWindowTreeHostWin&) = delete;

 private:
  // Overridden from DesktopWindowTreeHostWin:
  void HandleFrameChanged() override;
  void PostHandleMSG(UINT message, WPARAM w_param, LPARAM l_param) override;
  bool PreHandleMSG(UINT message,
                    WPARAM w_param,
                    LPARAM l_param,
                    LRESULT* result) override;

  void UpdateSystemColorsInPrefs();

  views::NativeMenuWin* GetSystemMenu();

  VivaldiNativeAppWindowViewsWin* window_view_;

  // The wrapped system menu itself.
  std::unique_ptr<views::NativeMenuWin> system_menu_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_WINDOW_DESKTOP_WINDOW_TREE_HOST_WIN_H_
