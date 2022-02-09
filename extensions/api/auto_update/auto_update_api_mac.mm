// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/auto_update/auto_update_api.h"

#import <Foundation/Foundation.h>
#include "app/vivaldi_constants.h"
#include "base/mac/foundation_util.h"
#import "chrome/browser/app_controller_mac.h"

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

ExtensionFunction::ResponseAction
AutoUpdateInstallUpdateAndRestartFunction::Run() {
  AppController* controller =
      base::mac::ObjCCastStrict<AppController>([NSApp delegate]);
  [controller installUpdateAndRestart];
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AutoUpdateGetAutoInstallUpdatesFunction::Run() {
  namespace Results = vivaldi::auto_update::IsUpdateNotifierEnabled::Results;
  using ::vivaldi::kSparkleAutoInstallSettingName;

  NSString* key =
      [NSString stringWithUTF8String:kSparkleAutoInstallSettingName];
  bool result = [[NSUserDefaults standardUserDefaults] boolForKey:key];

  return RespondNow(ArgumentList(Results::Create(result)));
}

ExtensionFunction::ResponseAction
AutoUpdateSetAutoInstallUpdatesFunction::Run() {
  using vivaldi::auto_update::SetAutoInstallUpdates::Params;
  using ::vivaldi::kSparkleAutoInstallSettingName;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());
  NSString* key =
      [NSString stringWithUTF8String:kSparkleAutoInstallSettingName];
  [[NSUserDefaults standardUserDefaults] setBool:params->autoinstall
                                          forKey:key];

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction AutoUpdateGetUpdateStatusFunction::Run() {
  AppController* controller =
    base::mac::ObjCCastStrict<AppController>([NSApp delegate]);
  absl::optional<AutoUpdateStatus> status = [controller getUpdateStatus];
  std::string version_string = [controller getUpdateVersion];
  std::string release_notes_url = [controller getUpdateReleaseNotesUrl];

  SendResult(status, std::move(version_string), std::move(release_notes_url));
  return AlreadyResponded();
}

bool AutoUpdateHasAutoUpdatesFunction::HasAutoUpdates() {
  // Auto updates are always supported on Mac
  return true;
}

}  // namespace extensions
