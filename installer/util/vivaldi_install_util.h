// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.

#ifndef INSTALLER_UTIL_VIVALDI_INSTALL_UTIL_H_
#define INSTALLER_UTIL_VIVALDI_INSTALL_UTIL_H_

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/version.h"
#include "base/win/scoped_handle.h"

#include "installer/util/vivaldi_install_constants.h"

namespace vivaldi {

enum class InstallType {
  // Install for the current user
  kForCurrentUser,

  // Install for all users, system level.
  kForAllUsers,

  // Vivaldi standalone installation
  kStandalone,
};

bool IsVivaldiInstalled(const base::FilePath& install_top_dir);

base::Optional<InstallType> FindInstallType(
    const base::FilePath& install_top_dir);

// Get the default directory for the Vivaldi installation. install_type must not
// be kStandalone as that has no notion of a default directory.
base::FilePath GetDefaultInstallTopDir(InstallType install_type);

// Does the post uninstall operations - open the URL to the uninstall survey.
void DoPostUninstallOperations(const base::Version& version);

// Launches process non-elevated even if the caller is elevated,
// using explorer. Reference:
// https://blogs.msdn.microsoft.com/oldnewthing/20131118-00/?p=2643
bool ShellExecuteFromExplorer(const base::FilePath& application_path,
                              const base::string16& parameters,
                              const base::FilePath& directory);

// Gets handles to all active processes on the system running from a given path
// or path2 if it is given.
std::vector<base::win::ScopedHandle> GetRunningProcessesForPath(
    const base::FilePath& path,
    const base::FilePath* path2 = nullptr);

// Kills |processes|. The handles must have been open with PROCESS_TERMINATE
// access for this to succeed.
void KillProcesses(std::vector<base::win::ScopedHandle> processes);

// Return the path of the directory holding the current exe. Compared with
// base::PathService::Get(base::FILE_EXE) the result is normalized with all
// symbolic link and mount points resolved.
const base::FilePath& GetDirectoryOfCurrentExe();

// Get the update notifier executable from the given directory. If exe_dir is
// null, use GetDirectoryOfCurrentExe().
base::FilePath GetUpdateNotifierPath(const base::FilePath* exe_dir);

// Return true if the update notifier is enabled for the installation in the
// given exe_dir and optionally copy the full command line that should be used
// with it. If exe_dir is null, use GetDirectoryOfCurrentExe().
bool IsUpdateNotifierEnabled(const base::FilePath* exe_dir,
                             base::CommandLine* cmdline = nullptr);

// Return true if the update notifier is enabled for some path installation
// path.
bool IsUpdateNotifierEnabledForAnyPath();

// Enable the update notifier using the given commnad line and launch the
// updater process.
void EnableUpdateNotifier(const base::CommandLine& cmdline);

// Disable the update notifier and quit any running instance. If exe_dir is
// null, use GetDirectoryOfCurrentExe().
void DisableUpdateNotifier(const base::FilePath* exe_dir);

// Launch the update notifier using the given commnad line.
void LaunchNotifierProcess(const base::CommandLine& command);

// Get Winapi event name for the update notifier from the given exe_dir. If
// exe_dir is null, use GetDirectoryOfCurrentExe().
base::string16 GetUpdateNotifierEventName(base::StringPiece16 event_prefix,
                                          const base::FilePath* exe_dir);

// For the installer installer_exe_dir comes from the user input, not
// GetDirectoryOfCurrentExe(), and may contain symlinks etc. Thus we must
// normalize it as we use the path to construct signal names and compare with
// the path in the registry for autostart.
base::FilePath NormalizeInstallExeDirectory(const base::FilePath& exe_dir);

// Installer-specific helper to enable the notifier during the installation.
void EnableUpdateNotifierFromInstaller(const base::FilePath& installer_exe_dir);

// Installer-specific helper to quit all running update notifiers and start a
// new one if the auto-update is enabled.
void RestartUpdateNotifier(const base::FilePath& installer_exe_dir);

// Installer-specific helper to quit all running notifiers.
void QuitAllUpdateNotifiers(const base::FilePath& installer_exe_dir);

  // Shows a modal messagebox with the installer result localized string.
void ShowInstallerResultMessage(int string_resource_id);

}  // namespace vivaldi

#endif  // INSTALLER_UTIL_VIVALDI_INSTALL_UTIL_H_
