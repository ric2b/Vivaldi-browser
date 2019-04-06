// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/auto_update/auto_update_api.h"

#include <Windows.h>

#include "app/vivaldi_constants.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/vivaldi_switches.h"
#include "base/win/win_util.h"
#include "chrome/installer/util/util_constants.h"
#include "installer/util/vivaldi_install_util.h"
#include "update_notifier/update_notifier_switches.h"

namespace {

base::FilePath GetUpdateNotifierPath() {
  base::FilePath exe_dir;
  base::PathService::Get(base::DIR_EXE, &exe_dir);
  return exe_dir.Append(vivaldi::constants::kVivaldiUpdateNotifierExe);
}
}  // Anonymous namespace

namespace extensions {

bool AutoUpdateCheckForUpdatesFunction::RunAsync() {
  std::unique_ptr<vivaldi::auto_update::CheckForUpdates::Params> params(
      vivaldi::auto_update::CheckForUpdates::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  base::CommandLine update_notifier_command(GetUpdateNotifierPath());
  update_notifier_command.AppendSwitch(::vivaldi_update_notifier::kCheckForUpdates);

  base::CommandLine* vivaldi_cmd_line = base::CommandLine::ForCurrentProcess();

  if (vivaldi_cmd_line->HasSwitch(switches::kVivaldiUpdateURL)) {
    update_notifier_command.AppendSwitchNative(
        switches::kVivaldiUpdateURL,
        vivaldi_cmd_line->GetSwitchValueNative(switches::kVivaldiUpdateURL));
  }

  base::LaunchProcess(update_notifier_command, base::LaunchOptions());

  SendResponse(true);
  return true;
}

bool AutoUpdateIsUpdateNotifierEnabledFunction::RunAsync() {
  base::string16 command;
  base::string16 notifier_path_string(
      L"\"" + GetUpdateNotifierPath().value() + L"\"");
  bool result =
      base::win::ReadCommandFromAutoRun(
        HKEY_CURRENT_USER, ::vivaldi::kUpdateNotifierAutorunName, &command) &&
        (base::FilePath::CompareEqualIgnoreCase(command,
            notifier_path_string) ||
         base::FilePath::CompareEqualIgnoreCase(command,
            GetUpdateNotifierPath().value()));
  results_ =
      vivaldi::auto_update::IsUpdateNotifierEnabled::Results::Create(result);
  SendResponse(true);
  return true;
}

bool AutoUpdateEnableUpdateNotifierFunction::RunAsync() {
  base::string16 command(L"\"" + GetUpdateNotifierPath().value() + L"\"");
  if (!base::win::AddCommandToAutoRun(HKEY_CURRENT_USER,
                                      ::vivaldi::kUpdateNotifierAutorunName,
                                      command)) {
    SendResponse(false);
    return true;
  }

  base::CommandLine* vivaldi_command_line =
      base::CommandLine::ForCurrentProcess();

  base::CommandLine update_notifier_command(GetUpdateNotifierPath());
  if (vivaldi_command_line->HasSwitch(switches::kVivaldiUpdateURL))
    update_notifier_command.AppendSwitchNative(
        switches::kVivaldiUpdateURL, vivaldi_command_line->GetSwitchValueNative(
                                         switches::kVivaldiUpdateURL));
  base::LaunchProcess(update_notifier_command, base::LaunchOptions());

  SendResponse(true);
  return true;
}

AutoUpdateDisableUpdateNotifierFunction::
    AutoUpdateDisableUpdateNotifierFunction() {}
AutoUpdateDisableUpdateNotifierFunction::
    ~AutoUpdateDisableUpdateNotifierFunction() {}

bool AutoUpdateDisableUpdateNotifierFunction::RunAsync() {
  if (!base::win::RemoveCommandFromAutoRun(
          HKEY_CURRENT_USER, ::vivaldi::kUpdateNotifierAutorunName)) {
    SendResponse(false);
    return true;
  }
  // Run the update_notifier with the --q switch to terminate any
  // running instance launched from the same filepath as ourselves.
  base::CommandLine update_notifier_command(GetUpdateNotifierPath());
  update_notifier_command.AppendSwitch(::vivaldi_update_notifier::kQuit);
  base::LaunchProcess(update_notifier_command, base::LaunchOptions());

  SendResponse(true);
  return true;
}

}  // namespace extensions
