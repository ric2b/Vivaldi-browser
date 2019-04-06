// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "browser/launch_update_notifier.h"

#include "app/vivaldi_constants.h"
#include "app/vivaldi_apptools.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string16.h"
#include "base/vivaldi_switches.h"
#include "base/win/win_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/env_vars.h"
#include "chrome/installer/util/util_constants.h"
#include "components/prefs/pref_service.h"
#include "prefs/vivaldi_pref_names.h"

#include "installer/util/vivaldi_install_util.h"

namespace vivaldi {

void LaunchUpdateNotifier(Profile* profile) {
  // Don't launch the update notifier if we are not running as Vivaldi.
  if (!vivaldi::IsVivaldiRunning())
    return;

  base::CommandLine* vivaldi_command_line =
      base::CommandLine::ForCurrentProcess();

#if defined(COMPONENT_BUILD)
  // For component (local) builds, the '--launch-updater' switch must be
  // present to launch the updater.
  if (!vivaldi_command_line->HasSwitch(switches::kLaunchUpdater))
    return;
#endif

  std::unique_ptr<base::Environment> env(base::Environment::Create());
  // For non-interactive tests we don't launch the update notifier.
  if (env->HasVar(env_vars::kHeadless))
    return;

  base::FilePath update_notifier_path;
  base::PathService::Get(base::DIR_EXE, &update_notifier_path);
  update_notifier_path =
      update_notifier_path.Append(
          vivaldi::constants::kVivaldiUpdateNotifierExe);

  base::string16 command;
  base::string16 notifier_path_string(
      L"\"" + update_notifier_path.value() + L"\"");

  base::CommandLine update_notifier_command(update_notifier_path);
  if (vivaldi_command_line->HasSwitch(switches::kVivaldiUpdateURL))
    update_notifier_command.AppendSwitchNative(
      switches::kVivaldiUpdateURL,
      vivaldi_command_line->GetSwitchValueNative(
        switches::kVivaldiUpdateURL));

  bool do_launch = base::win::ReadCommandFromAutoRun(
      HKEY_CURRENT_USER, ::vivaldi::kUpdateNotifierAutorunName, &command) &&
      (base::FilePath::CompareEqualIgnoreCase(command,
          notifier_path_string) ||
      base::FilePath::CompareEqualIgnoreCase(command,
          update_notifier_path.value()));

  // If the auto-run command is not set, check the obsolete auto update pref.
  // This hack fixes VB-30912.
  if (!do_launch) {
    PrefService* prefs = profile->GetPrefs();
    do_launch = prefs->GetBoolean(vivaldiprefs::kAutoUpdateEnabled);
    if (do_launch) {
      // Make sure we don't consider the pref on next launch.
      prefs->SetBoolean(vivaldiprefs::kAutoUpdateEnabled, false);
      // If pref was enabled, enable auto-run.
      base::win::AddCommandToAutoRun(HKEY_CURRENT_USER,
        ::vivaldi::kUpdateNotifierAutorunName,
        notifier_path_string);
    }
  }

  if (do_launch)
    base::LaunchProcess(update_notifier_command, base::LaunchOptions());
}

}  // namespace vivaldi
