// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.
//
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIVALDI_APP_WINDOW_CLIENT_H_
#define UI_VIVALDI_APP_WINDOW_CLIENT_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "extensions/browser/app_window/app_window_client.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

class VivaldiBrowserWindow;
class VivaldiNativeAppWindow;

class VivaldiAppWindowClient {
 public:
  VivaldiAppWindowClient();
  ~VivaldiAppWindowClient();

  // Get the LazyInstance for VivaldiAppWindowClient.
  static VivaldiAppWindowClient* GetInstance();

  // Return the app window client.
  static VivaldiAppWindowClient* Get();

  // Initialize the app window system with this app window client.
  static void Set(VivaldiAppWindowClient* client);

  VivaldiNativeAppWindow* CreateNativeAppWindow(
    VivaldiBrowserWindow* window,
    extensions::AppWindow::CreateParams* params);

private:
  friend struct base::DefaultSingletonTraits<VivaldiAppWindowClient>;

  // Implemented in platform specific code.
  static VivaldiNativeAppWindow* CreateNativeAppWindowImpl(
      VivaldiBrowserWindow* window,
      const extensions::AppWindow::CreateParams& params);

  DISALLOW_COPY_AND_ASSIGN(VivaldiAppWindowClient);
};

#endif  // UI_VIVALDI_APP_WINDOW_CLIENT_H_
