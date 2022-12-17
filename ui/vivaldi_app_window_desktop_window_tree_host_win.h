// Copyright (c) 2017-2022 Vivaldi Technologies AS. All rights reserved.
//
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIVALDI_APP_WINDOW_DESKTOP_WINDOW_TREE_HOST_WIN_H_
#define UI_VIVALDI_APP_WINDOW_DESKTOP_WINDOW_TREE_HOST_WIN_H_

#include <shobjidl.h>
#include <wrl/client.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"
#include "ui/views/widget/widget.h"

namespace views {
class DesktopNativeWidgetAura;
class NativeMenuWin;
}  // namespace views

class VivaldiNativeAppWindowViewsWin;
class VivaldiVirtualDesktopHelper;

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
  void Init(const views::Widget::InitParams& params) override;
  void Show(ui::WindowShowState show_state,
            const gfx::Rect& restore_bounds) override;
  std::string GetWorkspace() const override;
  void HandleFrameChanged() override;
  void PostHandleMSG(UINT message, WPARAM w_param, LPARAM l_param) override;
  bool PreHandleMSG(UINT message,
                    WPARAM w_param,
                    LPARAM l_param,
                    LRESULT* result) override;
  int GetInitialShowState() const override;

  void UpdateSystemColorsInPrefs();
  void UpdateWorkspace();

  views::NativeMenuWin* GetSystemMenu();

  raw_ptr<VivaldiNativeAppWindowViewsWin> window_view_;

  // The wrapped system menu itself.
  std::unique_ptr<views::NativeMenuWin> system_menu_;

  // This will be null pre Win10.
  scoped_refptr<VivaldiVirtualDesktopHelper> virtual_desktop_helper_;

  base::WeakPtrFactory<VivaldiAppWindowDesktopWindowTreeHostWin> weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_WINDOW_DESKTOP_WINDOW_TREE_HOST_WIN_H_
