// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.

#include "installer/util/vivaldi_install_util.h"

#include <windows.h>

#include <atlbase.h>
#include <exdisp.h>
#include <shellapi.h>
#include <shldisp.h>
#include <shlobj.h>
#include <stdlib.h>
#include <tlhelp32.h>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "base/win/wmi.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/util_constants.h"

#include "app/vivaldi_constants.h"
#include "base/vivaldi_switches.h"
#include "update_notifier/update_notifier_switches.h"

using base::PathService;

namespace vivaldi {

namespace {

// base::PathExists() asserts on non-blocking call, but we may need to check for
// file existence on UI thread, so provide own implementation.
bool DoesPathExist(const base::FilePath& path) {
  return (::GetFileAttributes(path.value().c_str()) != INVALID_FILE_ATTRIBUTES);
}

InstallType GetBrowserInstallType() {
  // Cache the value on the first call.
  static InstallType browser_install_type =
      FindInstallType(GetDirectoryOfCurrentExe().DirName())
          .value_or(InstallType::kForCurrentUser);
  return browser_install_type;
}

}  // namespace

bool IsVivaldiInstalled(const base::FilePath& install_top_dir) {
  base::FilePath exe_dir = install_top_dir.Append(installer::kInstallBinaryDir);
  base::FilePath vivaldi_exe_path = exe_dir.Append(installer::kChromeExe);

  bool is_installed = DoesPathExist(vivaldi_exe_path);
  return is_installed;
}

base::Optional<InstallType> FindInstallType(
    const base::FilePath& install_top_dir) {
  if (!IsVivaldiInstalled(install_top_dir))
    return base::nullopt;

  base::FilePath exe_dir = install_top_dir.Append(installer::kInstallBinaryDir);
  if (DoesPathExist(exe_dir.Append(constants::kStandaloneMarkerFile)))
    return InstallType::kStandalone;
  if (DoesPathExist(exe_dir.Append(constants::kSystemMarkerFile)))
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
  PathService::Get(base::DIR_PROGRAM_FILES6432, &program_files_path);
  if (program_files_path.IsParent(install_top_dir)) {
    return InstallType::kForAllUsers;
  }
  program_files_path.clear();
  PathService::Get(base::DIR_PROGRAM_FILESX86, &program_files_path);
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
      return base::FilePath();
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

namespace {

// Some of the following code is borrowed from:
// installer\util\google_chrome_distribution.cc
base::string16 LocalizeUrl(const wchar_t* url) {
  std::wstring lang = installer::GetCurrentTranslation();
  return base::ReplaceStringPlaceholders(url, lang.c_str(), NULL);
}

base::string16 GetUninstallSurveyUrl() {
  const wchar_t kSurveyUrl[] = L"https://vivaldi.com/uninstall/feedback?hl=$1";
  return LocalizeUrl(kSurveyUrl);
}

bool NavigateToUrlWithEdge(const base::string16& url) {
  base::string16 protocol_url = L"microsoft-edge:" + url;
  SHELLEXECUTEINFO info = {sizeof(info)};
  info.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
  info.lpVerb = L"open";
  info.lpFile = protocol_url.c_str();
  info.nShow = SW_SHOWNORMAL;
  if (::ShellExecuteEx(&info))
    return true;

  return false;
}

void NavigateToUrlWithIExplore(const base::string16& url) {
  base::FilePath iexplore;
  if (!PathService::Get(base::DIR_PROGRAM_FILES, &iexplore))
    return;

  iexplore = iexplore.AppendASCII("Internet Explorer");
  iexplore = iexplore.AppendASCII("iexplore.exe");

  base::string16 command = L"\"" + iexplore.value() + L"\" " + url;

  int pid = 0;
  // The reason we use WMI to launch the process is because the uninstall
  // process runs inside a Job object controlled by the shell. As long as there
  // are processes running, the shell will not close the uninstall applet. WMI
  // allows us to escape from the Job object so the applet will close.
  base::win::WmiLaunchProcess(command, &pid);
}

}  // namespace

void DoPostUninstallOperations(const base::Version& version) {
  // Add the Vivaldi version and OS version as params to the form.
  const base::string16 kVersionParam = L"version";
  const base::string16 kOSParam = L"os";

  const base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  base::win::OSInfo::VersionNumber version_number = os_info->version_number();
  base::string16 os_version =
      base::StringPrintf(L"W%d.%d.%d", version_number.major,
                         version_number.minor, version_number.build);

  base::string16 url = GetUninstallSurveyUrl() + L"&" + kVersionParam + L"=" +
                       base::ASCIIToUTF16(version.GetString()) + L"&" +
                       kOSParam + L"=" + os_version;

  if (os_info->version() >= base::win::Version::WIN10 &&
      NavigateToUrlWithEdge(url)) {
    return;
  }
  NavigateToUrlWithIExplore(url);
}

HRESULT FindDesktopFolderView(REFIID riid, void** ppv) {
  CComPtr<IShellWindows> spShellWindows;
  HRESULT hr = spShellWindows.CoCreateInstance(CLSID_ShellWindows);
  if (FAILED(hr))
    return hr;

  CComVariant vtLoc(CSIDL_DESKTOP);
  CComVariant vtEmpty;
  long lhwnd = 0;  // NOLINT - Unusual, but this API does take a long
  CComPtr<IDispatch> spdisp;
  hr = spShellWindows->FindWindowSW(&vtLoc, &vtEmpty, SWC_DESKTOP, &lhwnd,
                                    SWFO_NEEDDISPATCH, &spdisp);
  if (FAILED(hr))
    return hr;

  CComPtr<IShellBrowser> spBrowser;
  hr = CComQIPtr<IServiceProvider>(spdisp)->QueryService(
      SID_STopLevelBrowser, IID_PPV_ARGS(&spBrowser));
  if (FAILED(hr))
    return hr;

  CComPtr<IShellView> spView;
  hr = spBrowser->QueryActiveShellView(&spView);
  if (FAILED(hr))
    return hr;

  return spView->QueryInterface(riid, ppv);
}

HRESULT GetDesktopAutomationObject(REFIID riid, void** ppv) {
  CComPtr<IShellView> spsv;
  HRESULT hr = FindDesktopFolderView(IID_PPV_ARGS(&spsv));
  if (FAILED(hr))
    return hr;

  CComPtr<IDispatch> spdispView;
  hr = spsv->GetItemObject(SVGIO_BACKGROUND, IID_PPV_ARGS(&spdispView));
  if (FAILED(hr))
    return hr;

  return spdispView->QueryInterface(riid, ppv);
}

bool ShellExecuteFromExplorer(const base::FilePath& application_path,
                              const base::string16& parameters,
                              const base::FilePath& directory) {
  base::string16 operation = base::string16();
  int nShowCmd = SW_SHOWDEFAULT;

  CComPtr<IShellFolderViewDual> spFolderView;
  HRESULT hr = GetDesktopAutomationObject(IID_PPV_ARGS(&spFolderView));
  if (FAILED(hr))
    return false;

  CComPtr<IDispatch> spdispShell;
  hr = spFolderView->get_Application(&spdispShell);
  if (FAILED(hr))
    return false;

  AllowSetForegroundWindow(ASFW_ANY);

  hr =
      CComQIPtr<IShellDispatch2>(spdispShell)
          ->ShellExecute(CComBSTR(application_path.value().c_str()),
                         CComVariant(parameters.c_str()),
                         CComVariant(directory.value().c_str()),
                         CComVariant(operation.c_str()), CComVariant(nShowCmd));

  return (hr == S_OK);
}

std::vector<base::win::ScopedHandle> GetRunningProcessesForPath(
    const base::FilePath& path) {
  std::vector<base::win::ScopedHandle> processes;

  if (path.empty())
    return processes;
  PROCESSENTRY32 entry = {sizeof(PROCESSENTRY32)};
  base::win::ScopedHandle snapshot(
      CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
  if (!snapshot.IsValid() || Process32First(snapshot.Get(), &entry) == FALSE)
    return processes;
  DWORD current_process = GetCurrentProcessId();
  do {
    if (entry.th32ProcessID == current_process)
      continue;
    if (!base::FilePath::CompareEqualIgnoreCase(entry.szExeFile,
                                                path.BaseName().value())) {
      continue;
    }
    base::win::ScopedHandle process(OpenProcess(
        PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID));
    if (!process.IsValid())
      continue;

    // Check if process is dead already.
    if (WaitForSingleObject(process.Get(), 0) == WAIT_TIMEOUT)
      continue;

    wchar_t process_image_name[MAX_PATH];
    DWORD size = base::size(process_image_name);
    if (QueryFullProcessImageName(process.Get(), 0, process_image_name,
                                  &size) == FALSE)
      continue;
    VLOG(1) << "GetRunningProcessesForPath: process_image_name="
            << process_image_name;
    if (!base::FilePath::CompareEqualIgnoreCase(path.value(),
                                                process_image_name)) {
      continue;
    }

    processes.push_back(std::move(process));
  } while (Process32Next(snapshot.Get(), &entry) != FALSE);
  VLOG(1) << "GetRunningProcessesForPath: processes.size()="
          << processes.size();
  return processes;
}

void KillProcesses(std::vector<base::win::ScopedHandle> processes) {
  for (auto& process : processes) {
    DCHECK(process.IsValid());

    // It is necessary to reopen as we have not passed the terminate permission
    // in GetRunningProcessesForPath to ensure the maximum coverage of collected
    // processes.
    process.Set(::OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE,
                              ::GetProcessId(process.Get())));
    if (!process.IsValid())
      continue;
    ::TerminateProcess(process.Get(), 1);

    // Close no longer necessary process handle now without wating for the loop
    // to finish.
    process.Close();
  }
}

const base::FilePath& GetPathOfCurrentExe() {
  // This cannot use base::PathService::Get(base::DIR_EXE) as that calls
  // GetModuleFileName() and that does not normalize the exe path.
  static base::NoDestructor<base::FilePath> exe_path([] {
    wchar_t path[MAX_PATH];
    path[0] = L'\0';
    DWORD size = base::size(path);
    if (!::QueryFullProcessImageName(::GetCurrentProcess(), 0, path, &size)) {
      PLOG(INFO) << "Failed QueryFullProcessImageName()";
      if (GetModuleFileName(nullptr, path, base::size(path)) == 0) {
        PLOG(INFO) << "Failed QueryFullProcessImageName()";
        return base::FilePath();
      }
    }
    return base::FilePath(path);
  }());
  return *exe_path;
}

const base::FilePath& GetDirectoryOfCurrentExe() {
  static base::NoDestructor<base::FilePath> exe_dir(
      [] { return GetPathOfCurrentExe().DirName(); }());
  return *exe_dir;
}

void SendQuitUpdateNotifier(const base::FilePath* exe_dir, bool global) {
  DCHECK(!exe_dir || !exe_dir->empty());
  base::string16 event_name = GetUpdateNotifierEventName(
      global ? vivaldi_update_notifier::kGlobalQuitEventPrefix
             : vivaldi_update_notifier::kQuitEventPrefix,
      exe_dir);

  DLOG(INFO) << "Sending quit event " << event_name;
  base::win::ScopedHandle quit_event(
      ::OpenEvent(EVENT_MODIFY_STATE, FALSE, event_name.c_str()));
  if (!quit_event.IsValid()) {
    // No modifiers listen for the event
    return;
  }

  ::SetEvent(quit_event.Get());
}

namespace {

base::string16 GetUpdateNotifierAutorunCommand() {
  base::string16 command;
  if (!base::win::ReadCommandFromAutoRun(
          HKEY_CURRENT_USER, kUpdateNotifierAutorunName, &command)) {
    return base::string16();
  }
  return command;
}

}  // namespace

base::FilePath GetUpdateNotifierPath(const base::FilePath* exe_dir) {
  DCHECK(!exe_dir || !exe_dir->empty());
  if (!exe_dir) {
    exe_dir = &GetDirectoryOfCurrentExe();
  }
  return exe_dir->Append(vivaldi::constants::kVivaldiUpdateNotifierExe);
}

base::CommandLine GetCommonUpdateNotifierCommand(
    const base::FilePath* exe_dir) {
  // This must be thread-safe and non-blocking so it can be called from any
  // thread including UI thread.
  base::CommandLine command(GetUpdateNotifierPath(exe_dir));
  const base::CommandLine* vivaldi_cmd_line =
      base::CommandLine::ForCurrentProcess();
  if (vivaldi_cmd_line->HasSwitch(switches::kVivaldiUpdateURL)) {
    command.AppendSwitchNative(
        switches::kVivaldiUpdateURL,
        vivaldi_cmd_line->GetSwitchValueNative(switches::kVivaldiUpdateURL));
  }
  if (vivaldi_cmd_line->HasSwitch(installer::switches::kEnableLogging) ||
      vivaldi_cmd_line->HasSwitch(installer::switches::kVerboseLogging)) {
    // We do not copy the value here as we do not want to log to the browser log
    // file but rather always to the separated install log.
    command.AppendSwitch(installer::switches::kEnableLogging);
  }
  if (vivaldi_cmd_line->HasSwitch(switches::kVivaldiSilentUpdate)) {
    command.AppendSwitch(switches::kVivaldiSilentUpdate);
  }
  return command;
}

bool IsUpdateNotifierEnabled(const base::FilePath* exe_dir,
                             base::CommandLine* cmdline_result) {
  base::string16 command = GetUpdateNotifierAutorunCommand();
  if (command.empty())
    return false;
  base::CommandLine cmdline = base::CommandLine::FromString(command);
  base::FilePath registry_exe = cmdline.GetProgram().NormalizePathSeparators();

  base::FilePath exe_path = GetUpdateNotifierPath(exe_dir);
  if (!base::FilePath::CompareEqualIgnoreCase(registry_exe.value(),
                                              exe_path.value())) {
    return false;
  }

  if (cmdline_result) {
    // Copy only switches to avoid dummy %args, see EnableUpdateNotifier and
    // always use the canonical path from GetUpdateNotifierPath().
    cmdline_result->SetProgram(exe_path);
    for (const auto& key_value : cmdline.GetSwitches()) {
      cmdline_result->AppendSwitchNative(key_value.first, key_value.second);
    }
  }
  return true;
}

bool IsUpdateNotifierEnabledForAnyPath() {
  base::string16 command = GetUpdateNotifierAutorunCommand();
  return !command.empty();
}

bool EnableUpdateNotifier(const base::CommandLine& cmdline) {
  DCHECK(base::FilePath::CompareEqualIgnoreCase(
      cmdline.GetProgram().BaseName().value(),
      vivaldi::constants::kVivaldiUpdateNotifierExe));

  // NOTE(igor@vivaldi.com): According a stack overflow answer without further
  // references and own testing to pass command line arguments to the autorun
  // command the first argument after the program path must start with %. So
  // escape properly the exe path, then, if there are arguments, add a dummy
  // %args before them. This case does not arise normally as the arguments are
  // used for testing and debugging.
  base::string16 autocommand =
      base::CommandLine(cmdline.GetProgram()).GetCommandLineString();
  base::string16 args = cmdline.GetArgumentsString();
  if (!args.empty()) {
    autocommand += L" %args ";
    autocommand += args;
  }
  if (!base::win::AddCommandToAutoRun(HKEY_CURRENT_USER,
                                      ::vivaldi::kUpdateNotifierAutorunName,
                                      autocommand)) {
    LOG(ERROR) << "Failed base::win::AddCommandToAutoRun() for " << autocommand;
    return false;
  }
  return true;
}

bool DisableUpdateNotifier(const base::FilePath* exe_dir) {
  // Remove autorun only if it is enabled for the given exe_dir.
  if (IsUpdateNotifierEnabled(exe_dir)) {
    if (!base::win::RemoveCommandFromAutoRun(
            HKEY_CURRENT_USER, ::vivaldi::kUpdateNotifierAutorunName)) {
      LOG(ERROR) << "base::win::RemoveCommandFromAutoRun failed";
      return false;
    }
  }
  SendQuitUpdateNotifier(exe_dir, /*global=*/false);
  return true;
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
  base::TimeDelta max_wait = base::TimeDelta::FromSeconds(10);
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

base::string16 GetUpdateNotifierEventName(base::StringPiece16 event_prefix,
                                          const base::FilePath* exe_dir) {
  DCHECK(!exe_dir || !exe_dir->empty());
  if (!exe_dir) {
    exe_dir = &GetDirectoryOfCurrentExe();
  }
  base::string16 normalized_path =
      exe_dir->NormalizePathSeparatorsTo(L'/').value();
  // See
  // https://web.archive.org/web/20130528052217/http://blogs.msdn.com/b/michkap/archive/2005/10/17/481600.aspx
  ::CharUpper(&normalized_path[0]);
  base::string16 event_name(event_prefix.data(), event_prefix.length());
  event_name += normalized_path;
  return event_name;
}

base::FilePath NormalizeInstallExeDirectory(const base::FilePath& exe_dir) {
  // base::NormalizeFilePath() works only for existing files, not directories,
  // so go via an executable.
  base::FilePath exe_path = GetUpdateNotifierPath(&exe_dir);
  if (!base::NormalizeFilePath(exe_path, &exe_path)) {
    PLOG(ERROR) << "Failed to normalize " << exe_path;
  }
  return exe_path.DirName();
}

void QuitAllUpdateNotifiers(const base::FilePath& installer_exe_dir,
                            bool quit_old) {
  base::FilePath exe_dir = NormalizeInstallExeDirectory(installer_exe_dir);
  SendQuitUpdateNotifier(&exe_dir, /*global=*/false);
  SendQuitUpdateNotifier(&exe_dir, /*global=*/true);

  // Give up to 1 second for the notifiers to do a clean exit before terminating
  // the processes.
  base::FilePath exe_path =
      exe_dir.Append(quit_old ? vivaldi::constants::kVivaldiUpdateNotifierOldExe
                              : vivaldi::constants::kVivaldiUpdateNotifierExe);
  std::vector<base::win::ScopedHandle> update_notifier_processes;
  for (int i = 0; i < 10; ++i) {
    ::Sleep(100);
    update_notifier_processes = GetRunningProcessesForPath(exe_path);
    if (update_notifier_processes.empty())
      return;
  }
  LOG(INFO) << "Forcefully terminating "
            << vivaldi::constants::kVivaldiUpdateNotifierExe;
  KillProcesses(std::move(update_notifier_processes));
}

std::wstring ReadRegistryString(const wchar_t* name, base::win::RegKey& key) {
  std::wstring value;
  LSTATUS status = key.ReadValue(name, &value);
  if (status != ERROR_SUCCESS) {
    if (status != ERROR_FILE_NOT_FOUND) {
      LOG(ERROR) << base::StringPrintf(
          "Failed to read registry key %ls status==0x%lx", name, status);
    }
    return std::wstring();
  }
  if (value.empty()) {
    LOG(ERROR) << "Invalid empty string value for the registry key " << name;
    return std::wstring();
  }
  return value;
}

base::Optional<DWORD> ReadRegistryDW(const wchar_t* name,
                                     base::win::RegKey& key) {
  DWORD value = 0;
  LSTATUS status = key.ReadValueDW(name, &value);
  if (status != ERROR_SUCCESS) {
    if (status != ERROR_FILE_NOT_FOUND) {
      LOG(ERROR) << base::StringPrintf(
          "Failed to read registry key %ls status==0x%lx", name, status);
    }
    return base::nullopt;
  }
  return value;
}

base::Optional<bool> ReadRegistryBool(const wchar_t* name,
                                      base::win::RegKey& key) {
  base::Optional<DWORD> value_word = ReadRegistryDW(name, key);
  if (!value_word)
    return base::nullopt;
  if (*value_word > 1) {
    LOG(ERROR) << "Invalid boolean registry value in " << name << ": "
               << *value_word;
    return base::nullopt;
  }
  return *value_word != 0;
}

}  // namespace vivaldi
