// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.

#include "chrome/installer/util/install_util.h"
#include "installer/util/vivaldi_install_util.h"

#include <atlbase.h>
#include <exdisp.h>
#include <shellapi.h>
#include <shldisp.h>
#include <shlobj.h>
#include <stdlib.h>
#include <tlhelp32.h>

#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/wmi.h"

using base::PathService;

namespace vivaldi {

namespace constants {
const wchar_t kVivaldiAutoUpdate[] = L"AutoUpdate";
const wchar_t kVivaldiDeltaPatchFailed[] = L"DeltaPatchFailed";
const wchar_t kVivaldiKey[] = L"Software\\Vivaldi";
const wchar_t kVivaldiPinToTaskbarValue[] = L"EnablePinToTaskbar";

// Vivaldi installer settings from last install.
const wchar_t kVivaldiInstallerDestinationFolder[] = L"DestinationFolder";
const wchar_t kVivaldiInstallerInstallType[] = L"InstallType";
const wchar_t kVivaldiInstallerDefaultBrowser[] = L"DefaultBrowser";
const wchar_t kVivaldiInstallerRegisterBrowser[] = L"RegisterBrowser";
const wchar_t kVivaldiInstallerAdvancedMode[] = L"AdvancedMode";

// Vivaldi paths and filenames
const wchar_t kVivaldiUpdateNotifierExe[] = L"update_notifier.exe";
const wchar_t kVivaldiUpdateNotifierOldExe[] = L"update_notifier.old";

// Vivaldi installer command line switches.
const char kVivaldi[] = "vivaldi";
const char kVivaldiInstallDir[] = "vivaldi-install-dir";
const char kVivaldiStandalone[] = "vivaldi-standalone";
const char kVivaldiForceLaunch[] = "vivaldi-force-launch";
const char kVivaldiUpdate[] = "vivaldi-update";
const char kVivaldiRegisterStandalone[] = "vivaldi-register-standalone";
const char kVivaldiSilent[] = "vivaldi-silent";

} // namespace constants

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
  installer::WMIProcess::Launch(command, &pid);
}

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

  if (os_info->version() >= base::win::VERSION_WIN10 &&
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
                              const base::FilePath& directory,
                              const base::string16& operation,
                              int nShowCmd) {
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

base::string16 GetNewFeaturesUrl(const base::Version& version) {
  const base::string16 kVersionParam = L"version";
  const base::string16 kOSParam = L"os";
  const wchar_t kBaseUrl[] = L"https://vivaldi.com/newfeatures?hl=$1";

  const base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  base::win::OSInfo::VersionNumber version_number = os_info->version_number();
  base::string16 os_version =
      base::StringPrintf(L"W%d.%d.%d", version_number.major,
                         version_number.minor, version_number.build);

  base::string16 url = LocalizeUrl(kBaseUrl);
  url = url + L"&" + kVersionParam + L"=" +
        base::ASCIIToUTF16(version.GetString()) + L"&" + kOSParam + L"=" +
        os_version;
  return url;
}

std::vector<base::win::ScopedHandle> GetRunningProcessesForPath(
  const base::FilePath& path) {
  std::vector<base::win::ScopedHandle> processes;

  if (path.empty())
    return processes;
  VLOG(1) << "GetRunningProcessesForPath: path=" << path.value();
  PROCESSENTRY32 entry = { sizeof(PROCESSENTRY32) };
  base::win::ScopedHandle snapshot(
    CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
  if (!snapshot.IsValid() || Process32First(snapshot.Get(), &entry) == FALSE)
    return processes;
  do {
    if (!base::FilePath::CompareEqualIgnoreCase(
      entry.szExeFile, path.BaseName().value()))  // vivaldi.exe
      continue;
    base::win::ScopedHandle process(
      OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
        entry.th32ProcessID));
    if (!process.IsValid())
      continue;

    // Check if process is dead already.
    if (WaitForSingleObject(process.Get(), 0) == WAIT_TIMEOUT)
      continue;

    wchar_t process_image_name[MAX_PATH];
    DWORD size = arraysize(process_image_name);
    if (QueryFullProcessImageName(process.Get(), 0, process_image_name,
      &size) == FALSE)
      continue;
    VLOG(1) << "GetRunningProcessesForPath: process_image_name=" << process_image_name;
    if (!base::FilePath::CompareEqualIgnoreCase(path.value(),
      process_image_name))
      continue;

    processes.push_back(std::move(process));
  } while (Process32Next(snapshot.Get(), &entry) != FALSE);
  VLOG(1) << "GetRunningProcessesForPath: processes.size()=" << processes.size();
  return processes;
}

void KillProcesses(const std::vector<base::win::ScopedHandle>& processes) {
  base::string16 cmd_line_string(L"taskkill.exe /F");
  std::vector<DWORD>::iterator it;
  for (auto& process : processes) {
    DCHECK(process.IsValid());
    cmd_line_string +=
      base::StringPrintf(L" /PID %u", GetProcessId(process.Get()));
  }

  WCHAR* cmd_line = new WCHAR[cmd_line_string.length() + 1];
  std::copy(cmd_line_string.begin(), cmd_line_string.end(), cmd_line);
  cmd_line[cmd_line_string.length()] = 0;

  STARTUPINFO si = { sizeof(si) };
  PROCESS_INFORMATION pi = { 0 };

  if (CreateProcess(NULL, cmd_line, NULL, NULL, FALSE,
    CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  }
  delete[] cmd_line;
}

}  // namespace vivaldi

// static
void InstallUtil::ShowInstallerResultMessage(installer::InstallStatus status, int string_resource_id) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(vivaldi::constants::kVivaldi) &&
    !base::CommandLine::ForCurrentProcess()->HasSwitch(installer::switches::kUninstall)) {
    base::string16 msg = installer::GetLocalizedString(string_resource_id);
    MessageBox(NULL, msg.c_str(), NULL, MB_ICONINFORMATION | MB_SETFOREGROUND);
  }
}
