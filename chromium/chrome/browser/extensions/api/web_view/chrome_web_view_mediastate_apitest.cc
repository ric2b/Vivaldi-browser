// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

namespace extensions {

// Testing 'webview.onMediaStateChanged.'
// tomas@vivaldi.com - disabled test (VB-7468)
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_WebviewMediastate) {
  ASSERT_TRUE(RunPlatformAppTest("webview/mediastate_event")) << message_;
}

}  // namespace extensions
