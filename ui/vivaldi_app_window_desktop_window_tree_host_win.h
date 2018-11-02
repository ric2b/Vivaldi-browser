// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.
//
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIVALDI_APP_WINDOW_DESKTOP_WINDOW_TREE_HOST_WIN_H_
#define UI_VIVALDI_APP_WINDOW_DESKTOP_WINDOW_TREE_HOST_WIN_H_

#include "base/macros.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"

namespace views {
class DesktopNativeWidgetAura;
class NativeMenuWin;
}

class VivaldiNativeAppWindowViewsWin;

// AppWindowDesktopWindowTreeHostWin handles updating the glass of app frames on
// Windows. It is used for all desktop app windows on Windows, but is only
// actively doing anything when a glass window frame is being used.
class VivaldiAppWindowDesktopWindowTreeHostWin
    : public views::DesktopWindowTreeHostWin {
 public:
  VivaldiAppWindowDesktopWindowTreeHostWin(
      VivaldiNativeAppWindowViewsWin* app_window,
      views::DesktopNativeWidgetAura* desktop_native_widget_aura);
  ~VivaldiAppWindowDesktopWindowTreeHostWin() override;

 private:
  // Overridden from DesktopWindowTreeHostWin:
  bool GetClientAreaInsets(gfx::Insets* insets) const override;
  void HandleFrameChanged() override;
  void PostHandleMSG(UINT message, WPARAM w_param, LPARAM l_param) override;
  bool PreHandleMSG(UINT message,
                    WPARAM w_param,
                    LPARAM l_param,
                    LRESULT* result) override;

  // Updates the glass frame area by calling the DwmExtendFrameIntoClientArea
  // Windows function.
  void UpdateDWMFrame();

  views::NativeMenuWin* GetSystemMenu();

  VivaldiNativeAppWindowViewsWin* app_window_;

  // The wrapped system menu itself.
  std::unique_ptr<views::NativeMenuWin> system_menu_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiAppWindowDesktopWindowTreeHostWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_WINDOW_DESKTOP_WINDOW_TREE_HOST_WIN_H_
