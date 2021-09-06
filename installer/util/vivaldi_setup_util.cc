// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#include "installer/util/vivaldi_setup_util.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_util_win.h"
#include "base/strings/stringprintf.h"
#include "base/win/registry.h"
#include "chrome/installer/setup/install_params.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/util/initial_preferences.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/logging_installer.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/work_item_list.h"

#include "base/vivaldi_switches.h"
#include "components/version_info/version_info_values.h"
#include "installer/util/marker_file_work_item.h"
#include "installer/util/vivaldi_install_dialog.h"
#include "installer/util/vivaldi_install_util.h"
#include "installer/util/vivaldi_progress_dialog.h"
#include "update_notifier/update_notifier_switches.h"

#include <Windows.h>

#include <CommCtrl.h>

#pragma comment(lib, "comctl32.lib")

namespace vivaldi {

#if !defined(OFFICIAL_BUILD)
base::FilePath* debug_subprocesses_exe = nullptr;
#endif

namespace {

bool g_start_browser_after_install = false;
bool g_silent_install = false;

installer::VivaldiProgressDialog* g_vivaldi_progress_dialog = nullptr;

// Enable the SE_DEBUG privilege when running elevated, which allows us to
// obtain tokens for processes of other users so we can kill all browser
// and update_notifier instances.
void EnableDebugPrivileges() {
  DCHECK(installer::kVivaldi);
  HANDLE process_token_handle;
  if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES,
                         &process_token_handle) == FALSE)
    return;
  base::win::ScopedHandle process_token(process_token_handle);

  LUID locally_unique_id;

  if (::LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &locally_unique_id) ==
      FALSE)
    return;

  TOKEN_PRIVILEGES token_privileges;
  token_privileges.PrivilegeCount = 1;
  token_privileges.Privileges[0].Luid = locally_unique_id;
  token_privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

  ::AdjustTokenPrivileges(process_token.Get(), FALSE, &token_privileges, 0,
                          nullptr, nullptr);
}

bool TryToCloseAllRunningBrowsers(
    const installer::InstallerState& installer_state) {
  base::FilePath vivaldi_exe_path =
      installer_state.target_path().Append(installer::kChromeExe);
  if (!base::PathExists(vivaldi_exe_path))
    return true;
  if (!base::NormalizeFilePath(vivaldi_exe_path, &vivaldi_exe_path)) {
    PLOG(ERROR) << "Failed to normalize " << vivaldi_exe_path;
  }
  std::vector<base::win::ScopedHandle> vivaldi_processes(
      GetRunningProcessesForPath(vivaldi_exe_path));
  if (vivaldi_processes.empty())
    return true;
  KillProcesses(std::move(vivaldi_processes));

  const int MAX_WAIT_SECS = 10;
  for (int wait = MAX_WAIT_SECS * 10; wait > 0; --wait) {
    Sleep(100);
    vivaldi_processes = GetRunningProcessesForPath(vivaldi_exe_path);
    if (vivaldi_processes.empty())
      break;
  }

  while (!vivaldi_processes.empty()) {
    int choice = MessageBox(
        nullptr,
        L"Vivaldi is still running.\n"
        L"Please close all Vivaldi windows before continuing install.",
        L"Vivaldi Installer", MB_RETRYCANCEL | MB_ICONEXCLAMATION);
    if (choice == IDCANCEL) {
      VLOG(1) << "Vivaldi: install cancelled due to running instances.";
      return false;
    }
    vivaldi_processes = vivaldi::GetRunningProcessesForPath(vivaldi_exe_path);
  }

  return true;
}

// TODO(igor@vivaldi.com): Move this to vivaldi_install_utils and share this
// with the update notifier.
void UpdateDeltaPatchStatus(bool successful) {
  DCHECK(installer::kVivaldi);
  std::wstring key_name(vivaldi::constants::kVivaldiKey);
  key_name.append(L"\\");
  key_name.append(vivaldi::constants::kVivaldiAutoUpdate);
  base::win::RegKey key(HKEY_CURRENT_USER, key_name.c_str(), KEY_ALL_ACCESS);
  if (key.Valid()) {
    key.WriteValue(vivaldi::constants::kVivaldiDeltaPatchFailed,
                   (successful) ? L"0" : L"1");
  }
}

