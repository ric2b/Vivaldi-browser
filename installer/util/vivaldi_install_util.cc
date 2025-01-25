// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.

#include "installer/util/vivaldi_install_util.h"

#include <Windows.h>

#include <shlobj.h>

#include "base/file_version_info_win.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/win/registry.h"
#include "chrome/installer/util/util_constants.h"

#include "app/vivaldi_constants.h"
#include "base/vivaldi_switches.h"
#include "update_notifier/update_notifier_switches.h"

namespace vivaldi {

bool g_inside_installer_application = false;

namespace {

// base::PathExists() asserts on non-blocking call, but we may need to check for
// file existence on UI thread, so provide own implementation.
bool DoesPathExist(const base::FilePath& path) {
  return (::GetFileAttributes(path.value().c_str()) != INVALID_FILE_ATTRIBUTES);
}

}  // namespace

InstallType GetBrowserInstallType() {
  // Cache the value on the first call.
  static InstallType browser_install_type =
      FindInstallType(GetDirectoryOfCurrentExe().DirName())
          .value_or(InstallType::kForCurrentUser);
  return browser_install_type;
}

bool IsVivaldiInstalled(const base::FilePath& install_top_dir) {
  base::FilePath install_binary_dir =
      install_top_dir.Append(installer::kInstallBinaryDir);
  base::FilePath vivaldi_exe_path =
      install_binary_dir.Append(installer::kChromeExe);

  bool is_installed = DoesPathExist(vivaldi_exe_path);
  return is_installed;
}

std::optional<InstallType> FindInstallType(
    const base::FilePath& install_top_dir) {
  if (!IsVivaldiInstalled(install_top_dir))
    return std::nullopt;

  base::FilePath install_binary_dir =
      install_top_dir.Append(installer::kInstallBinaryDir);
  if (DoesPathExist(
          install_binary_dir.Append(constants::kStandaloneMarkerFile)))
    return InstallType::kStandalone;
  if (DoesPathExist(install_binary_dir.Append(constants::kSystemMarkerFile)))
    return InstallType::kForAllUsers;

  // Support older installations without the marker files for system
  // installations. We check both for 32 and 64 paths irrespective of the
  // current architecture as the user may have installed a 64 bit version over
  // 32 installation or vise-versa, see VB-79028.
  //
  // TODO(igor@vivaldi.com): Do this check only in setup.exe, not for the
  // browser or update_notifier.exe as those should see kSystemMarkerFile that
  // the installer has added.
  base::FilePath program_files_path;
  base::PathService::Get(base::DIR_PROGRAM_FILES6432, &program_files_path);
  if (program_files_path.IsParent(install_top_dir)) {
    return InstallType::kForAllUsers;
  }
  program_files_path.clear();
  base::PathService::Get(base::DIR_PROGRAM_FILESX86, &program_files_path);
  if (program_files_path.IsParent(install_top_dir)) {
    return InstallType::kForAllUsers;
  }

  return InstallType::kForCurrentUser;
}

bool IsStandaloneBrowser() {
  return GetBrowserInstallType() == InstallType::kStandalone;
}

base::FilePath GetDefaultInstallTopDir(InstallType install_type) {
  int csidl;
  switch (install_type) {
    case InstallType::kForAllUsers:
      csidl = CSIDL_PROGRAM_FILES;
      break;
    case InstallType::kForCurrentUser:
      csidl = CSIDL_LOCAL_APPDATA;
      break;
    case InstallType::kStandalone:
      NOTREACHED();
      //return base::FilePath();
  }

  wchar_t szPath[MAX_PATH];
  // TODO(igor@vivaldi.com): Use ::SHGetKnownFolderPath.
  if (FAILED(::SHGetFolderPath(nullptr, csidl, nullptr, 0, szPath))) {
    PLOG(ERROR) << "Failed SHGetFolderPath";
    return base::FilePath();
  }
  base::FilePath install_top_dir = base::FilePath(szPath).Append(L"Vivaldi");
  return install_top_dir;
}

const base::FilePath& GetPathOfCurrentExe() {
  // This cannot use base::PathService::Get(base::DIR_EXE) as that calls
  // GetModuleFileName() and that does not normalize the exe path.
  static base::NoDestructor<base::FilePath> exe_path([] {
    wchar_t path[MAX_PATH];
    path[0] = L'\0';
    DWORD size = std::size(path);
    if (!::QueryFullProcessImageName(::GetCurrentProcess(), 0, path, &size)) {
      PLOG(INFO) << "Failed QueryFullProcessImageName()";
      if (GetModuleFileName(nullptr, path, std::size(path)) == 0) {
        PLOG(INFO) << "Failed QueryFullProcessImageName()";
        return base::FilePath();
      }
    }
    return base::FilePath(path);
  }());
  return *exe_path;
}

const base::FilePath& GetDirectoryOfCurrentExe() {
  static base::NoDestructor<base::FilePath> current_exe_dir(
      [] { return GetPathOfCurrentExe().DirName(); }());
  return *current_exe_dir;
}

base::FilePath GetInstallBinaryDir() {
  // For the installer use the value of --vivaldi-install-dir.
  if (g_inside_installer_application) {
    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();
    if (command_line.HasSwitch(vivaldi::constants::kVivaldiInstallDir)) {
      return command_line
          .GetSwitchValuePath(vivaldi::constants::kVivaldiInstallDir)
          .Append(installer::kInstallBinaryDir);
    }
  }
  base::FilePath path = GetDirectoryOfCurrentExe();
  if (g_inside_installer_application) {
    // We are called from setup.exe and the kVivaldiInstallDir option is not
    // specified. Thus this is an invocation of setup.exe that is a part of an
    // existing installation. Strip the version/Install part.
    path = path.DirName().DirName();

    // Check that setup.exe is indeed a part of an installation.
    base::FilePath browser_exe = path.Append(installer::kChromeExe);
    if (!DoesPathExist(browser_exe)) {
      LOG(ERROR) << browser_exe << " does not exist";
      return base::FilePath();
    }
  }
  return path;
}

void AppendInstallChildProcessSwitches(base::CommandLine& command_line) {
  if (!g_inside_installer_application) {
    // This brunch can be hit in Chromium tests.
    return;
  }
  // Let the child process to know where to install and if it is a standalone.
  // Chromium passes installer::switches::kSystemLevel itself.
  const char* const switched_to_copy[] = {
      vivaldi::constants::kVivaldiInstallDir,
      vivaldi::constants::kVivaldiStandalone,
  };
  const base::CommandLine& current = *base::CommandLine::ForCurrentProcess();
  command_line.CopySwitchesFrom(current, switched_to_copy);
}

namespace {

base::Version ReadExeVersion(const base::FilePath& exe_path) {
  std::unique_ptr<FileVersionInfoWin> file_version_info =
      FileVersionInfoWin::CreateFileVersionInfoWin(exe_path);
  if (!file_version_info) {
    PLOG(ERROR) << "Failed to extract version info for " << exe_path;
    return base::Version();
  }
  base::Version version = file_version_info->GetFileVersion();
  if (!version.IsValid()) {
    LOG(ERROR) << "Cannot determine the version of " << exe_path;
    return base::Version();
  }
  return version;
}

}  // namespace

base::Version GetInstallVersion(base::FilePath install_binary_dir) {
  if (install_binary_dir.empty()) {
    install_binary_dir = GetInstallBinaryDir();
  }

  // If kChromeNewExe is present, read the version from it to reflect the state
  // after a successfull installation that just waits for the user to approve
  // renaming of executables.
  const wchar_t* const exe_to_try[] = {
      installer::kChromeNewExe,
      installer::kChromeExe,
  };
  for (const wchar_t* exe_name : exe_to_try) {
    base::FilePath exe_path = install_binary_dir.Append(exe_name);
    if (DoesPathExist(exe_path)) {
      base::Version version = ReadExeVersion(exe_path);
      if (version.IsValid())
        return version;
    }
  }
  return base::Version();
}

std::optional<base::Version> GetPendingUpdateVersion(
    base::FilePath install_binary_dir) {
  if (install_binary_dir.empty()) {
    install_binary_dir = GetInstallBinaryDir();
  }
  base::FilePath new_exe_path =
      install_binary_dir.Append(installer::kChromeNewExe);
  if (!DoesPathExist(new_exe_path))
    return std::nullopt;
  return ReadExeVersion(new_exe_path);
}

installer::AppCommand GetNewUpdateFinalizeCommand() {
  // This is based on AppendPostInstallTasks from install_worker.cc, see the
  // part that constructs the rename command to write to the registry. In
  // Vivaldi we skip the registry and construct the command as necessary.
  base::FilePath install_binary_dir = GetInstallBinaryDir();
  base::Version version =
      ReadExeVersion(install_binary_dir.Append(installer::kChromeNewExe));
  if (!version.IsValid())
    return installer::AppCommand();
  base::FilePath setup_exe = install_binary_dir.AppendASCII(version.GetString())
                                 .Append(installer::kInstallerDir)
                                 .Append(installer::kSetupExe);
  base::CommandLine rename_cmd(setup_exe);
  rename_cmd.AppendSwitch(installer::switches::kRenameChromeExe);
  const base::CommandLine* vivaldi_cmd_line =
      base::CommandLine::ForCurrentProcess();
  if (vivaldi_cmd_line->HasSwitch(installer::switches::kEnableLogging) ||
      vivaldi_cmd_line->HasSwitch(installer::switches::kVerboseLogging)) {
    rename_cmd.AppendSwitch(installer::switches::kVerboseLogging);
  }
  return installer::AppCommand(installer::kSetupExe, rename_cmd.GetCommandLineString());
}

void SendQuitUpdateNotifier(const base::FilePath& install_binary_dir,
                            bool global) {
  std::wstring event_name = GetUpdateNotifierEventName(
      global ? vivaldi_update_notifier::kGlobalQuitEventPrefix
             : vivaldi_update_notifier::kQuitEventPrefix,
      install_binary_dir);

  DLOG(INFO) << "Sending quit event " << event_name;
  base::win::ScopedHandle quit_event(
      ::OpenEvent(EVENT_MODIFY_STATE, FALSE, event_name.c_str()));
  if (!quit_event.IsValid()) {
    // No modifiers listen for the event
    return;
  }

  ::SetEvent(quit_event.Get());
}

base::FilePath GetUpdateNotifierPath(const base::FilePath& install_binary_dir) {
  const base::FilePath& exe_dir_ref = !install_binary_dir.empty()
                                          ? install_binary_dir
                                          : GetDirectoryOfCurrentExe();
  return exe_dir_ref.Append(vivaldi::constants::kVivaldiUpdateNotifierExe);
}

base::CommandLine GetCommonUpdateNotifierCommand(
    const base::FilePath& install_binary_dir) {
  // This must be thread-safe and non-blocking so it can be called from any
  // thread including UI thread.
  base::CommandLine command(GetUpdateNotifierPath(install_binary_dir));
  const base::CommandLine* vivaldi_cmd_line =
      base::CommandLine::ForCurrentProcess();
  if (vivaldi_cmd_line->HasSwitch(switches::kVivaldiUpdateURL)) {
    command.AppendSwitchNative(
        switches::kVivaldiUpdateURL,
        vivaldi_cmd_line->GetSwitchValueNative(switches::kVivaldiUpdateURL));
  }

  if (vivaldi_cmd_line->HasSwitch(installer::switches::kDisableLogging)) {
    command.AppendSwitch(installer::switches::kDisableLogging);
  } else {
    // Make logging verbose if invoked from a browser with enabled logging or
    // from the installer with the verbose logging.
    if (vivaldi_cmd_line->HasSwitch(installer::switches::kEnableLogging) ||
        vivaldi_cmd_line->HasSwitch(installer::switches::kVerboseLogging)) {
      command.AppendSwitch(installer::switches::kVerboseLogging);
    }
  }
  if (vivaldi_cmd_line->HasSwitch(switches::kVivaldiSilentUpdate)) {
    command.AppendSwitch(switches::kVivaldiSilentUpdate);
  }
  return command;
}

bool LaunchNotifierProcess(const base::CommandLine& cmdline) {
  DCHECK(base::FilePath::CompareEqualIgnoreCase(
      cmdline.GetProgram().BaseName().value(),
      vivaldi::constants::kVivaldiUpdateNotifierExe));
  base::LaunchOptions options;
  options.current_directory = cmdline.GetProgram().DirName();
  base::Process process = base::LaunchProcess(cmdline, options);
  if (!process.IsValid()) {
    LOG(ERROR) << "Failed to launch process - "
               << cmdline.GetCommandLineString();
    return false;
  }
  return true;
}

int RunNotifierSubaction(const base::CommandLine& cmdline) {
  DCHECK(base::FilePath::CompareEqualIgnoreCase(
      cmdline.GetProgram().BaseName().value(),
      vivaldi::constants::kVivaldiUpdateNotifierExe));
  base::LaunchOptions options;
  options.current_directory = cmdline.GetProgram().DirName();
  base::Process process = base::LaunchProcess(cmdline, options);
  if (!process.IsValid()) {
    LOG(ERROR) << "Failed to launch process - "
               << cmdline.GetCommandLineString();
    return -1;
  }

  // Typically an update notifier action finishes within milliseconds. So if it
  // takes 10 seconds, this is definitely a bug. Kill then the process.
  base::TimeDelta max_wait = base::Seconds(10);
  int exit_code = 0;
  if (!process.WaitForExitWithTimeout(max_wait, &exit_code)) {
    LOG(ERROR)
        << "Timed out while waiting for the notifier process to finish - "
        << cmdline.GetCommandLineString();
    process.Terminate(1, false);
    return -1;
  }
  return exit_code;
}

std::wstring GetUpdateNotifierEventName(
    std::wstring_view event_prefix,
    const base::FilePath& install_binary_dir) {
  const base::FilePath& exe_dir_ref = !install_binary_dir.empty()
                                          ? install_binary_dir
                                          : GetDirectoryOfCurrentExe();
  std::wstring normalized_path =
      exe_dir_ref.NormalizePathSeparatorsTo(L'/').value();
  // See
  // https://web.archive.org/web/20130528052217/http://blogs.msdn.com/b/michkap/archive/2005/10/17/481600.aspx
  ::CharUpper(&normalized_path[0]);
  std::wstring event_name(event_prefix.data(), event_prefix.length());
  event_name += normalized_path;
  return event_name;
}

base::win::RegKey OpenRegistryKeyToRead(HKEY rootkey, const wchar_t* subkey) {
  base::win::RegKey key;
  LONG status = key.Open(rootkey, subkey, KEY_QUERY_VALUE);
  if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) {
    LOG(ERROR) << base::StringPrintf(
        "Failed to open registry key %ls for reading status=0x%lx", subkey,
        status);
  }
  return key;
}

