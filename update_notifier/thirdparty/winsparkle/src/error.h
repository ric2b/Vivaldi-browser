// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_ERROR_H_
#define UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_ERROR_H_

#include <string>
#include "base/check.h"

namespace vivaldi_update_notifier {

// Helper to report errors with kind and detailed message.
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
}  // namespace vivaldi_update_notifier

#endif  // UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_ERROR_H_
