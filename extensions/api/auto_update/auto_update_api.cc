// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/auto_update/auto_update_api.h"
#if defined(OS_WIN)
#include "third_party/_winsparkle_lib/include/winsparkle.h"
#endif

namespace extensions {

AutoUpdateCheckForUpdatesFunction::~AutoUpdateCheckForUpdatesFunction() {
}

AutoUpdateCheckForUpdatesFunction::AutoUpdateCheckForUpdatesFunction() {
}

bool AutoUpdateCheckForUpdatesFunction::RunAsync() {
  std::unique_ptr<vivaldi::auto_update::CheckForUpdates::Params> params(
    vivaldi::auto_update::CheckForUpdates::Params::Create(*args_));
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
