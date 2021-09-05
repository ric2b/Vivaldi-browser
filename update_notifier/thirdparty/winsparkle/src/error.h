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

#ifndef UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_ERROR_H_
#define UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_ERROR_H_

#include <string>
#include "base/check.h"

namespace winsparkle {
/// Helper to replace exceptions.
class Error {
 public:
  enum Kind {
    kNone,

    // Execution was cancelled by an external request.
    kCancelled,

    // Invalid or unsuitable data format
    kFormat,

    // Storage-related error
    kStorage,

    // Network-related error
    kNetwork,

    // Failed to execute external program
    kExec,

    // Failed to verify a signature
    kVerify,
  };

  Error() = default;
  explicit Error(Kind kind, std::string message = std::string()) {
    set(kind, std::move(message));
  }

  operator bool() const { return kind_ != kNone; }

  Kind kind() const { return kind_; }

  const std::string& message() const { return message_; }

  std::string log_message() const;

  void set(Kind kind, std::string message = std::string()) {
    DCHECK(kind != kNone);
    DCHECK(kind_ == kNone);
    kind_ = kind;
    message_ = std::move(message);
  }

 private:
  Kind kind_ = kNone;
  std::string message_;
};

std::string LastWin32Error(const char* api_function,
                           std::string message = std::string());
}  // namespace winsparkle

#endif  // UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_ERROR_H_
