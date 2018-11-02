// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.
//
// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIVALDI_NATIVE_APP_WINDOW_VIEWS_AURA_H_
#define UI_VIVALDI_NATIVE_APP_WINDOW_VIEWS_AURA_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "ui/vivaldi_native_app_window_views.h"

// Aura-specific parts of VivaldiNativeAppWindowViews. This is used directly on
// Linux and Windows.
class VivaldiNativeAppWindowViewsAura : public VivaldiNativeAppWindowViews {
 public:
  VivaldiNativeAppWindowViewsAura();
 ~VivaldiNativeAppWindowViewsAura() override;

 protected:
  ui::WindowShowState GetRestorableState(
      const ui::WindowShowState restore_state) const;

  // ChromeNativeAppWindowViews implementation.
  void OnBeforeWidgetInit(
      const extensions::AppWindow::CreateParams& create_params,
      views::Widget::InitParams* init_params,
      views::Widget* widget) override;
  views::NonClientFrameView* CreateNonStandardAppFrame() override;

  // ui::BaseWindow implementation.
  ui::WindowShowState GetRestoredState() const override;
  bool IsAlwaysOnTop() const override;

  // NativeAppWindow implementation.
  void UpdateShape(std::unique_ptr<ShapeRects> rects) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(VivaldiNativeAppWindowViewsAura);
};

#endif  // UI_VIVALDI_NATIVE_APP_WINDOW_VIEWS_AURA_H_
