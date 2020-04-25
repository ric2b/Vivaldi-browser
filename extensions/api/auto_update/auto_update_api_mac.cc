// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/auto_update/auto_update_api.h"

namespace extensions {

ExtensionFunction::ResponseAction AutoUpdateCheckForUpdatesFunction::Run() {
  using vivaldi::auto_update::CheckForUpdates::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  LOG(INFO) << "Sparkle hook";
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AutoUpdateIsUpdateNotifierEnabledFunction::Run() {
  return RespondNow(Error("Not implemented"));
}

ExtensionFunction::ResponseAction
AutoUpdateEnableUpdateNotifierFunction::Run() {
  return RespondNow(Error("Not implemented"));
}

ExtensionFunction::ResponseAction
AutoUpdateDisableUpdateNotifierFunction::Run() {
  return RespondNow(Error("Not implemented"));
}

}  // namespace extensions
