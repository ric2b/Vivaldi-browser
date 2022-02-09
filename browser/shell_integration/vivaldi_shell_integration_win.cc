// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "browser/shell_integration/vivaldi_shell_integration.h"

#include <windows.h>
#include <string>

#include "base/strings/string_util.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/installer/util/shell_util.h"

namespace vivaldi {

// There is no reliable way to say which browser is default on a machine (each
// browser can have some of the protocols/shortcuts). So we look for only HTTP
// protocol handler. Even this handler is located at different places in
// registry on XP and Vista:
// - HKCR\http\shell\open\command (XP)
// - HKCU\Software\Microsoft\Windows\Shell\Associations\UrlAssociations\
//   http\UserChoice (Vista)
// This method checks if Chrome is default browser by checking these
// locations and returns true if Chrome traces are found there. In case of
// error (or if Chrome is not found) it returns the default value which
// is false.
bool IsChromeDefaultBrowser() {
  bool chrome_default = false;
  if (base::win::GetVersion() >= base::win::Version::VISTA) {
    std::wstring app_cmd;
    base::win::RegKey key(HKEY_CURRENT_USER, ShellUtil::kRegVistaUrlPrefs,
                          KEY_READ);
    if (key.Valid() && (key.ReadValue(L"Progid", &app_cmd) == ERROR_SUCCESS) &&
        app_cmd == L"chrome")
      chrome_default = true;
  } else {
    std::wstring key_path(L"http");
    key_path.append(ShellUtil::kRegShellOpen);
    base::win::RegKey key(HKEY_CLASSES_ROOT, key_path.c_str(), KEY_READ);
    std::wstring app_cmd;
    if (key.Valid() && (key.ReadValue(L"", &app_cmd) == ERROR_SUCCESS) &&
        std::u16string::npos != base::ToLowerASCII(app_cmd).find(L"chrome"))
      chrome_default = true;
  }
  return chrome_default;
}

// There is no reliable way to say which browser is default on a machine (each
// browser can have some of the protocols/shortcuts). So we look for only HTTP
// protocol handler. Even this handler is located at different places in
// registry on XP and Vista:
// - HKCR\http\shell\open\command (XP)
// - HKCU\Software\Microsoft\Windows\Shell\Associations\UrlAssociations\
//   http\UserChoice (Vista)
// This method checks if Opera is defualt browser by checking these
// locations and returns true if Firefox traces are found there. In case of
// error (or if Opera is not found)it returns the default value which
// is false.
bool IsOperaDefaultBrowser() {
  bool opera_default = false;
  if (base::win::GetVersion() >= base::win::Version::VISTA) {
    std::wstring app_cmd;
    base::win::RegKey key(HKEY_CURRENT_USER, ShellUtil::kRegVistaUrlPrefs,
                          KEY_READ);
    if (key.Valid() && (key.ReadValue(L"Progid", &app_cmd) == ERROR_SUCCESS) &&
        std::u16string::npos != base::ToLowerASCII(app_cmd).find(L"opera"))
      opera_default = true;
  } else {
    std::wstring key_path(L"http");
    key_path.append(ShellUtil::kRegShellOpen);
    base::win::RegKey key(HKEY_CLASSES_ROOT, key_path.c_str(), KEY_READ);
    std::wstring app_cmd;
    if (key.Valid() && (key.ReadValue(L"", &app_cmd) == ERROR_SUCCESS) &&
        std::u16string::npos != base::ToLowerASCII(app_cmd).find(L"opera"))
      opera_default = true;
  }
  return opera_default;
}

}  //  namespace vivaldi
