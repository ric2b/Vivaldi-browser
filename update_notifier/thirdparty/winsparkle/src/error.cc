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

#include "update_notifier/thirdparty/winsparkle/src/error.h"

#include <windows.h>
#include <string>

namespace winsparkle {

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
  message += "Windows reported the error";
  if (api_function) {
    message += " from ";
    message += api_function;
    message += "()";
  }

  LPSTR buf;
  bool ok = FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                0,  // source - not set
                win32_error_code,
                0,  // language - use best
                (LPSTR)&buf, 0, NULL) != 0;

  if (ok) {
    message += " - ";
    message += buf;
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

/*--------------------------------------------------------------------------*
                                 Logging
 *--------------------------------------------------------------------------*/

void LogError(const char* msg) {
  std::string err("WinSparkle: ");
  err.append(msg);
  err.append("\n");

  OutputDebugStringA(err.c_str());
}

}  // namespace winsparkle