base::win::RegKey OpenRegistryKeyToWrite(HKEY rootkey, const wchar_t* subkey) {
  base::win::RegKey key;
  LONG status = key.Create(rootkey, subkey, KEY_ALL_ACCESS);
  if (status != ERROR_SUCCESS) {
    LOG(ERROR) << base::StringPrintf(
        "Failed to open registry key %ls for writing status=0x%lx", subkey,
        status);
  }
  return key;
}

std::wstring ReadRegistryString(const wchar_t* name,
                                const base::win::RegKey& key) {
  if (!key.Valid())
    return std::wstring();
  std::wstring value;
  LSTATUS status = key.ReadValue(name, &value);
  if (status != ERROR_SUCCESS) {
    if (status != ERROR_FILE_NOT_FOUND) {
      LOG(ERROR) << base::StringPrintf(
          "Failed to read registry name %ls status==0x%lx", name, status);
    }
    return std::wstring();
  }
  if (value.empty()) {
    LOG(ERROR) << "Invalid empty string value for the registry name " << name;
    return std::wstring();
  }
  return value;
}

std::optional<uint32_t> ReadRegistryUint32(const wchar_t* name,
                                            const base::win::RegKey& key) {
  if (!key.Valid())
    return std::nullopt;
  DWORD value = 0;
  LSTATUS status = key.ReadValueDW(name, &value);
  if (status != ERROR_SUCCESS) {
    if (status != ERROR_FILE_NOT_FOUND) {
      LOG(ERROR) << base::StringPrintf(
          "Failed to read registry name %ls status==0x%lx", name, status);
    }
    return std::nullopt;
  }
  return value;
}