// Return the installation directory if setup_exe_dir is a part of an
// installation.
base::FilePath SetupExeDirToInstallDir(const base::FilePath& setup_exe_dir) {
  // installer_dir is InstallationDirectory/Application/version/Installer,
  // verify the structure. We do not verify the version to support various
  // debugging setups when the version in executable does not match the
  // installation version.
  if (!base::FilePath::CompareEqualIgnoreCase(setup_exe_dir.BaseName().value(),
                                              installer::kInstallerDir)) {
    return base::FilePath();
  }
  base::FilePath vivaldi_exe_dir = setup_exe_dir.DirName().DirName();
  if (!base::FilePath::CompareEqualIgnoreCase(
          vivaldi_exe_dir.BaseName().value(), installer::kInstallBinaryDir)) {
    return base::FilePath();
  }

  // Check that vivaldi.exe exists
  if (!base::PathExists(vivaldi_exe_dir.Append(installer::kChromeExe)))
    return base::FilePath();

  return vivaldi_exe_dir.DirName();
}

}  // namespace

bool PrepareSetupConfig(HINSTANCE instance) {
  DCHECK(installer::kVivaldi);

  // Chromium initializes logging using a global const instance of
  // InitialPreferences that reflects the command line. But we need to alter the
  // command line before that instance is initialized yet we want to log errors
  // here. So we use a temporary preferences instance that parses the initial
  // command line and pass that to the logging to reflect the logging settings.
  // Then Chromium will initialize the global instance in setup_main.cc from the
  // patched command line after we return.
  installer::InitialPreferences tmp_prefs_for_logging;
  installer::InitInstallerLogging(tmp_prefs_for_logging);

  base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();

  // Add an empty line between log entries from different invocations of
  // setup.exe for convenience.
  VLOG(1) << "Initial command line:\n\n" << cmd_line.GetCommandLineString();

  g_silent_install = cmd_line.HasSwitch(vivaldi::constants::kVivaldiSilent);
  bool is_update = cmd_line.HasSwitch(vivaldi::constants::kVivaldiUpdate);
  g_start_browser_after_install =
      !cmd_line.HasSwitch(installer::switches::kDoNotLaunchChrome);
  bool is_silent_update = cmd_line.HasSwitch(switches::kVivaldiSilentUpdate);
  if (is_silent_update) {
    g_silent_install = true;
    g_start_browser_after_install = false;
  }
  bool is_from_mini = cmd_line.HasSwitch(vivaldi::constants::kVivaldiMini);
  if (is_from_mini) {
    // Do not propagate the switch to other invocations like the invocation with
    // administrative privileges for system installs.
    cmd_line.RemoveSwitch(vivaldi::constants::kVivaldiMini);
  }

  installer::VivaldiInstallUIOptions options;

  options.install_dir =
      cmd_line.GetSwitchValuePath(vivaldi::constants::kVivaldiInstallDir);
  if (options.install_dir.empty() && !is_from_mini) {
    // Check if setup.exe is a part of an existing installation. If so, default
    // to that directory. With is_from_mini we know that we are not a part.
    base::FilePath setup_exe_dir = GetDirectoryOfCurrentExe();
    if (setup_exe_dir.empty())
      return false;
#if !defined(OFFICIAL_BUILD)
    if (cmd_line.HasSwitch(vivaldi::constants::kVivaldiDebugTargetExe)) {
      setup_exe_dir =
          cmd_line
              .GetSwitchValuePath(vivaldi::constants::kVivaldiDebugTargetExe)
              .DirName();
    }
#endif
    options.install_dir = SetupExeDirToInstallDir(setup_exe_dir);
  }

  if (cmd_line.HasSwitch(installer::switches::kSystemLevel)) {
    options.install_type = vivaldi::InstallType::kForAllUsers;
    options.given_install_type = true;
  } else if (cmd_line.HasSwitch(vivaldi::constants::kVivaldiStandalone)) {
    options.install_type = vivaldi::InstallType::kStandalone;
    options.given_install_type = true;
  }

  if (is_update) {
    if (options.install_dir.empty()) {
      LOG(ERROR) << "Vivaldi update requires --"
                 << vivaldi::constants::kVivaldiInstallDir << " option";
      return false;
    }
  }

  if (!is_update && is_from_mini) {
    // We are called from the mini installer after the decompression and this is
    // not an update. Show Vivaldi UI to customize options or use defaults for
    // silent installs.
    DCHECK(!options.register_browser);
    if (cmd_line.HasSwitch(installer::switches::kMakeChromeDefault) ||
        cmd_line.HasSwitch(vivaldi::constants::kVivaldiRegisterStandalone)) {
      // See comments for VivaldiInstallUIOptions::register_browser.
      options.register_browser = true;
      options.given_register_browser = true;
    }
    if (g_silent_install) {
      if (options.install_dir.empty()) {
        if (options.install_type == vivaldi::InstallType::kStandalone) {
          LOG(ERROR) << "Vivaldi silent standalone install requires --"
                     << vivaldi::constants::kVivaldiInstallDir << " option";
          return false;
        }
        options.install_dir =
            vivaldi::GetDefaultInstallTopDir(options.install_type);
        if (options.install_dir.empty())
          return false;
      }
    } else {
      installer::VivaldiInstallDialog dlg(instance, std::move(options));

      const installer::VivaldiInstallDialog::DlgResult dlg_result =
          dlg.ShowModal();
      if (dlg_result != installer::VivaldiInstallDialog::INSTALL_DLG_INSTALL) {
        VLOG(1) << "Vivaldi: install cancelled/failed.";
        return false;
      }

      options = dlg.ExtractOptions();
    }
  }

  // For an existing installation ignore any attempt to change the installation
  // type.
  base::Optional<vivaldi::InstallType> existing_install_type =
      vivaldi::FindInstallType(options.install_dir);
  if (existing_install_type) {
    if (!is_update) {
      is_update = true;
      cmd_line.AppendSwitch(vivaldi::constants::kVivaldiUpdate);
    }
    if (*existing_install_type != options.install_type) {
      LOG(WARNING) << "Replacing the user-selected installation type "
                   << static_cast<int>(options.install_type)
                   << " with the type of existing installation "
                   << static_cast<int>(*existing_install_type);
      // An existing type unconditionally overrides any options.
      options.install_type = *existing_install_type;
    }
  }

  // Sync switches with the final configuration as we query them in few places
  // throught out the installer and to let Chromium settings code pick the right
  // values.

  if (options.register_browser) {
    if (options.install_type == InstallType::kStandalone) {
      cmd_line.AppendSwitch(vivaldi::constants::kVivaldiRegisterStandalone);
    }
    if (ShellUtil::CanMakeChromeDefaultUnattended()) {
      cmd_line.AppendSwitch(installer::switches::kMakeChromeDefault);
    }
  } else {
    cmd_line.RemoveSwitch(vivaldi::constants::kVivaldiRegisterStandalone);
    cmd_line.RemoveSwitch(installer::switches::kMakeChromeDefault);
  }

  if (!options.install_dir.empty()) {
    cmd_line.AppendSwitchPath(vivaldi::constants::kVivaldiInstallDir,
                              options.install_dir);
  }

  switch (options.install_type) {
    case vivaldi::InstallType::kForCurrentUser:
      cmd_line.RemoveSwitch(installer::switches::kSystemLevel);
      cmd_line.RemoveSwitch(vivaldi::constants::kVivaldiStandalone);
      VLOG(1) << "Vivaldi: install for current user - install_dir="
              << options.install_dir.value();
      break;
    case vivaldi::InstallType::kForAllUsers:
      cmd_line.AppendSwitch(installer::switches::kSystemLevel);
      cmd_line.RemoveSwitch(vivaldi::constants::kVivaldiStandalone);
      VLOG(1)
          << "Vivaldi: install for all users (system install) - install_dir="
          << options.install_dir.value();
      break;
    case vivaldi::InstallType::kStandalone:
      cmd_line.RemoveSwitch(installer::switches::kSystemLevel);
      cmd_line.AppendSwitch(vivaldi::constants::kVivaldiStandalone);
      VLOG(1) << "Vivaldi: standalone install - install dir="
              << options.install_dir.value();
      break;
  }

  return true;
}

