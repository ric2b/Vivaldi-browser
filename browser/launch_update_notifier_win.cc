// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "browser/launch_update_notifier.h"

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/win/win_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/env_vars.h"
#include "chrome/installer/util/util_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_switches.h"

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_constants.h"
#include "base/vivaldi_switches.h"
#include "extensions/api/features/vivaldi_runtime_feature.h"
#include "installer/util/vivaldi_install_util.h"
#include "prefs/vivaldi_pref_names.h"
#include "update_notifier/update_notifier_switches.h"

namespace vivaldi {

void LaunchUpdateNotifier(Profile* profile) {
  // Don't launch the update notifier if we are not running as Vivaldi.
  if (!vivaldi::IsVivaldiRunning())
    return;

#if defined(COMPONENT_BUILD)
  // For component (local) builds, the '--launch-updater' switch must be
  // present to launch the updater.
  base::CommandLine* vivaldi_command_line =
      base::CommandLine::ForCurrentProcess();
  if (!vivaldi_command_line->HasSwitch(switches::kLaunchUpdater))
    return;
#endif

  std::unique_ptr<base::Environment> env(base::Environment::Create());
  // For non-interactive tests we don't launch the update notifier.
  if (env->HasVar(env_vars::kHeadless))
    return;

  bool enabled = IsUpdateNotifierEnabled(nullptr);
  if (!enabled) {
    // If the auto-run command is not set, check the obsolete auto update pref.
    // This hack fixes VB-30912.
    PrefService* prefs = profile->GetPrefs();
    if (prefs->GetBoolean(vivaldiprefs::kAutoUpdateEnabled)) {
      // Make sure we don't consider the pref on next launch.
      prefs->SetBoolean(vivaldiprefs::kAutoUpdateEnabled, false);
      EnableUpdateNotifier(GetCommonUpdateNotifierCommand(profile));
      enabled = true;
    }
  }
  if (enabled) {
    // We want to run the notifier with the current flags even if those are
    // different from the autostart registry. This way one can kill the notifier
    // and try with a new value of --vuu.
    LaunchNotifierProcess(GetCommonUpdateNotifierCommand(profile));
  }
}

base::CommandLine GetCommonUpdateNotifierCommand(Profile* profile) {
  base::CommandLine command(GetUpdateNotifierPath(nullptr));
  base::CommandLine* vivaldi_cmd_line = base::CommandLine::ForCurrentProcess();
  if (vivaldi_cmd_line->HasSwitch(switches::kVivaldiUpdateURL)) {
    command.AppendSwitchNative(
        switches::kVivaldiUpdateURL,
        vivaldi_cmd_line->GetSwitchValueNative(switches::kVivaldiUpdateURL));
  }
  if (vivaldi_cmd_line->HasSwitch(switches::kEnableLogging)) {
    // We do not copy the value here as we do not want to log to the browser log
    // file but rather always to the separated install log.
    command.AppendSwitch(vivaldi_update_notifier::kEnableLogging);
  }
  if (vivaldi_cmd_line->HasSwitch(switches::kVivaldiSilentUpdate)) {
    command.AppendSwitch(switches::kVivaldiSilentUpdate);
  }
  return command;
}

}  // namespace vivaldi
