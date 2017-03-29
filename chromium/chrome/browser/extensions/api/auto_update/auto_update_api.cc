// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/auto_update/auto_update_api.h"
#if defined(OS_WIN)
#include "third_party/_winsparkle_lib/include/winsparkle.h"
#endif

namespace extensions {

AutoUpdateCheckForUpdatesFunction::~AutoUpdateCheckForUpdatesFunction() {
}

AutoUpdateCheckForUpdatesFunction::AutoUpdateCheckForUpdatesFunction() {
}

bool AutoUpdateCheckForUpdatesFunction::RunAsync() {
  scoped_ptr<api::auto_update::CheckForUpdates::Params> params(
    api::auto_update::CheckForUpdates::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
#if defined(OS_WIN)
  if (params->with_ui)
    win_sparkle_check_update_with_ui();
  else
    win_sparkle_check_update_without_ui();
#elif defined(OS_MACOSX)
  LOG(INFO) << "Sparkle hook";
#endif
  return true;
}

} //namespace extensions
