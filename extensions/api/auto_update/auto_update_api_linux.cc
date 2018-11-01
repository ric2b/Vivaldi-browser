// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/auto_update/auto_update_api.h"

namespace extensions {

bool AutoUpdateCheckForUpdatesFunction::RunAsync() {
  std::unique_ptr<vivaldi::auto_update::CheckForUpdates::Params> params(
      vivaldi::auto_update::CheckForUpdates::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  SendResponse(false);
  return true;
}

bool AutoUpdateIsUpdateNotifierEnabledFunction::RunAsync() {
  SendResponse(false);
  return true;
}

bool AutoUpdateEnableUpdateNotifierFunction::RunAsync() {
  SendResponse(false);
  return true;
}

AutoUpdateDisableUpdateNotifierFunction::
    AutoUpdateDisableUpdateNotifierFunction() {}
AutoUpdateDisableUpdateNotifierFunction::
    ~AutoUpdateDisableUpdateNotifierFunction() {}

bool AutoUpdateDisableUpdateNotifierFunction::RunAsync() {
  SendResponse(false);
  return true;
}

}  // namespace extensions
