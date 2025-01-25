// Copyright (c) 2017-2022 Vivaldi Technologies AS. All rights reserved.
//
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIVALDI_DESKTOP_WINDOW_TREE_HOST_WIN_H_
#define UI_VIEWS_VIVALDI_DESKTOP_WINDOW_TREE_HOST_WIN_H_

#include <shobjidl.h>
#include <wrl/client.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/color/win/accent_color_observer.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"
#include "ui/views/widget/widget.h"

// Set as window borders, on Win11 and higher if accent-color is not enabled.
#define VIVALDI_WINDOW_BORDER_DARK RGB(0x37, 0x37, 0x37)
#define VIVALDI_WINDOW_BORDER_LIGHT RGB(0xAA, 0xAA, 0xAA)

namespace views {
class DesktopNativeWidgetAura;
class NativeMenuWin;
}  // namespace views

class VivaldiBrowserWindow;
class VivaldiSystemMenuModelBuilder;
class VivaldiVirtualDesktopHelper;

// A sublass to ensure system menu initialization.
class VivaldiDesktopWindowTreeHostWin : public views::DesktopWindowTreeHostWin {
 public:
  VivaldiDesktopWindowTreeHostWin(
      VivaldiBrowserWindow* window,
      views::DesktopNativeWidgetAura* desktop_native_widget_aura);
  ~VivaldiDesktopWindowTreeHostWin() override;
  VivaldiDesktopWindowTreeHostWin(const VivaldiDesktopWindowTreeHostWin&) =
      delete;
  VivaldiDesktopWindowTreeHostWin& operator=(
      const VivaldiDesktopWindowTreeHostWin&) = delete;

 private:
  // Overridden from DesktopWindowTreeHostWin:
  void Init(const views::Widget::InitParams& params) override;
  void Show(ui::mojom::WindowShowState show_state,
            const gfx::Rect& restore_bounds) override;
  std::string GetWorkspace() const override;
  void HandleFrameChanged() override;
  void PostHandleMSG(UINT message, WPARAM w_param, LPARAM l_param) override;
  bool PreHandleMSG(UINT message,
                    WPARAM w_param,
                    LPARAM l_param,
                    LRESULT* result) override;
  int GetInitialShowState() const override;
  bool ShouldUseNativeFrame() const override;
  bool HasFrame() const override;
  views::FrameMode GetFrameMode() const override;
  void HandleCreate() override;
  void Restore() override;
  void Maximize() override;
  void Minimize() override;
  void SetFullscreen(bool fullscreen, int64_t target_display_id) override;
  bool GetDwmFrameInsetsInPixels(gfx::Insets* insets) const override;
  bool GetClientAreaInsets(gfx::Insets* insets,
                           HMONITOR monitor) const override;

  void SetRoundedWindowCorners(bool enable);
  void SetWindowAccentColor(COLORREF bordercolor);
  // Will read the system setting for accent border color and use this if
  // enabled. If this we will fallback to two predefined colors for dark and
  // light modes.
  void UpdateWindowBorderColor(bool is_inactive,
                               bool check_global_accent = false);
  void UpdateWorkspace();
  views::NativeMenuWin* GetSystemMenu();
  void OnPrefsChanged(const std::string& path);
  void OnAccentColorUpdated();

  base::CallbackListSubscription subscription_ =
      ui::AccentColorObserver::Get()->Subscribe(base::BindRepeating(
          &VivaldiDesktopWindowTreeHostWin::OnAccentColorUpdated,
          base::Unretained(this)));

  raw_ptr<VivaldiBrowserWindow> window_;

  std::unique_ptr<VivaldiSystemMenuModelBuilder> menu_model_builder_;

  COLORREF window_border_color_;
  // If the system has border accent set. This will override our own colors.
  bool has_accent_set_ = false;
  // The wrapped system menu itself.
  std::unique_ptr<views::NativeMenuWin> system_menu_;
  std::unique_ptr<base::win::RegKey> dwm_key_;

  // This will be null pre Win10.
  scoped_refptr<VivaldiVirtualDesktopHelper> virtual_desktop_helper_;

  PrefChangeRegistrar prefs_registrar_;

  base::WeakPtrFactory<VivaldiDesktopWindowTreeHostWin> weak_factory_{this};
};

#endif  // UI_VIEWS_VIVALDI_DESKTOP_WINDOW_TREE_HOST_WIN_H_
