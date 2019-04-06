// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.
//
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/vivaldi_app_window_client.h"

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "components/version_info/version_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature_channel.h"
#include "ui/vivaldi_browser_window.h"

// TODO(jamescook): We probably shouldn't compile this class at all on Android.
// See http://crbug.com/343612
#if !defined(OS_ANDROID)
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#endif

class VivaldiNativeAppWindow;

VivaldiAppWindowClient::VivaldiAppWindowClient() {
}

VivaldiAppWindowClient::~VivaldiAppWindowClient() {
}

// static
VivaldiAppWindowClient* VivaldiAppWindowClient::GetInstance() {
  return base::Singleton<
    VivaldiAppWindowClient,
      base::LeakySingletonTraits<VivaldiAppWindowClient>>::get();
}

VivaldiNativeAppWindow* VivaldiAppWindowClient::CreateNativeAppWindow(
    VivaldiBrowserWindow* window,
    extensions::AppWindow::CreateParams* params) {
#if defined(OS_ANDROID)
  return NULL;
#else
  return CreateNativeAppWindowImpl(window, *params);
#endif
}

namespace {
VivaldiAppWindowClient* g_client = NULL;

}  // namespace

VivaldiAppWindowClient* VivaldiAppWindowClient::Get() {
  return g_client;
}

void VivaldiAppWindowClient::Set(VivaldiAppWindowClient* client) {
  // This can happen in unit tests, where the utility thread runs in-process.
  if (g_client)
    return;

  g_client = client;
}
