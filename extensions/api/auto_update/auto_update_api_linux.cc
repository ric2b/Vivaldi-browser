// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/auto_update/auto_update_api.h"

namespace extensions {

ExtensionFunction::ResponseAction AutoUpdateCheckForUpdatesFunction::Run() {
  using vivaldi::auto_update::CheckForUpdates::Params;

  absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  return RespondNow(Error("Not implemented"));
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

ExtensionFunction::ResponseAction
AutoUpdateInstallUpdateAndRestartFunction::Run() {
  return RespondNow(Error("Not implemented"));
}

ExtensionFunction::ResponseAction
AutoUpdateGetAutoInstallUpdatesFunction::Run() {
  return RespondNow(Error("Not implemented"));
}

ExtensionFunction::ResponseAction
AutoUpdateSetAutoInstallUpdatesFunction::Run() {
  return RespondNow(Error("Not implemented"));
}

ExtensionFunction::ResponseAction AutoUpdateGetLastCheckTimeFunction::Run() {
  return RespondNow(Error("Not implemented"));
}

ExtensionFunction::ResponseAction AutoUpdateGetUpdateStatusFunction::Run() {
  return RespondNow(Error("Not implemented"));
}

bool AutoUpdateHasAutoUpdatesFunction::HasAutoUpdates() {
  return false;
}

}  // namespace extensions