std::optional<bool> ReadRegistryBool(const wchar_t* name,
                                      const base::win::RegKey& key) {
  std::optional<uint32_t> value_word = ReadRegistryUint32(name, key);
  if (!value_word)
    return std::nullopt;
  if (*value_word > 1) {
    LOG(ERROR) << "Invalid boolean registry value in " << name << ": "
               << *value_word;
    return std::nullopt;
  }
  return *value_word != 0;
}

void WriteRegistryString(const wchar_t* name,
                         const std::wstring& value,
                         base::win::RegKey& key) {
  if (!key.Valid())
    return;
  if (value.empty()) {
    LONG status = key.DeleteValue(name);
    if (status != ERROR_SUCCESS) {
      LOG(ERROR) << base::StringPrintf(
          "Failed to delete registry name %ls status==0x%lx", name, status);
    }
  } else {
    LONG status = key.WriteValue(name, value.c_str());
    if (status != ERROR_SUCCESS) {
      LOG(ERROR) << base::StringPrintf(
          "Failed to write registry name %ls status==0x%lx", name, status);
    }
  }
}

void WriteRegistryUint32(const wchar_t* name,
                         DWORD value,
                         base::win::RegKey& key) {
  if (!key.Valid())
    return;
  LONG status = key.WriteValue(name, value);
  if (status != ERROR_SUCCESS) {
    LOG(ERROR) << base::StringPrintf(
        "Failed to write registry name %ls status==0x%lx", name, status);
  }
}

void WriteRegistryBool(const wchar_t* name,
                       bool value,
                       base::win::RegKey& key) {
  WriteRegistryUint32(name, value ? 1 : 0, key);
}

}  // namespace vivaldi
