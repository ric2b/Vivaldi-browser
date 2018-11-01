// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "browser/win/vivaldi_standalone.h"

#include "app/vivaldi_apptools.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/installer/util/util_constants.h"

namespace vivaldi {

#if defined(OS_WIN)
bool GetVivaldiStandaloneUserDataDirectory(base::FilePath* result) {
  const wchar_t kStandaloneProfileHelper[] = L"stp.viv";

  // Allow IO temporarily, since this call will come before UI is displayed.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  if (!vivaldi::IsVivaldiRunning())
    return false;

  // Check if we are launched with the --vivaldi-standalone switch.
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  bool is_standalone =
      command_line.HasSwitch(installer::switches::kVivaldiStandalone);

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
        command_line, installer::switches::kVivaldiStandalone);

  return is_standalone;
}

bool IsStandalone() {
  return GetVivaldiStandaloneUserDataDirectory(nullptr);
}
#endif

}  // namespace vivaldi
