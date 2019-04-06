// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.
//
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/vivaldi_app_window_client.h"

#include "ui/vivaldi_native_app_window_views_aura.h"

// static
VivaldiNativeAppWindow* VivaldiAppWindowClient::CreateNativeAppWindowImpl(
    VivaldiBrowserWindow* app_window,
    const extensions::AppWindow::CreateParams& params) {
  VivaldiNativeAppWindowViewsAura* window = new VivaldiNativeAppWindowViewsAura;
  window->Init(app_window, params);
  return window;
}
