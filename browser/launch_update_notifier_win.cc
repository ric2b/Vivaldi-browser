// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "browser/launch_update_notifier.h"

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/timer/timer.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/env_vars.h"
#include "chrome/installer/util/util_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_constants.h"
#include "base/vivaldi_switches.h"
#include "browser/vivaldi_runtime_feature.h"
#include "installer/util/vivaldi_install_util.h"
#include "prefs/vivaldi_pref_names.h"
#include "update_notifier/update_notifier_switches.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

namespace vivaldi {

namespace {

constexpr base::TimeDelta kStandaloneCheckPeriod =
#ifdef OFFICIAL_BUILD
    base::Days(1)
#else
    base::Hours(1)
#endif
    ;

void StartUpdateNotifierIfEnabled() {
  // We want to run the notifier with the current flags even if those are
  // different from the command line in the task scheduler entry. This way one
  // can kill the notifier and try with a new value of --vuu.
  base::CommandLine cmdline = ::vivaldi::GetCommonUpdateNotifierCommand();
  cmdline.AppendSwitch(vivaldi_update_notifier::kLaunchIfEnabled);
  cmdline.AppendSwitch(vivaldi_update_notifier::kBrowserStartup);
  LaunchNotifierProcess(cmdline);
}

base::RepeatingTimer& GetStandaloneAutoUpdateTimer() {
  static base::NoDestructor<base::RepeatingTimer> instance;
  return *instance;
}

void LaunchStandaloneAutoUpdateCheck(bool first_time) {
  DLOG(INFO) << "Starting standalone update check";
  base::CommandLine cmdline = ::vivaldi::GetCommonUpdateNotifierCommand();

  // Notify that this an automatic check invoked from the browser.
  cmdline.AppendSwitch(vivaldi_update_notifier::kAutoCheck);
  if (first_time) {
    cmdline.AppendSwitch(vivaldi_update_notifier::kBrowserStartup);
  }
  LaunchNotifierProcess(cmdline);
}

void DoStartStandaloneAutoUpdateCheck() {
  base::RepeatingTimer& timer = GetStandaloneAutoUpdateTimer();
  timer.Stop();
  timer.Start(FROM_HERE, kStandaloneCheckPeriod,
              base::BindRepeating(&LaunchStandaloneAutoUpdateCheck, false));
  LaunchStandaloneAutoUpdateCheck(true);
}

void DoStopStandaloneUpdateCheck() {
  base::RepeatingTimer& timer = GetStandaloneAutoUpdateTimer();
  timer.Stop();

  // Ask a running process if any to quit.
  SendQuitUpdateNotifier(base::FilePath(), /*global=*/false);
}

void StartStandaloneAutoUpdateCheck() {
  content::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&DoStartStandaloneAutoUpdateCheck));
}

}  // namespace

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

  if (IsStandaloneBrowser()) {
    if (IsStandaloneAutoUpdateEnabled()) {
      StartStandaloneAutoUpdateCheck();
    }
    return;
  }

  // Ensure that the old obsolete preference is removed from the profile.
  profile->GetPrefs()->ClearPref(vivaldiprefs::kAutoUpdateEnabled);

  base::ThreadPool::PostTask(FROM_HERE,
                             {base::WithBaseSyncPrimitives(),
                              base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
                             base::BindOnce(&StartUpdateNotifierIfEnabled));
}

void DisableStandaloneAutoUpdate() {
  DCHECK(IsStandaloneBrowser());
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(vivaldiprefs::kVivaldiAutoUpdateStandalone, false);
  content::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&DoStopStandaloneUpdateCheck));
  DLOG(INFO) << "Disabled standalone update notifier";
}

void EnableStandaloneAutoUpdate() {
  DCHECK(IsStandaloneBrowser());
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(vivaldiprefs::kVivaldiAutoUpdateStandalone, true);
  StartStandaloneAutoUpdateCheck();
  DLOG(INFO) << "Enabled standalone update notifier";
}

bool IsStandaloneAutoUpdateEnabled() {
  DCHECK(IsStandaloneBrowser());
  PrefService* prefs = g_browser_process->local_state();

  // Read the real default from the registry. We do it here and not in
  // RegisterLocalState to avoid reading the registry if the user has already
  // set the value.
  const base::Value* value =
      prefs->GetUserPrefValue(vivaldiprefs::kVivaldiAutoUpdateStandalone);
  if (value) {
    DCHECK(value->is_bool());
    return !value->is_bool() || value->GetBool();
  }

  // Cache the registry value to avoid de-synchronization between running the
  // update check and the value reported to settings dialog in case the registry
  // changes.
  static bool registry_enabled = []() -> bool {
    base::win::RegKey key(HKEY_CURRENT_USER, vivaldi::constants::kVivaldiKey,
                          KEY_QUERY_VALUE);
    if (key.Valid()) {
      if (std::optional<bool> bool_value = ReadRegistryBool(
              vivaldi::constants::kVivaldiInstallerDisableStandaloneAutoupdate,
              key)) {
        // The meaning in the registry is reversed.
        return !*bool_value;
      }
    }
    return true;
  }();
  return registry_enabled;
}

}  // namespace vivaldi
