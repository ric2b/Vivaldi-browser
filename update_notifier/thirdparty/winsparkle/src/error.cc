// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "update_notifier/thirdparty/winsparkle/src/error.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"

#include <Windows.h>
#include <string>

namespace vivaldi_update_notifier {

/*--------------------------------------------------------------------------*
                                 Helpers
 *--------------------------------------------------------------------------*/

namespace {

std::string GetWin32ErrorMessage(const char* api_function,
                                 DWORD win32_error_code,
                                 std::string message) {
  if (!message.empty()) {
    message += ": ";
  }
  message += "Windows reported the error ";
  message += std::to_string(win32_error_code);
  if (api_function) {
    message += " from ";
    message += api_function;
    message += "()";
  }

  // This is not a user-facing message so always use english.
  DWORD lang_id = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
  LPWSTR buf = nullptr;
  DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                               FORMAT_MESSAGE_FROM_SYSTEM |
                               FORMAT_MESSAGE_IGNORE_INSERTS,
                           nullptr, win32_error_code, lang_id,
                           reinterpret_cast<LPWSTR>(&buf), 0, nullptr);
  if (n != 0) {
    message += " - ";
    message += base::WideToUTF8(std::wstring_view(buf, n));
    LocalFree(buf);
  }

  return message;
}

}  // anonymous namespace

std::string LastWin32Error(const char* api_function, std::string message) {
  DWORD win32_error_code = GetLastError();
  return GetWin32ErrorMessage(api_function, win32_error_code,
                              std::move(message));
}

std::string Error::log_message() const {
  return "error_kind=" + std::to_string(kind()) + " " + message();
}

}  // namespace vivaldi_update_notifier
