// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.
//
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIVALDI_APP_WINDOW_DESKTOP_NATIVE_WIDGET_AURA_WIN_H_
#define UI_VIVALDI_APP_WINDOW_DESKTOP_NATIVE_WIDGET_AURA_WIN_H_

#include "base/macros.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"

class VivaldiNativeAppWindowViewsWin;

namespace views {
class DesktopWindowTreeHost;
}

// AppWindowDesktopNativeWidgetAura is a DesktopNativeWidgetAura subclass that
// handles creating the right type of tree hosts for app windows on Windows.
class VivaldiAppWindowDesktopNativeWidgetAuraWin
    : public views::DesktopNativeWidgetAura {
 public:
  explicit VivaldiAppWindowDesktopNativeWidgetAuraWin(
      VivaldiNativeAppWindowViewsWin* app_window);

 protected:
  ~VivaldiAppWindowDesktopNativeWidgetAuraWin() override;

  // Overridden from views::DesktopNativeWidgetAura:
  void InitNativeWidget(const views::Widget::InitParams& params) override;
  void Maximize() override;
  void Minimize() override;

 private:
  // Ownership managed by the views system.
  VivaldiNativeAppWindowViewsWin* app_window_;

  // Owned by superclass DesktopNativeWidgetAura.
  views::DesktopWindowTreeHost* tree_host_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiAppWindowDesktopNativeWidgetAuraWin);
};

#endif  // UI_VIVALDI_APP_WINDOW_DESKTOP_NATIVE_WIDGET_AURA_WIN_H_
