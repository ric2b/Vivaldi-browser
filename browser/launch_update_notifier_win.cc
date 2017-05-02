// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "browser/launch_update_notifier.h"

#include "app/vivaldi_constants.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string16.h"
#include "base/vivaldi_switches.h"
#include "base/win/win_util.h"
#include "chrome/installer/util/util_constants.h"

namespace vivaldi {
void LaunchUpdateNotifier() {
  base::FilePath update_notifier_path;
  base::PathService::Get(base::DIR_EXE, &update_notifier_path);
  update_notifier_path =
      update_notifier_path.Append(installer::kVivaldiUpdateNotifierExe);
  base::string16 command;
  if (base::win::ReadCommandFromAutoRun(
          HKEY_CURRENT_USER, ::vivaldi::kUpdateNotifierAutorunName, &command) &&
      base::FilePath::CompareEqualIgnoreCase(command,
                                             update_notifier_path.value())) {
    base::CommandLine* vivaldi_command_line =
        base::CommandLine::ForCurrentProcess();

    base::CommandLine update_notifier_command(update_notifier_path);
    if (vivaldi_command_line->HasSwitch(switches::kVivaldiUpdateURL))
      update_notifier_command.AppendSwitchNative(
          switches::kVivaldiUpdateURL,
          vivaldi_command_line->GetSwitchValueNative(
              switches::kVivaldiUpdateURL));

    base::LaunchProcess(update_notifier_command, base::LaunchOptions());
  }
}

}  // namespace vivaldi
