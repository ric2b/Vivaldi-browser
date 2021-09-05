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

  // WidgetDelegate implementation.
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override;

  // VivaldiNativeAppWindowViews implementation.
  void OnBeforeWidgetInit(views::Widget::InitParams& init_params) override;

  ui::WindowShowState GetRestoredState() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(VivaldiNativeAppWindowViewsAura);
};

#endif  // UI_VIVALDI_NATIVE_APP_WINDOW_VIEWS_AURA_H_
