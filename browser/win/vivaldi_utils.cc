// Copyright (c) 2016-2018 Vivaldi Technologies AS. All rights reserved

#include "browser/win/vivaldi_utils.h"

#include <algorithm>
#include <windows.h>
#include "app/vivaldi_apptools.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "base/vivaldi_switches.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/installer/util/util_constants.h"

#include "installer/util/vivaldi_install_util.h"

using base::PathService;

namespace vivaldi {

bool GetVivaldiStandaloneUserDataDirectory(base::FilePath* result) {
  const wchar_t kStandaloneProfileHelper[] = L"stp.viv";

  // Allow IO temporarily, since this call will come before UI is displayed.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  if (!vivaldi::IsVivaldiRunning())
    return false;

  // Check if we are launched with the --vivaldi-standalone switch.
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  bool is_standalone =
      command_line.HasSwitch(vivaldi::constants::kVivaldiStandalone);

  // Check if the magic file exists. If so we are standalone.
  base::FilePath exe_file_path;
  PathService::Get(base::DIR_EXE, &exe_file_path);
  is_standalone =
      is_standalone ||
      base::PathExists(exe_file_path.Append(kStandaloneProfileHelper));

  if (is_standalone && result) {
    *result = base::MakeAbsoluteFilePath(
        exe_file_path.Append(L"..").Append(chrome::kUserDataDirname));
  }
  // Make sure the --vivaldi-standalone switch is set. Code shared with the
  // installer depends on it, i.e. prog-id suffix generation.
  if (is_standalone)
    vivaldi::CommandLineAppendSwitchNoDup(
        command_line,vivaldi::constants::kVivaldiStandalone);

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

}  // namespace vivaldi