bool BeginInstallOrUninstall(HINSTANCE instance,
                             const installer::InstallerState& installer_state) {
  if (installer_state.system_install()) {
    EnableDebugPrivileges();
  }
  if (installer_state.operation() == installer::InstallerState::UNINSTALL)
    return true;

  DCHECK(installer_state.operation() ==
         installer::InstallerState::SINGLE_INSTALL_OR_UPDATE);
  if (!IsInstallSilentUpdate()) {
    if (!TryToCloseAllRunningBrowsers(installer_state))
      return false;
  }
  if (!g_silent_install) {
    g_vivaldi_progress_dialog = new installer::VivaldiProgressDialog(instance);
    g_vivaldi_progress_dialog->ShowModeless();
  }
  return true;
}

bool PrepareRegistration(const installer::InstallerState& installer_state) {
  // NOTE(jarle@vivaldi.com):
  // If standalone install and we should not register ourselves, return now.
  if (IsInstallStandalone() && !IsInstallRegisterStandalone()) {
    return false;
  }

  return true;
}

void EndInstallOrUninstall(const installer::InstallerState& installer_state,
                           installer::InstallStatus install_status) {
  if (g_vivaldi_progress_dialog) {
    int return_code = InstallUtil::GetInstallReturnCode(install_status);
    g_vivaldi_progress_dialog->FinishProgress(return_code == 0 ? 1000 : 0);
    delete g_vivaldi_progress_dialog;
    g_vivaldi_progress_dialog = nullptr;
  }
}

