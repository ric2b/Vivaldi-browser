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
#include "third_party/_winsparkle_lib/include/winsparkle.h"

namespace {
const int kQuitAllUpdateNotifiersEventInterval = 1;  // s

base::FilePath GetUpdateNotifierPath() {
  base::FilePath exe_dir;
  base::PathService::Get(base::DIR_EXE, &exe_dir);
  return exe_dir.Append(installer::kVivaldiUpdateNotifierExe);
}
}  // Anonymous namespace

namespace extensions {

bool AutoUpdateCheckForUpdatesFunction::RunAsync() {
  std::unique_ptr<vivaldi::auto_update::CheckForUpdates::Params> params(
      vivaldi::auto_update::CheckForUpdates::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  if (params->with_ui)
    win_sparkle_check_update_with_ui();
  else
    win_sparkle_check_update_without_ui();
  SendResponse(true);
  return true;
}

bool AutoUpdateIsUpdateNotifierEnabledFunction::RunAsync() {
  base::string16 command;
  bool result =
      base::win::ReadCommandFromAutoRun(
          HKEY_CURRENT_USER, ::vivaldi::kUpdateNotifierAutorunName, &command) &&
      base::FilePath::CompareEqualIgnoreCase(command,
                                             GetUpdateNotifierPath().value());
  results_ =
      vivaldi::auto_update::IsUpdateNotifierEnabled::Results::Create(result);
  SendResponse(true);
  return true;
}

bool AutoUpdateEnableUpdateNotifierFunction::RunAsync() {
  if (!base::win::AddCommandToAutoRun(HKEY_CURRENT_USER,
                                      ::vivaldi::kUpdateNotifierAutorunName,
                                      GetUpdateNotifierPath().value())) {
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

  // Best effort attempt to stop any running notifier for the current user.
  base::win::ScopedHandle quit_all_event_handle;
  quit_all_event_handle.Set(OpenEvent(
      EVENT_MODIFY_STATE, FALSE, ::vivaldi::kQuitAllUpdateNotifiersEventName));
  if (quit_all_event_handle.IsValid()) {
    quit_all_update_notifiers_event_.reset(
        new base::WaitableEvent(std::move(quit_all_event_handle)));

    quit_all_update_notifiers_event_->Signal();
    base::MessageLoop::current()->task_runner()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&AutoUpdateDisableUpdateNotifierFunction::
                       FinishClosingUpdateNotifiers,
                   this),
        base::TimeDelta::FromSeconds(kQuitAllUpdateNotifiersEventInterval));
  }

  SendResponse(true);
  return true;
}

void AutoUpdateDisableUpdateNotifierFunction::FinishClosingUpdateNotifiers() {
  DCHECK(quit_all_update_notifiers_event_.get());
  quit_all_update_notifiers_event_->Reset();
  quit_all_update_notifiers_event_.reset();
}

}  // namespace extensions
