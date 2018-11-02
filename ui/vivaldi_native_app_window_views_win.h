// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.
//
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIVALDI_NATIVE_APP_WINDOW_VIEWS_WIN_H_
#define UI_VIVALDI_NATIVE_APP_WINDOW_VIEWS_WIN_H_

#include "base/macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/vivaldi_native_app_window_views_aura.h"

namespace web_app {
struct ShortcutInfo;
}

namespace ui {
class MenuModel;
}

class GlassAppWindowFrameViewWin;
class VivaldiSystemMenuModelBuilder;

// Windows-specific parts of the views-backed native shell window implementation
// for packaged apps.
class VivaldiNativeAppWindowViewsWin : public VivaldiNativeAppWindowViewsAura {
 public:
  VivaldiNativeAppWindowViewsWin();
  ~VivaldiNativeAppWindowViewsWin() override;

  GlassAppWindowFrameViewWin* glass_frame_view() {
    return glass_frame_view_;
  }

  ui::MenuModel* GetSystemMenuModel();

  // On Windows, we serve an icon adjusted for hidpi.
  gfx::ImageSkia GetWindowIcon() override;
  gfx::ImageSkia GetWindowAppIcon() override;

 protected:
   gfx::Insets GetFrameInsets() const override;

 private:
  void OnShortcutInfoLoaded(
      const web_app::ShortcutInfo& shortcut_info);

  HWND GetNativeAppWindowHWND() const;
  void EnsureCaptionStyleSet();

  // Overridden from ChromeNativeAppWindowViews:
  void OnBeforeWidgetInit(
      const extensions::AppWindow::CreateParams& create_params,
      views::Widget::InitParams* init_params,
      views::Widget* widget) override;
  void InitializeDefaultWindow(
      const extensions::AppWindow::CreateParams& create_params) override;
  views::NonClientFrameView* CreateStandardDesktopAppFrame() override;

  // Overridden from views::WidgetDelegate:
  bool CanMinimize() const override;
  bool ExecuteWindowsCommand(int command_id) override;

  void UpdateEventTargeterWithInset() override;

  int GetCommandIDForAppCommandID(int app_command_id) const;

  // Populated if there is a glass frame, which provides special information
  // to the native widget implementation. This will be NULL if there is no
  // glass frame. Note, this can change from NULL to non-NULL and back again
  // throughout the life of a window, e.g. if DWM is enabled and disabled.
  GlassAppWindowFrameViewWin* glass_frame_view_;

  // The Windows Application User Model ID identifying the app.
  base::string16 app_model_id_;

  // Whether the InitParams indicated that this window should be translucent.
  bool is_translucent_;

  std::unique_ptr<VivaldiSystemMenuModelBuilder> menu_model_builder_;

  base::WeakPtrFactory<VivaldiNativeAppWindowViewsWin> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiNativeAppWindowViewsWin);
};

#endif  // UI_VIVALDI_NATIVE_APP_WINDOW_VIEWS_WIN_H_