namespace {

void RestartUpdateNotifier(const installer::InstallerState& installer_state) {
  base::FilePath exe_dir =
      NormalizeInstallExeDirectory(installer_state.target_path());

  if (IsInstallUpdate()) {
    // At this point the running update notifier was renamed to the old name.
    QuitAllUpdateNotifiers(exe_dir, /*quit_old=*/true);
  }

  bool new_install = !IsInstallUpdate();

  if (IsInstallStandalone()) {
    // An update check for a standalone install is always run by the browser.
    // Check if we have older an autorun entry and remove it.
    if (IsUpdateNotifierEnabled(&exe_dir)) {
      DisableUpdateNotifier(&exe_dir);
    }
    return;
  }

  if (vivaldi_update_notifier::kUseTaskScheduler) {
    base::CommandLine update_notifier_command =
        GetCommonUpdateNotifierCommand(&exe_dir);
    if (new_install) {
      update_notifier_command.AppendSwitch(vivaldi_update_notifier::kEnable);
    } else {
      update_notifier_command.AppendSwitch(
          vivaldi_update_notifier::kEnableIfUpgrade);
    }

    // We must not use LaunchNotifierProcess() as the installer may be a
    // privileged process and we must start a normal process here.
    ShellExecuteFromExplorer(update_notifier_command.GetProgram(),
                             update_notifier_command.GetArgumentsString(),
                             exe_dir);
  } else {
    if (new_install && !IsUpdateNotifierEnabledForAnyPath()) {
      EnableUpdateNotifier(GetCommonUpdateNotifierCommand(&exe_dir));
    }
  }
}

}  // namespace

void FinalizeSuccessfullInstall(
    const installer::InstallerState& installer_state,
    installer::InstallStatus install_status) {
  DCHECK(installer::kVivaldi);
  // See comments in RunInstaller in updatedownloader.cc why we have to do this
  // even for full installs.
  UpdateDeltaPatchStatus(true);

  if (IsInstallStandalone()) {
    // TODO(jarle@vivaldi.com): REMOVE THIS:
    // rename the "Profile" folder to "User Data" for standalone builds if
    // the "Profile" folder exists
    std::wstring::size_type pos =
        std::wstring(installer_state.target_path().value())
            .rfind(L"\\Application");
    if (pos != std::wstring::npos) {
      std::wstring base =
          std::wstring(installer_state.target_path().value()).substr(0, pos);
      base::FilePath old_profile_dir(base);
      old_profile_dir = old_profile_dir.AppendASCII("Profile");
      base::FilePath new_user_data_dir(base);
      new_user_data_dir = new_user_data_dir.AppendASCII("User Data");
      if (base::DirectoryExists(old_profile_dir)) {
        if (!::MoveFileEx(old_profile_dir.value().c_str(),
                          new_user_data_dir.value().c_str(),
                          MOVEFILE_WRITE_THROUGH)) {
          DWORD error = ::GetLastError();
          LOG(WARNING) << "Failed to rename old Profile folder to User "
                          "Data. Error="
                       << error;
          base::string16 message(base::StringPrintf(
              L"Failed to rename 'Profile' folder. Error=%u", error));
          MessageBox(NULL, message.c_str(), L"Vivaldi Installer",
                     MB_OK | MB_ICONWARNING);
        } else {
          // relax for a sec to be 100% sure that the rename has been
          // flushed to disk ...
          Sleep(1000);
        }
      }
    }
  }

  RestartUpdateNotifier(installer_state);
  base::DeleteFile(installer_state.target_path().Append(
      vivaldi::constants::kVivaldiUpdateNotifierOldExe));

  if (g_start_browser_after_install) {
    base::FilePath vivaldi_path =
        installer_state.target_path().Append(installer::kChromeExe);

    // We need to use the custom ShellExecuteFromExplorer to avoid
    // launching vivaldi.exe with elevated privileges.
    // The setup.exe process could be elevated.
    VLOG(1) << "Launching: " << vivaldi_path.value()
            << ", is_standalone() = " << IsInstallStandalone()
            << ", install_status = " << static_cast<int>(install_status);
    vivaldi::ShellExecuteFromExplorer(vivaldi_path, base::string16(),
                                      base::FilePath());
  }
}

