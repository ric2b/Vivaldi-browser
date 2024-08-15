// Copyright (c) 2016-2019 Vivaldi Technologies AS. All rights reserved

#include "browser/win/vivaldi_utils.h"

#include <windows.h>
#include <algorithm>
#include "app/vivaldi_apptools.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/vivaldi_switches.h"
#include "base/win/windows_version.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/installer/util/util_constants.h"
#include "installer/util/vivaldi_install_util.h"

#include <tlhelp32.h>

using base::PathService;

namespace {

void KillVivaldiProcesses(std::vector<DWORD>& process_ids) {
  if (process_ids.empty())
    return;

  std::wstring cmd_line_string(L"taskkill.exe /F");
  std::vector<DWORD>::iterator it;
  for (it = process_ids.begin(); it != process_ids.end(); it++) {
    DWORD pid = *it;
    cmd_line_string.append(base::UTF8ToWide(base::StringPrintf(" /PID %lu", pid)));
  }

  std::unique_ptr<wchar_t[]> cmd_line(
      new wchar_t[cmd_line_string.length() + 1]);
  std::copy(cmd_line_string.begin(), cmd_line_string.end(), cmd_line.get());
  cmd_line[cmd_line_string.length()] = 0;

  STARTUPINFO si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};

  if (CreateProcess(NULL, cmd_line.get(), NULL, NULL, FALSE, CREATE_NO_WINDOW,
                    NULL, NULL, &si, &pi)) {
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  }
}

typedef BOOL(WINAPI* LPQueryFullProcessImageName)(HANDLE hProcess,
                                                  DWORD dwFlags,
                                                  LPWSTR lpExeName,
                                                  PDWORD lpdwSize);

static LPQueryFullProcessImageName fpQueryFullProcessImageName = NULL;

bool LoadQueryFullProcessImageNameFunc() {
  HMODULE hDLL = LoadLibrary(L"kernel32.dll");
  if (!hDLL)
    return false;

  fpQueryFullProcessImageName = (LPQueryFullProcessImageName)GetProcAddress(
      hDLL, "QueryFullProcessImageNameW");

  if (!fpQueryFullProcessImageName)
    return false;

  return true;
}

void GetRunningVivaldiProcesses(const std::wstring& path,
                                std::vector<DWORD>& process_ids) {
  if (!fpQueryFullProcessImageName) {
    if (!LoadQueryFullProcessImageNameFunc())
      return;
  }

  process_ids.clear();
  PROCESSENTRY32 entry = {sizeof(PROCESSENTRY32)};
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
  if (snapshot && Process32First(snapshot, &entry)) {
    while (Process32Next(snapshot, &entry)) {
      if (!wcsicmp(entry.szExeFile, L"vivaldi.exe")) {
        if (base::win::GetVersion() >= base::win::Version::VISTA) {
          HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE,
                                       entry.th32ProcessID);
          if (process) {
            wchar_t process_image_name[MAX_PATH] = {0};
            DWORD size = MAX_PATH;
            if (fpQueryFullProcessImageName(process, 0, process_image_name,
                                            &size)) {
              std::wstring proc_path(process_image_name);
              std::wstring::size_type pos = proc_path.rfind(L"\\vivaldi.exe");
              if (pos != std::wstring::npos)
                proc_path = proc_path.substr(0, pos);

              if (proc_path != path ||
                  GetCurrentProcessId() == entry.th32ProcessID) {
                CloseHandle(process);
                continue;
              }
            }
            CloseHandle(process);
          }
        }
        process_ids.push_back(entry.th32ProcessID);
      }
    }
  }
  if (snapshot)
    CloseHandle(snapshot);
}

}  // namespace

namespace vivaldi {

bool GetVivaldiStandaloneUserDataDirectory(base::FilePath* result) {
  // Allow IO temporarily, since this call will come before UI is displayed.
  base::VivaldiScopedAllowBlocking allow_blocking;

  if (!vivaldi::IsVivaldiRunning())
    return false;

  // Check if we are launched with the --vivaldi-standalone switch.
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  bool is_standalone = command_line.HasSwitch(constants::kVivaldiStandalone);

  // Check if the magic file exists. If so we are standalone.
  base::FilePath exe_file_path;
  PathService::Get(base::DIR_EXE, &exe_file_path);
  is_standalone =
      is_standalone ||
      base::PathExists(exe_file_path.Append(constants::kStandaloneMarkerFile));

  if (is_standalone && result) {
    *result = base::MakeAbsoluteFilePath(
        exe_file_path.Append(L"..").Append(chrome::kUserDataDirname));
  }
  // Make sure the --vivaldi-standalone switch is set. Code shared with the
  // installer depends on it, i.e. prog-id suffix generation.
  if (is_standalone)
    vivaldi::CommandLineAppendSwitchNoDup(
        command_line, vivaldi::constants::kVivaldiStandalone);

  return is_standalone;
}

bool IsStandalone() {
  return GetVivaldiStandaloneUserDataDirectory(nullptr);
}

namespace {
std::wstring GenerateExitMutexName() {
  base::FilePath exe_path;
  PathService::Get(base::FILE_EXE, &exe_path);
  std::wstring exe = exe_path.value();
  std::replace(exe.begin(), exe.end(), '\\', '!');
  std::transform(exe.begin(), exe.end(), exe.begin(), tolower);
  exe = L"Global\\" + exe + L"-Exiting";

  return exe;
}
}  // namespace

bool IsVivaldiExiting() {
  std::wstring exe = GenerateExitMutexName();
  HANDLE handle = OpenEvent(SYNCHRONIZE | READ_CONTROL, FALSE, exe.c_str());
  bool exiting = (handle != NULL);
  if (handle != NULL) {
    CloseHandle(handle);
  }
  return exiting;
}

void SetVivaldiExiting() {
  if (!IsVivaldiExiting()) {
    std::wstring exe = GenerateExitMutexName();
    HANDLE handle = CreateEvent(NULL, TRUE, TRUE, exe.c_str());
    int error = GetLastError();
    DCHECK(handle != NULL);
    DCHECK(error != ERROR_ALREADY_EXISTS && error != ERROR_ACCESS_DENIED);
  }
}

void OnShutdownStarted() {
  vivaldi::SetVivaldiExiting();

  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kTestAlreadyRunningDialog)) {
    // Add an artificial 15s delay here for testing purposes.
    Sleep(1000 * 15);
  }
}

void AttemptToKillTheUndead() {
  base::FilePath exe_path;
  base::PathService::Get(base::DIR_EXE, &exe_path);

  std::vector<DWORD> process_ids;
  GetRunningVivaldiProcesses(exe_path.value(), process_ids);
  if (!process_ids.empty())
    KillVivaldiProcesses(process_ids);
}

}  // namespace vivaldi
