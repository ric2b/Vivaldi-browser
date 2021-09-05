// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#include "installer/util/vivaldi_setup_util.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/win/registry.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/util/install_util.h"

#include "base/vivaldi_switches.h"
#include "installer/util/vivaldi_install_dialog.h"
#include "installer/util/vivaldi_install_util.h"
#include "installer/util/vivaldi_progress_dialog.h"

#include <Windows.h>

#include <CommCtrl.h>

namespace vivaldi {

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

}  // namespace

bool PrepareSetupConfig(HINSTANCE instance) {
  DCHECK(installer::kVivaldi);
  base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  base::FilePath vivaldi_target_path =
      cmd_line.GetSwitchValuePath(vivaldi::constants::kVivaldiInstallDir);
  g_silent_install = cmd_line.HasSwitch(vivaldi::constants::kVivaldiSilent);

  const bool is_update = cmd_line.HasSwitch(vivaldi::constants::kVivaldiUpdate);
  g_start_browser_after_install =
      !cmd_line.HasSwitch(installer::switches::kDoNotLaunchChrome);
  bool is_silent_update = cmd_line.HasSwitch(switches::kVivaldiSilentUpdate);
  if (is_silent_update) {
    g_silent_install = true;
    g_start_browser_after_install = false;
  }
  vivaldi::InstallType vivaldi_install_type =
      vivaldi::InstallType::kForCurrentUser;
  if (cmd_line.HasSwitch(installer::switches::kSystemLevel)) {
    vivaldi_install_type = vivaldi::InstallType::kForAllUsers;
  } else if (cmd_line.HasSwitch(vivaldi::constants::kVivaldiStandalone)) {
    vivaldi_install_type = vivaldi::InstallType::kStandalone;
  }
  bool is_from_mini = cmd_line.HasSwitch(vivaldi::constants::kVivaldiMini);
  if (is_from_mini) {
    // Do not propagate the switch
    cmd_line.RemoveSwitch(vivaldi::constants::kVivaldiMini);
  }

  // We only show Vivaldi UI when run from the mini-installer and this is not
  // an update. This excludes any internal invocations that are run from the
  // installation directory like uninstall or to delete old binaries.
  bool ask_for_option = is_from_mini && !is_update;
  if (ask_for_option) {
    if (g_silent_install) {
      if (vivaldi_target_path.empty()) {
        // for silent installs, make sure we have an install path
        if (vivaldi_install_type == vivaldi::InstallType::kStandalone) {
          LOG(ERROR) << "Vivaldi silent standalone install requires --"
                     << vivaldi::constants::kVivaldiInstallDir << " option";
          return false;
        }
        vivaldi_target_path =
            vivaldi::GetDefaultInstallTopDir(vivaldi_install_type);
        if (vivaldi_target_path.empty()) {
          return false;
        }
      }
    } else {
      // For non-GUI cases Chromium code does this check in setup_main.cc
      if (!InstallUtil::IsOSSupported()) {
        // TODO(jarle@vivaldi.com): Localize
        MessageBox(NULL, L"Vivaldi requires Windows 7 or higher.", NULL,
                   MB_ICONINFORMATION | MB_SETFOREGROUND);
        return false;
      }

      INITCOMMONCONTROLSEX iccx;
      iccx.dwSize = sizeof(iccx);
      iccx.dwICC = ICC_COOL_CLASSES | ICC_BAR_CLASSES | ICC_TREEVIEW_CLASSES |
                   ICC_USEREX_CLASSES;
      ::InitCommonControlsEx(&iccx);

      installer::VivaldiInstallDialog dlg(instance, false, vivaldi_install_type,
                                          vivaldi_target_path);

      const installer::VivaldiInstallDialog::DlgResult dlg_result =
          dlg.ShowModal();
      if (dlg_result != installer::VivaldiInstallDialog::INSTALL_DLG_INSTALL) {
        VLOG(1) << "Vivaldi: install cancelled/failed.";
        return false;
      }

      vivaldi_target_path = dlg.GetDestinationFolder();
      vivaldi_install_type = dlg.GetInstallType();

      if (dlg.GetSetAsDefaultBrowser()) {
        cmd_line.AppendSwitch(installer::switches::kMakeChromeDefault);
        VLOG(1) << "Vivaldi: set as default browser.";
      }

      if (dlg.GetRegisterBrowser()) {
        cmd_line.AppendSwitch(vivaldi::constants::kVivaldiRegisterStandalone);
        VLOG(1) << "Vivaldi: register standalone browser.";
      }
    }
    cmd_line.AppendSwitchPath(vivaldi::constants::kVivaldiInstallDir,
                              vivaldi_target_path);
  }

  if (is_update) {
    if (vivaldi_target_path.empty()) {
      LOG(ERROR) << "Vivaldi update requires --"
                 << vivaldi::constants::kVivaldiInstallDir << " option";
      return false;
    }
    // Find the install type of the installed Vivaldi.
    // If installed, update the main install type, unless we are launched
    // with the --system-level flag. Then we keep the install type since we
    // have no way to find out if this is a system install for non-standard
    // install paths.
    if (vivaldi_install_type != vivaldi::InstallType::kForAllUsers) {
      base::Optional<vivaldi::InstallType> existing_install_type =
          vivaldi::FindInstallType(vivaldi_target_path);
      if (existing_install_type) {
        vivaldi_install_type = *existing_install_type;
      }
    }
  }

  // Sync switches with the final configuration as we query them in few places
  // throught out the installer and to let Chromium settings code pick them.
  switch (vivaldi_install_type) {
    case vivaldi::InstallType::kForCurrentUser:
      cmd_line.RemoveSwitch(installer::switches::kSystemLevel);
      cmd_line.RemoveSwitch(vivaldi::constants::kVivaldiStandalone);
      VLOG(1) << "Vivaldi: install for current user - install_dir="
              << vivaldi_target_path.value();
      break;
    case vivaldi::InstallType::kForAllUsers:
      cmd_line.AppendSwitch(installer::switches::kSystemLevel);
      cmd_line.RemoveSwitch(vivaldi::constants::kVivaldiStandalone);
      VLOG(1)
          << "Vivaldi: install for all users (system install) - install_dir="
          << vivaldi_target_path.value();
      break;
    case vivaldi::InstallType::kStandalone:
      cmd_line.RemoveSwitch(installer::switches::kSystemLevel);
      cmd_line.AppendSwitch(vivaldi::constants::kVivaldiStandalone);
      VLOG(1) << "Vivaldi: standalone install - install dir="
              << vivaldi_target_path.value();
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
  if (!installer_state.is_vivaldi_silent_update()) {
    if (!TryToCloseAllRunningBrowsers(installer_state))
      return false;
  }
  if (!g_silent_install) {
    g_vivaldi_progress_dialog = new installer::VivaldiProgressDialog(instance);
    g_vivaldi_progress_dialog->ShowModeless();
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

void FinalizeSuccessfullInstall(
    const installer::InstallerState& installer_state,
    installer::InstallStatus install_status) {
  DCHECK(installer::kVivaldi);
  // See comments in RunInstaller in updatedownloader.cc why we have to do this
  // even for full installs.
  UpdateDeltaPatchStatus(true);

  if (installer_state.is_vivaldi_standalone()) {
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

  vivaldi::RestartUpdateNotifier(installer_state.target_path());
  base::DeleteFile(installer_state.target_path().Append(
      vivaldi::constants::kVivaldiUpdateNotifierOldExe));

  if (g_start_browser_after_install) {
    base::FilePath vivaldi_path =
        installer_state.target_path().Append(installer::kChromeExe);

    // We need to use the custom ShellExecuteFromExplorer to avoid
    // launching vivaldi.exe with elevated privileges.
    // The setup.exe process could be elevated.
    VLOG(1) << "Launching: " << vivaldi_path.value()
            << ", is_standalone() = " << installer_state.is_vivaldi_standalone()
            << ", install_status = " << static_cast<int>(install_status);
    vivaldi::ShellExecuteFromExplorer(vivaldi_path, base::string16(),
                                      base::FilePath());
  }
}

}  // namespace vivaldi