void AddVivaldiSpecificWorkItems(const installer::InstallParams& install_params,
                                 WorkItemList* install_list) {
  if (!installer::kVivaldi)
    return;

  const installer::InstallerState& installer_state =
      install_params.installer_state;
  const base::FilePath& src_path = install_params.src_path;
  const base::FilePath& temp_path = install_params.temp_path;
  const base::FilePath& target_path = installer_state.target_path();

  base::FilePath update_notifier(
      target_path.Append(vivaldi::constants::kVivaldiUpdateNotifierExe));
  base::FilePath old_update_notifier(
      target_path.Append(vivaldi::constants::kVivaldiUpdateNotifierOldExe));

  // Delete any update_notifier.old if present
  install_list->AddDeleteTreeWorkItem(old_update_notifier, temp_path);

  // Rename the currently running update_notifier.exe to update_notifier.old
  // (ignore failure if it doesn't exist)
  install_list
      ->AddMoveTreeWorkItem(update_notifier, old_update_notifier, temp_path,
                            WorkItem::ALWAYS_MOVE)
      ->set_best_effort(true);

  // Install the new update_notifier.exe
  install_list->AddCopyTreeWorkItem(
      src_path.Append(vivaldi::constants::kVivaldiUpdateNotifierExe),
      update_notifier, temp_path, WorkItem::CopyOverWriteOption::ALWAYS);

  // Mark standalone or system installs.
  if (IsInstallStandalone()) {
    base::FilePath standalone_marker =
        target_path.Append(vivaldi::constants::kStandaloneMarkerFile);
    install_list->AddWorkItem(new vivaldi::MarkerFileWorkItem(
        std::move(standalone_marker), "// Vivaldi Standalone\n"));
  } else if (installer_state.system_install()) {
    base::FilePath system_marker =
        target_path.Append(vivaldi::constants::kSystemMarkerFile);
    install_list->AddWorkItem(new vivaldi::MarkerFileWorkItem(
        std::move(system_marker), "// Vivaldi System Install\n"));
  }
}

void UnregisterUpdateNotifier(
    const installer::InstallerState& installer_state) {
  base::CommandLine update_notifier_command =
      ::vivaldi::GetCommonUpdateNotifierCommand(&installer_state.target_path());
  update_notifier_command.AppendSwitch(vivaldi_update_notifier::kUnregister);
  int exit_code = ::vivaldi::RunNotifierSubaction(update_notifier_command);
  if (exit_code != 0) {
    LOG(ERROR) << "Failed to unregister the update notifier, exit_code="
               << exit_code;
  }
}

void ShowInstallerResultMessage(int string_resource_id) {
  base::string16 msg = installer::GetLocalizedString(string_resource_id);
  LOG(ERROR) << msg;
  if (!g_silent_install) {
    ::MessageBox(nullptr, base::as_wcstr(msg.c_str()), nullptr,
                 MB_ICONINFORMATION | MB_SETFOREGROUND);
  }
}

bool IsInstallUpdate() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      vivaldi::constants::kVivaldiUpdate);
}

bool IsInstallStandalone() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      vivaldi::constants::kVivaldiStandalone);
}

bool IsInstallRegisterStandalone() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      vivaldi::constants::kVivaldiRegisterStandalone);
}

bool IsInstallSilentUpdate() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kVivaldiSilentUpdate);
}

}  // namespace vivaldi
