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

#ifndef UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_SETTINGS_H_
#define UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_SETTINGS_H_

#include "base/strings/utf_string_conversions.h"

namespace winsparkle {

// Access to runtime configuration.
// This is stored in registry, under HKCU\Software\...\...\WinSparkle.

enum class ConfigKey {
  kDeltaPatchFailed,
  kLastCheckTime,
  kSkipThisVersion,
};

class Settings {
 public:
  static void WriteConfigValue(ConfigKey key, const std::string& value) {
    WriteConfigValue(key, base::UTF8ToUTF16(value).c_str());
  }

  static void WriteConfigValue(ConfigKey key, const std::wstring& value);

  static std::string ReadConfigValue(ConfigKey key) {
    const std::wstring v = ReadConfigValueW(key);
    return base::UTF16ToUTF8(v);
  }

  static std::wstring ReadConfigValueW(ConfigKey key);

  // Deletes value from registry.
  static void DeleteConfigValue(ConfigKey key);
};

}  // namespace winsparkle

#endif  // UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_SETTINGS_H_
