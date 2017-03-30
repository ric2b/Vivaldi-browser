// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "browser/win/vivaldi_standalone.h"

#include "app/vivaldi_apptools.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/common/chrome_constants.h"

namespace vivaldi {

#if defined(OS_WIN)
bool GetVivaldiStandaloneUserDataDirectory(base::FilePath* result) {
  const wchar_t kStandaloneProfileHelper[] = L"stp.viv";

  if (!vivaldi::IsVivaldiRunning())
    return false;

  // Allow IO temporarily, since this call will come before UI is displayed.
  bool last_ioallow = base::ThreadRestrictions::SetIOAllowed(true);

  // Check if the magic file exists. If so we are standalone.
  base::FilePath exe_file_path;
  PathService::Get(base::DIR_EXE, &exe_file_path);
  bool is_standalone =
      base::PathExists(exe_file_path.Append(kStandaloneProfileHelper));

  if (is_standalone && result)
    *result = base::MakeAbsoluteFilePath(
      exe_file_path.Append(L"..").Append(chrome::kUserDataDirname));

  // Set restrictions back to previous setting.
  base::ThreadRestrictions::SetIOAllowed(last_ioallow);
  return is_standalone;
}

bool IsStandalone() {
  return GetVivaldiStandaloneUserDataDirectory(nullptr);
}
#endif

}  // namespace vivaldi
