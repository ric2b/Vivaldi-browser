/*
 *  This file is part of WinSparkle (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2016 Vaclav Slavik
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files (the "Software"),
 *  to deal in the Software without restriction, including without limitation
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 *
 */

#include "update_notifier/thirdparty/winsparkle/src/settings.h"

#include "installer/util/vivaldi_install_constants.h"
#include "update_notifier/thirdparty/winsparkle/src/config.h"
#include "update_notifier/thirdparty/winsparkle/src/error.h"

#include <Windows.h>
#include "base/check.h"
#include "base/logging.h"
#include "base/notreached.h"

namespace winsparkle {

/*--------------------------------------------------------------------------*
                             resources access
 *--------------------------------------------------------------------------*/

namespace {

const wchar_t* GetRegistryName(ConfigKey key) {
  switch (key) {
    case ConfigKey::kDeltaPatchFailed:
      return vivaldi::constants::kVivaldiDeltaPatchFailed;
    case ConfigKey::kLastCheckTime:
      return L"LastCheckTime";
    case ConfigKey::kSkipThisVersion:
      return L"SkipThisVersion";
  }
  NOTREACHED();
  return nullptr;
}

const wchar_t* GetKeyPath() {
  return GetConfig().registry_path.c_str();
}

}  // anonymous namespace

/*--------------------------------------------------------------------------*
                             runtime config access
 *--------------------------------------------------------------------------*/

void Settings::WriteConfigValue(ConfigKey config_key,
                                const std::wstring& value) {
  HKEY key;
  LONG result =
      RegCreateKeyEx(HKEY_CURRENT_USER, GetKeyPath(), 0, NULL,
                     REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &key, NULL);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Cannot write settings to registry";
    return;
  }

  result = RegSetValueEx(key, GetRegistryName(config_key), 0, REG_SZ,
                         (const BYTE*)value.c_str(),
                         (value.length() + 1) * sizeof(wchar_t));

  RegCloseKey(key);

  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Cannot write settings to registry";
  }
}

void Settings::DeleteConfigValue(ConfigKey config_key) {
  HKEY key;
  LONG result =
      RegOpenKeyEx(HKEY_CURRENT_USER, GetKeyPath(), 0, KEY_SET_VALUE, &key);
  if (result != ERROR_SUCCESS) {
    if (result != ERROR_FILE_NOT_FOUND) {
      LOG(ERROR) << "Cannot delete settings from registry";
    }
    return;
  }

  result = RegDeleteValue(key, GetRegistryName(config_key));

  RegCloseKey(key);

  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Cannot delete settings from registry";
  }
}

namespace {

bool DoRegistryRead(HKEY root, ConfigKey config_key, wchar_t* buf, size_t len) {
  HKEY key;
  LONG result = RegOpenKeyEx(root, GetKeyPath(), 0, KEY_QUERY_VALUE, &key);
  if (result != ERROR_SUCCESS)
    return false;

  DWORD buflen = len;
  DWORD type;
  result = RegQueryValueEx(key, GetRegistryName(config_key), 0, &type,
                           (BYTE*)buf, &buflen);

  RegCloseKey(key);

  if (result != ERROR_SUCCESS)
    return false;

  if (type != REG_SZ) {
    // incorrect type -- pretend that the setting doesn't exist, it will
    // be newly written by WinSparkle anyway
    return false;
  }

  return true;
}

}  // anonymous namespace

std::wstring Settings::ReadConfigValueW(ConfigKey key) {
  wchar_t buf[512];

  // Try reading from HKCU first. If that fails, look at HKLM too, in case
  // some settings have globally set values (either by the installer or the
  // administrator).
  if (!DoRegistryRead(HKEY_CURRENT_USER, key, buf, sizeof buf)) {
    if (!DoRegistryRead(HKEY_LOCAL_MACHINE, key, buf, sizeof buf))
      return std::wstring();
  }
  return buf;
}

}  // namespace winsparkle
