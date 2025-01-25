// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#include "installer/util/vivaldi_setup_util.h"

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_util_win.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "base/win/wmi.h"
#include "chrome/installer/setup/install_params.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/util/initial_preferences.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/logging_installer.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/work_item_list.h"

#include "base/vivaldi_switches.h"
#include "components/version_info/version_info_values.h"
#include "installer/util/marker_file_work_item.h"
#include "installer/util/vivaldi_install_dialog.h"
#include "installer/util/vivaldi_install_util.h"
#include "installer/util/vivaldi_progress_dialog.h"
#include "installer/win/vivaldi_install_l10n.h"
#include "update_notifier/update_notifier_switches.h"

#include <Windows.h>

#include <CommCtrl.h>
#include <atlbase.h>
#include <exdisp.h>
#include <shellapi.h>
#include <shldisp.h>
#include <shlobj.h>
#include <stdlib.h>
#include <tlhelp32.h>

#pragma comment(lib, "comctl32.lib")

using vivaldi_installer::GetLocalizedStringF;

namespace vivaldi {

namespace {

// Registry name for older autorun-based update notifier.
const wchar_t kUpdateNotifierOldAutorunName[] = L"Vivaldi Update Notifier";

bool g_start_browser_after_install = false;
bool g_silent_install = false;

constexpr base::win::i18n::LanguageSelector::LangToOffset
    kLanguageOffsetPairs[] = {
#define HANDLE_LANGUAGE(l_, o_) {L## #l_, o_},
        DO_LANGUAGES
#undef HANDLE_LANGUAGE
};

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

// Some of the following code is borrowed from:
// installer\util\google_chrome_distribution.cc
std::wstring LocalizeUrl(const wchar_t* url) {
  std::wstring lang = installer::GetCurrentTranslation();
  return base::ReplaceStringPlaceholders(std::wstring_view(url), {lang}, NULL);
}

std::wstring GetUninstallSurveyUrl() {
  return LocalizeUrl(vivaldi::constants::kUninstallSurveyUrl);
}

bool NavigateToUrlWithEdge(const std::wstring& url) {
  std::wstring protocol_url = L"microsoft-edge:" + url;
  SHELLEXECUTEINFO info = {sizeof(info)};
  info.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
  info.lpVerb = L"open";
  info.lpFile = protocol_url.c_str();
  info.nShow = SW_SHOWNORMAL;
  if (::ShellExecuteEx(&info))
    return true;

  return false;
}

void NavigateToUrlWithIExplore(const std::wstring& url) {
  base::FilePath iexplore;
  if (!base::PathService::Get(base::DIR_PROGRAM_FILES, &iexplore))
    return;

  iexplore = iexplore.AppendASCII("Internet Explorer");
  iexplore = iexplore.AppendASCII("iexplore.exe");

  std::wstring command = L"\"" + iexplore.value() + L"\" " + url;

  int pid = 0;
  // The reason we use WMI to launch the process is because the uninstall
  // process runs inside a Job object controlled by the shell. As long as there
  // are processes running, the shell will not close the uninstall applet. WMI
  // allows us to escape from the Job object so the applet will close.
  base::win::WmiLaunchProcess(command, &pid);
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

// Launches process non-elevated even if the caller is elevated,
// using explorer. Reference:
// https://blogs.msdn.microsoft.com/oldnewthing/20131118-00/?p=2643
bool ShellExecuteFromExplorer(const base::FilePath& application_path,
                              const std::wstring& parameters,
                              const base::FilePath& directory) {
  std::wstring operation = std::wstring();
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
    DWORD size = std::size(process_image_name);
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

  // NOTE(andre@vivaldi.com) : 20/12-2024.
  // Start using a clean shutdown instead of process kill if we update a version
  // that has support for switches::kCleanShutdown. This assume that we do not
  // backport this snippet ofcourse.
  base::Version old_running_version = GetInstallVersion(installer_state.target_path());

  LOG(ERROR) << "Running " << installer::kChromeExe
             << " has version : " << old_running_version;

  // Todays(10.01.25) main branch is on 7.1.3572
  if (old_running_version >= base::Version("7.1.3572.1")) {
    LOG(ERROR)<< "Using kCleanShutdown";
    base::CommandLine cmdline =
        base::CommandLine::FromString(::GetCommandLineW());
    cmdline.SetProgram(vivaldi_exe_path);
    cmdline.AppendSwitch(switches::kCleanShutdown);
    base::LaunchProcess(cmdline, base::LaunchOptions());
  } else {
    LOG(ERROR)<< "Using KillProcesses";
    // This will cause kSessionExitType profile.exit_type to be set to "Crashed".
    KillProcesses(std::move(vivaldi_processes));
  }

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
  const wchar_t* subkey = vivaldi::constants::kVivaldiAutoUpdateKey;
  base::win::RegKey key(HKEY_CURRENT_USER, subkey, KEY_ALL_ACCESS);
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

#if !defined(OFFICIAL_BUILD)
void CheckForDebugSetupCommand(int show_command) {
  std::string debug_setup;
  base::Environment::Create()->GetVar(
      vivaldi::constants::kDebugSetupCommandEnvironment, &debug_setup);
  if (debug_setup.empty())
    return;
  base::CommandLine debug_cmdline =
      base::CommandLine::FromString(base::UTF8ToWide(debug_setup));
  // Check if setup.exe is already the debug one.
  base::FilePath debug_exe = debug_cmdline.GetProgram();
  base::NormalizeFilePath(debug_exe, &debug_exe);
  if (base::FilePath::CompareEqualIgnoreCase(debug_exe.value(),
                                             GetPathOfCurrentExe().value())) {
    return;
  }

  // We are called very early befor the global CommandLine instance is
  // initialized, so do not use CommandLine::ForCurrentProcess().
  base::CommandLine cmdline =
      base::CommandLine::FromString(::GetCommandLineW());
  // The the debug exe about the original one.
  cmdline.AppendSwitchPath(vivaldi::constants::kVivaldiDebugTargetExe,
                           cmdline.GetProgram());
  cmdline.SetProgram(debug_exe);
  cmdline.AppendArguments(debug_cmdline, /*include_program=*/false);

  // Always log verbosely with debug.
  cmdline.AppendSwitch(installer::switches::kVerboseLogging);

  base::LaunchOptions options;
  options.wait = true;
  options.start_hidden = (show_command == SW_HIDE);

  // Remove kDebugSetupCommandEnvironment for the child process.
  options.environment.emplace(
      base::UTF8ToWide(vivaldi::constants::kDebugSetupCommandEnvironment),
      std::wstring());

  base::Process process = base::LaunchProcess(cmdline, options);
  DWORD exit_code = 255;
  if (process.IsValid()) {
    ::GetExitCodeProcess(process.Handle(), &exit_code);
  }
  ::ExitProcess(exit_code);
}
#endif

bool PrepareSetupConfig(HINSTANCE instance) {
  DCHECK(g_inside_installer_application);

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

  InitInstallerLanguage(kLanguageOffsetPairs);

  g_silent_install = cmd_line.HasSwitch(vivaldi::constants::kVivaldiSilent);
  bool is_update = cmd_line.HasSwitch(vivaldi::constants::kVivaldiUpdate);
  g_start_browser_after_install =
      !cmd_line.HasSwitch(installer::switches::kDoNotLaunchChrome);
  bool is_silent_update = cmd_line.HasSwitch(switches::kVivaldiSilentUpdate);
  if (is_silent_update && is_update) {
    // --vsu without --vivaldi-update means to run installation normally, but
    // make the future update silent.
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
  std::optional<vivaldi::InstallType> existing_install_type =
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
    installer::VivaldiProgressDialog::ShowModeless(instance);
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
  if (installer_state.operation() == installer::InstallerState::UNINSTALL)
    return;
  if (!g_silent_install) {
    int return_code = InstallUtil::GetInstallReturnCode(install_status);
    if (return_code == 0) {
      // Show the progress briefly at 100% level for better perception as we
      // never call SetProgress() during the install.
      installer::VivaldiProgressDialog::SetProgress(100);
      ::Sleep(1000);
    }
    installer::VivaldiProgressDialog::Finish();
  }
}

namespace {

// For the installer installer_exe_dir comes from the user input, not
// GetDirectoryOfCurrentExe(), and may contain symlinks etc. Thus we must
// normalize it as we use the path to construct signal names and compare with
// the path in the registry for autostart.
base::FilePath NormalizeInstallExeDirectory(const base::FilePath& exe_dir) {
  // base::NormalizeFilePath() works only for existing files, not directories,
  // so go via an executable.
  base::FilePath exe_path = GetUpdateNotifierPath(exe_dir);
  if (!base::NormalizeFilePath(exe_path, &exe_path)) {
    PLOG(ERROR) << "Failed to normalize " << exe_path;
  }
  return exe_path.DirName();
}

void QuitAllUpdateNotifiers(const base::FilePath& installer_exe_dir,
                            bool quit_old) {
  base::FilePath exe_dir = NormalizeInstallExeDirectory(installer_exe_dir);
  SendQuitUpdateNotifier(exe_dir, /*global=*/false);
  SendQuitUpdateNotifier(exe_dir, /*global=*/true);

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

void RestartUpdateNotifier(const installer::InstallerState& installer_state) {
  base::FilePath exe_dir =
      NormalizeInstallExeDirectory(installer_state.target_path());

  if (IsInstallUpdate()) {
    // At this point the running update notifier was renamed to the old name.
    QuitAllUpdateNotifiers(exe_dir, /*quit_old=*/true);
  }

  // Remove an older autorun entry registry entry if any.
  base::win::RemoveCommandFromAutoRun(HKEY_CURRENT_USER,
                                      ::vivaldi::kUpdateNotifierOldAutorunName);

  if (IsInstallStandalone()) {
    // An update check for a standalone install is always run by the browser.
    return;
  }

  if (!IsInstallUpdate()) {
    // As this is a new installation, there should be no any update notification
    // task for the installation path. Running the browser for the first time
    // will create it. But if we are running with administrative privileges, we
    // want to remove any existing update task to ensure a clean start in case
    // the user created such task accidentally via running a Vivaldi installer
    // or browser with administrative privileges. Such task cannot be altered
    // when running as normal user without UAC, see VB-83328.
    if (::IsUserAnAdmin()) {
      base::CommandLine update_notifier_command =
          GetCommonUpdateNotifierCommand(exe_dir);
      update_notifier_command.AppendSwitch(
          vivaldi_update_notifier::kUnregister);
      LaunchNotifierProcess(update_notifier_command);
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
          std::wstring message(base::UTF8ToWide(base::StringPrintf(
              "Failed to rename 'Profile' folder. Error=%lu", error)));
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
    vivaldi::ShellExecuteFromExplorer(vivaldi_path, std::wstring(),
                                      base::FilePath());
  }
}

void AddVivaldiSpecificWorkItems(const installer::InstallParams& install_params,
                                 WorkItemList* install_list) {
  if (!installer::kVivaldi)
    return;

  const installer::InstallerState& installer_state =
      *install_params.installer_state;
  const base::FilePath& src_path = *install_params.src_path;
  const base::FilePath& temp_path = *install_params.temp_path;
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

void DoPostUninstallOperations(const base::Version& version) {
  // Add the Vivaldi version and OS version as params to the form.
  const std::wstring kVersionParam = L"version";
  const std::wstring kOSParam = L"os";

  const base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  base::win::OSInfo::VersionNumber version_number = os_info->version_number();
  std::wstring os_version = base::UTF8ToWide(
      base::StringPrintf("W%d.%d.%d", version_number.major,
                         version_number.minor, version_number.build));

  std::wstring url = GetUninstallSurveyUrl() + L"&" + kVersionParam + L"=" +
                     base::ASCIIToWide(version.GetString()) + L"&" + kOSParam +
                     L"=" + os_version;

  if (os_info->version() >= base::win::Version::WIN10 &&
      NavigateToUrlWithEdge(url)) {
    return;
  }
  NavigateToUrlWithIExplore(url);
}

void UnregisterUpdateNotifier(
    const installer::InstallerState& installer_state) {
  base::CommandLine update_notifier_command =
      ::vivaldi::GetCommonUpdateNotifierCommand(installer_state.target_path());
  update_notifier_command.AppendSwitch(vivaldi_update_notifier::kUnregister);
  int exit_code = ::vivaldi::RunNotifierSubaction(update_notifier_command);
  if (exit_code != 0) {
    LOG(ERROR) << "Failed to unregister the update notifier, exit_code="
               << exit_code;
  }
  QuitAllUpdateNotifiers(installer_state.target_path(), /*quit_old=*/false);
}

void ShowInstallerResultMessage(int string_resource_id) {
  std::wstring msg = installer::GetLocalizedString(string_resource_id);
  LOG(ERROR) << msg;
  if (!g_silent_install) {
    ::MessageBox(nullptr, msg.c_str(), nullptr,
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
