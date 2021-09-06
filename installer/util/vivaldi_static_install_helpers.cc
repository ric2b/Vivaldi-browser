// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#include "installer/util/vivaldi_static_install_helpers.h"

#include <Windows.h>

// Only kernel.lib API are allowed here.

namespace vivaldi {

bool GetStandaloneInstallDataDirectory(std::wstring& result) {
  wchar_t exe_path[MAX_PATH] = {};
  ::GetModuleFileName(nullptr, exe_path, MAX_PATH);

  // Do not use PathRemoveFileSpec() as only kernel.lib API is allowed here.
  wchar_t* last_backslash = ::wcsrchr(exe_path, L'\\');
  if (!last_backslash)
    return false;

  *last_backslash = L'\0';
  std::wstring stp_viv_path(exe_path);
  stp_viv_path.push_back(L'\\');
  stp_viv_path.append(vivaldi::constants::kStandaloneMarkerFile);

  bool is_standalone =
      ::GetFileAttributes(stp_viv_path.c_str()) != INVALID_FILE_ATTRIBUTES;

  if (!is_standalone)
    return false;

  last_backslash = ::wcsrchr(exe_path, L'\\');
  if (!last_backslash)
    return false;

  *last_backslash = L'\0';
  result = exe_path;
  result.push_back(L'\\');
  result.append(L"User Data");
  return true;
}

bool IsSystemInstallExecutable(const std::wstring& exe_path) {
  const wchar_t* last_backslash = ::wcsrchr(exe_path.c_str(), L'\\');
  if (!last_backslash)
    return false;

  // Normally exe_path points to Application\vivaldi.exe or other files in
  // Application directory, but the notification helper is placed under
  // Application\VERSION\, so skip the version part if so.
  if (0 == ::wcsicmp(last_backslash + 1, L"notification_helper.exe")) {
    // Windows does not have wmemrchr
    const wchar_t* i = last_backslash;
    last_backslash = nullptr;
    while (i != exe_path.c_str()) {
      --i;
      if (*i == L'\\') {
        last_backslash = i;
        break;
      }
    }
    if (!last_backslash)
      return false;
  }

  // Copy directory path including the backslash.
  std::wstring marker_file_path(exe_path.c_str(),
                                last_backslash + 1 - exe_path.c_str());
  marker_file_path += vivaldi::constants::kSystemMarkerFile;
  bool is_system =
      ::GetFileAttributes(marker_file_path.c_str()) != INVALID_FILE_ATTRIBUTES;
  return is_system;
}

}  // namespace vivaldi
