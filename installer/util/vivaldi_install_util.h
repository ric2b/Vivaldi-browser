// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.

#ifndef INSTALLER_UTIL_VIVALDI_INSTALL_UTIL_H_
#define INSTALLER_UTIL_VIVALDI_INSTALL_UTIL_H_

#include <windows.h>
#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/version.h"
#include "base/win/scoped_handle.h"

namespace vivaldi {

namespace constants {
// Vivaldi registry.
extern const wchar_t kVivaldiAutoUpdate[];
extern const wchar_t kVivaldiDeltaPatchFailed[];
extern const wchar_t kVivaldiKey[];
extern const wchar_t kVivaldiPinToTaskbarValue[];

// Vivaldi installer settings from last install.
extern const wchar_t kVivaldiInstallerDestinationFolder[];
extern const wchar_t kVivaldiInstallerInstallType[];
extern const wchar_t kVivaldiInstallerDefaultBrowser[];
extern const wchar_t kVivaldiInstallerRegisterBrowser[];
extern const wchar_t kVivaldiInstallerAdvancedMode[];

// Vivaldi paths and filenames
extern const wchar_t kVivaldiUpdateNotifierExe[];
extern const wchar_t kVivaldiUpdateNotifierOldExe[];

// Vivaldi installer command line switches.
extern const char kVivaldi[];
extern const char kVivaldiInstallDir[];
extern const char kVivaldiStandalone[];
extern const char kVivaldiForceLaunch[];
extern const char kVivaldiUpdate[];
extern const char kVivaldiRegisterStandalone[];
extern const char kVivaldiSilent[];
} // namespace constants

// Does the post uninstall operations - open the URL to the uninstall survey.
void DoPostUninstallOperations(const base::Version& version);

// Launches process non-elevated even if the caller is elevated,
// using explorer. Reference:
// https://blogs.msdn.microsoft.com/oldnewthing/20131118-00/?p=2643
bool ShellExecuteFromExplorer(const base::FilePath& application_path,
                              const base::string16& parameters,
                              const base::FilePath& directory,
                              const base::string16& operation,
                              int nShowCmd = SW_SHOWDEFAULT);

// Returns the URL with parameters for the 'new features' page.
// The new |version| is appended as a parameter in the URL.
base::string16 GetNewFeaturesUrl(const base::Version& version);

// Gets handles to all active processes on the system running from a given path,
// that could be opened with the |desired_access|.
std::vector<base::win::ScopedHandle> GetRunningProcessesForPath(
  const base::FilePath& path);

// Kills |processes|. The handles must have been open with PROCESS_TERMINATE
// access for this to succeed.
void KillProcesses(const std::vector<base::win::ScopedHandle>& processes);

}  // namespace vivaldi

#endif  // INSTALLER_UTIL_VIVALDI_INSTALL_UTIL_H_
