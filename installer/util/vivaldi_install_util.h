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

namespace base {
namespace win {
class RegKey;
}
}  // namespace base

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

bool IsStandaloneBrowser();

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

// Gets handles to all active processes on the system running from a given path.
// If path matches the path of the current process, it is not included.
std::vector<base::win::ScopedHandle> GetRunningProcessesForPath(
    const base::FilePath& path);

// Kills |processes|. The handles must have been open with PROCESS_TERMINATE
// access for this to succeed.
void KillProcesses(std::vector<base::win::ScopedHandle> processes);

// Return the path of the directory holding the current exe. Compared with
// base::PathService::Get(base::FILE_EXE) the result is normalized with all
// symbolic link and mount points resolved.
const base::FilePath& GetDirectoryOfCurrentExe();

// Get path of the current exe with all symlinks and mount points resolved.
const base::FilePath& GetPathOfCurrentExe();

// Get the update notifier executable from the given directory. If exe_dir is
// null, use GetDirectoryOfCurrentExe().
base::FilePath GetUpdateNotifierPath(const base::FilePath* exe_dir);

// Get the command line to start the update notifier with common switches
// reflecting the current browser process flags. This can be called from any
// thread. If exe_dir is null, use GetDirectoryOfCurrentExe().
base::CommandLine GetCommonUpdateNotifierCommand(const base::FilePath* exe_dir);

// Return true if the update notifier is enabled for the installation in the
// given exe_dir and optionally copy the full command line that should be used
// with it. If exe_dir is null, use GetDirectoryOfCurrentExe().
bool IsUpdateNotifierEnabled(const base::FilePath* exe_dir,
                             base::CommandLine* cmdline = nullptr);

// Return true if the update notifier is enabled for some path installation
// path. This can block and must not be called from the browser UI thread.
bool IsUpdateNotifierEnabledForAnyPath();

// Enable the update notifier using the given commnad line and launch the
// updater process. This can block and must not be called from the browser UI
// thread.
bool EnableUpdateNotifier(const base::CommandLine& cmdline);

// Disable the update notifier and quit any running instance. If exe_dir is
// null, use GetDirectoryOfCurrentExe(). This can block and must not be called
// from the browser UI thread.
bool DisableUpdateNotifier(const base::FilePath* exe_dir);

// Launch the update notifier using the given commnad line. This can block and
// must not be called from the browser UI thread.
bool LaunchNotifierProcess(const base::CommandLine& command);

// Launch the update notifier using one of its subactions and wait for its exit.
// Return the exit code or -1 on errors. This can block and must not be called
// from the browser UI thread.
int RunNotifierSubaction(const base::CommandLine& command);

void SendQuitUpdateNotifier(const base::FilePath* exe_dir, bool global);

// Get Winapi event name for the update notifier from the given exe_dir. If
// exe_dir is null, use GetDirectoryOfCurrentExe().
base::string16 GetUpdateNotifierEventName(base::StringPiece16 event_prefix,
                                          const base::FilePath* exe_dir);

// For the installer installer_exe_dir comes from the user input, not
// GetDirectoryOfCurrentExe(), and may contain symlinks etc. Thus we must
// normalize it as we use the path to construct signal names and compare with
// the path in the registry for autostart.
base::FilePath NormalizeInstallExeDirectory(const base::FilePath& exe_dir);

// Installer-specific helper to quit all running notifiers.
void QuitAllUpdateNotifiers(const base::FilePath& installer_exe_dir,
                            bool quit_old);

// Read a string from the registry. Return an empty string on errors. This
// assumes that an empty string is never a valid value.
std::wstring ReadRegistryString(const wchar_t* name, base::win::RegKey& key);

// Return nullopt on errors.
base::Optional<DWORD> ReadRegistryDW(const wchar_t* name,
                                     base::win::RegKey& key);

// Return nullopt on errors.
base::Optional<bool> ReadRegistryBool(const wchar_t* name,
                                      base::win::RegKey& key);
}  // namespace vivaldi

#endif  // INSTALLER_UTIL_VIVALDI_INSTALL_UTIL_H_
