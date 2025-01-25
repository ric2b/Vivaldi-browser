// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.

#ifndef INSTALLER_UTIL_VIVALDI_INSTALL_UTIL_H_
#define INSTALLER_UTIL_VIVALDI_INSTALL_UTIL_H_

#include <string_view>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "chrome/installer/util/app_command.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#include "installer/util/vivaldi_install_constants.h"

// Installation-related utilities shared by the browser, installer and
// update_notifier.exe

namespace installer {

// Marker to annotate Vivaldi-specific changes to the Chromium
// installation-related code when it is otherwise not clear if the change is
// from a Vivaldi patch.
constexpr bool kVivaldi = true;

}  // namespace installer

namespace vivaldi {

// Flag indicating that we are running setup.exe, not the browser process.
extern bool g_inside_installer_application;

enum class InstallType {
  // Install for the current user
  kForCurrentUser,

  // Install for all users, system level.
  kForAllUsers,

  // Vivaldi standalone installation
  kStandalone,
};

bool IsVivaldiInstalled(const base::FilePath& install_top_dir);

std::optional<InstallType> FindInstallType(
    const base::FilePath& install_top_dir);

bool IsStandaloneBrowser();

InstallType GetBrowserInstallType();

// Get the default directory for the Vivaldi installation. install_type must not
// be kStandalone as that has no notion of a default directory.
base::FilePath GetDefaultInstallTopDir(InstallType install_type);

// Return the path of the directory holding the current exe. Compared with
// base::PathService::Get(base::FILE_EXE) the result is normalized with all
// symbolic link and mount points resolved.
const base::FilePath& GetDirectoryOfCurrentExe();

// Get path of the current exe with all symlinks and mount points resolved.
const base::FilePath& GetPathOfCurrentExe();

base::FilePath GetInstallBinaryDir();

// NOTE(igor@vivaldi.com): Ideally this should be in vivaldi_setup_util.h as it
// is only used by the setup process but to simplify linking we place this here.
void AppendInstallChildProcessSwitches(base::CommandLine& command_line);

// Get the version of Vivaldi installation in the given directory or in
// GetInstallBinaryDir() if not given. If there is a pending update, return its
// version.
base::Version GetInstallVersion(
    base::FilePath install_binary_dir = base::FilePath());

// Return the version of a pending update if any or nullopt if there is no
// pending update. If the version cannot be determined, return a non-null
// optional with the version where IsValid() method return false. Use
// GetInstallBinaryDir() if install_binary_dir is not given.
std::optional<base::Version> GetPendingUpdateVersion(
    base::FilePath install_binary_dir = base::FilePath());

// Assuming kChromeNewExe exists alongside the browser executable, return the
// command to finish the installation. Return an empty string on errors.
installer::AppCommand GetNewUpdateFinalizeCommand();

// Get the update notifier executable from the given directory. If
// install_binary_dir is empty, use GetDirectoryOfCurrentExe().
base::FilePath GetUpdateNotifierPath(const base::FilePath& install_binary_dir);

// Get the command line to start the update notifier with common switches
// reflecting the current browser process flags. This can be called from any
// thread. If install_binary_dir is empty, use GetDirectoryOfCurrentExe().
base::CommandLine GetCommonUpdateNotifierCommand(
    const base::FilePath& install_binary_dir = base::FilePath());

// Launch the update notifier using the given commnad line. This can block and
// must not be called from the browser UI thread.
bool LaunchNotifierProcess(const base::CommandLine& command);

// Launch the update notifier using one of its subactions and wait for its exit.
// Return the exit code or -1 on errors. This can block and must not be called
// from the browser UI thread.
int RunNotifierSubaction(const base::CommandLine& command);

void SendQuitUpdateNotifier(const base::FilePath& install_binary_dir,
                            bool global = false);

// Get Winapi event name for the update notifier from the given
// install_binary_dir. If install_binary_dir is empty, use
// GetDirectoryOfCurrentExe().
std::wstring GetUpdateNotifierEventName(
    std::wstring_view event_prefix,
    const base::FilePath& install_binary_dir);

base::win::RegKey OpenRegistryKeyToRead(HKEY rootkey, const wchar_t* subkey);
base::win::RegKey OpenRegistryKeyToWrite(HKEY rootkey, const wchar_t* subkey);

// Read a string from the registry. Return an empty string on errors. This
// assumes that an empty string is never a valid value.
std::wstring ReadRegistryString(const wchar_t* name,
                                const base::win::RegKey& key);

// Return nullopt on errors.
std::optional<uint32_t> ReadRegistryUint32(const wchar_t* name,
                                            const base::win::RegKey& key);

// Return nullopt on errors.
std::optional<bool> ReadRegistryBool(const wchar_t* name,
                                      const base::win::RegKey& key);

// If value is empty, this delete the name.
void WriteRegistryString(const wchar_t* name,
                         const std::wstring& value,
                         base::win::RegKey& key);

void WriteRegistryUint32(const wchar_t* name,
                         DWORD value,
                         base::win::RegKey& key);

void WriteRegistryBool(const wchar_t* name, bool value, base::win::RegKey& key);

}  // namespace vivaldi

#endif  // INSTALLER_UTIL_VIVALDI_INSTALL_UTIL_H_
