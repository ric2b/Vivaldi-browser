/*
 * Copyright 2017 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef FCP_BASE_MONITORING_H_
#define FCP_BASE_MONITORING_H_

#include <sstream>

#include "absl/base/attributes.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace fcp {

// General Definitions
// ===================

// Logging and Assertions
// ======================

/**
 * Defines a subset of Google style logging. Use FCP_LOG(INFO),
 * FCP_LOG(WARNING), FCP_LOG(ERROR) or FCP_LOG(FATAL) to stream log messages.
 *
 * These macros should be preferred over using Absl's log macros directly, since
 * they are compatible with the different environments some of the FCP code is
 * used in. They also reduce the logging verbosity on Android, to ensure
 * that INFO or VLOG logs are not logged to logcat unless
 * FCP_VERBOSE_ANDROID_LOGCAT is defined at build time.
 *
 * Example:
 *
 *     FCP_LOG(INFO) << "some info log";
 *     FCP_VLOG(1) << "some verbose log";
 */

#define FCP_LOG(severity) _FCP_LOG_##severity
#define FCP_LOG_IF(severity, condition) _FCP_LOG_IF_##severity(condition)
// An FCP_VLOG is also defined (below), but note that these log statements may
// be evaluated regardless of whether the binary's verbosity level is set high
// enough for them to be included in the actual logs, so be careful when using
// it with expensive-to-evaluate log statements.

#if !defined(__ANDROID__)
// On regular (non-Android) builds we forward all logs to Absl as-is.
#define _FCP_LOG_INFO ABSL_LOG(INFO)
#define _FCP_LOG_WARNING ABSL_LOG(WARNING)
#define _FCP_LOG_ERROR ABSL_LOG(ERROR)
#define _FCP_LOG_FATAL ABSL_LOG(FATAL)
#define _FCP_LOG_IF_INFO(condition) ABSL_LOG_IF(INFO, condition)
#define _FCP_LOG_IF_WARNING(condition) ABSL_LOG_IF(WARNING, condition)
#define _FCP_LOG_IF_ERROR(condition) ABSL_LOG_IF(ERROR, condition)
#define _FCP_LOG_IF_FATAL(condition) ABSL_LOG_IF(FATAL, condition)
#define FCP_VLOG(verbosity) ABSL_LOG(INFO).WithVerbosity(verbosity)

#endif  // !defined(__ANDROID__)

#if defined(__ANDROID__)
// On Android we prepend "fcp: " to all logs, since Absl will set the log tag
// for all process-wide logs to a generic "native". Prefixing by "fcp" helps us
// find FCP-related logs in the logcat more easily.

#define _FCP_LOG_WARNING ABSL_LOG(WARNING) << "fcp: "
#define _FCP_LOG_ERROR ABSL_LOG(ERROR) << "fcp: "
#define _FCP_LOG_FATAL ABSL_LOG(FATAL) << "fcp: "
#define _FCP_LOG_IF_WARNING(condition) \
  ABSL_LOG_IF(WARNING, condition) << "fcp: "
#define _FCP_LOG_IF_ERROR(condition) ABSL_LOG_IF(ERROR, condition) << "fcp: "
#define _FCP_LOG_IF_FATAL(condition) ABSL_LOG_IF(FATAL, condition) << "fcp: "

// On Android we also, by default, do not log INFO level logs (or more verbose
// VLOGs) to Absl (which in turn would log them to logcat). Only if
// FCP_VERBOSE_ANDROID_LOGCAT is defined do we log those logs. If that is not
// defined, then the logs will be stripped out by the linker due to our use of
// ABSL_LOG_IF(..., false).

#ifdef FCP_VERBOSE_ANDROID_LOGCAT
#define _FCP_LOG_INFO ABSL_LOG(INFO) << "fcp: "
#define _FCP_LOG_IF_INFO(condition) ABSL_LOG_IF(INFO, condition) << "fcp: "
#define FCP_VLOG(verbosity) ABSL_LOG(INFO).WithVerbosity(verbosity) << "fcp: "
#else
#define _FCP_LOG_INFO ABSL_LOG_IF(INFO, false)
#define _FCP_LOG_IF_INFO(condition) ABSL_LOG_IF(INFO, false)
#define FCP_VLOG(verbosity) ABSL_LOG_IF(INFO, false)
#endif  // FCP_VERBOSE_ANDROID_LOGCAT

#endif  // defined(__ANDROID__)

#define FCP_PREDICT_FALSE(x) ABSL_PREDICT_FALSE(x)
#define FCP_PREDICT_TRUE(x) ABSL_PREDICT_TRUE(x)

/**
 * Check that the condition holds, otherwise die. Any additional messages can
 * be streamed into the invocation. Example:
 *
 *     FCP_CHECK(condition) << "stuff went wrong";
 */
#define FCP_CHECK(condition)                         \
  FCP_LOG_IF(FATAL, FCP_PREDICT_FALSE(!(condition))) \
      << ("Check failed: " #condition ". ")

/**
 * Check that the expression generating a status code is OK, otherwise die.
 * Any additional messages can be streamed into the invocation.
 */
#define FCP_CHECK_STATUS(status)                                     \
  for (auto __check_status = (status);                               \
       __check_status.code() != ::fcp::StatusCode::kOk;)             \
  FCP_LOG_IF(FATAL, __check_status.code() != ::fcp::StatusCode::kOk) \
      << "status not OK: " << __check_status

// Status and StatusOr
// ===================

/**
 * Constructor for a status. A status message can be streamed into it. This
 * captures the current file and line position and includes it into the status
 * message if the status code is not OK.
 *
 * Use as in:
 *
 *   FCP_STATUS(OK);                // signal success
 *   FCP_STATUS(code) << message;   // signal failure
 *
 * FCP_STATUS can be used in places which either expect a Status or a
 * StatusOr<T>.
 */
#define FCP_STATUS(code) \
  ::fcp::internal::MakeStatusBuilder(code, __FILE__, __LINE__)

#define FCP_MUST_USE_RESULT ABSL_MUST_USE_RESULT

using Status = absl::Status;
using StatusCode = absl::StatusCode;
template <typename T>
using StatusOr = absl::StatusOr<T>;

constexpr auto OK = StatusCode::kOk;
constexpr auto CANCELLED = StatusCode::kCancelled;
constexpr auto UNKNOWN = StatusCode::kUnknown;
constexpr auto INVALID_ARGUMENT = StatusCode::kInvalidArgument;
constexpr auto DEADLINE_EXCEEDED = StatusCode::kDeadlineExceeded;
constexpr auto NOT_FOUND = StatusCode::kNotFound;
constexpr auto ALREADY_EXISTS = StatusCode::kAlreadyExists;
constexpr auto PERMISSION_DENIED = StatusCode::kPermissionDenied;
constexpr auto RESOURCE_EXHAUSTED = StatusCode::kResourceExhausted;
constexpr auto FAILED_PRECONDITION = StatusCode::kFailedPrecondition;
constexpr auto ABORTED = StatusCode::kAborted;
constexpr auto OUT_OF_RANGE = StatusCode::kOutOfRange;
constexpr auto UNIMPLEMENTED = StatusCode::kUnimplemented;
constexpr auto INTERNAL = StatusCode::kInternal;
constexpr auto UNAVAILABLE = StatusCode::kUnavailable;
constexpr auto DATA_LOSS = StatusCode::kDataLoss;
constexpr auto UNAUTHENTICATED = StatusCode::kUnauthenticated;

namespace internal {
/** Functions to assist with FCP_RETURN_IF_ERROR() */
inline Status AsStatus(const Status& status) { return status; }
template <typename T>
inline Status AsStatus(const StatusOr<T>& status_or) {
  return status_or.status();
}
}  // namespace internal

/**
 * Macro which allows to check for a Status (or StatusOr) and return from the
 * current method if not OK. Example:
 *
 *     Status DoSomething() {
 *       FCP_RETURN_IF_ERROR(Step1());
 *       FCP_RETURN_IF_ERROR(Step2ReturningStatusOr().status());
 *       return FCP_STATUS(OK);
 *     }
 */
#define FCP_RETURN_IF_ERROR(expr)                             \
  do {                                                        \
    ::fcp::Status __status = ::fcp::internal::AsStatus(expr); \
    if (__status.code() != ::fcp::StatusCode::kOk) {          \
      return (__status);                                      \
    }                                                         \
  } while (false)

/**
 * Macro which allows to check for a StatusOr and return it's status if not OK,
 * otherwise assign the value in the StatusOr to variable or declaration. Usage:
 *
 *     StatusOr<bool> DoSomething() {
 *       FCP_ASSIGN_OR_RETURN(auto value, TryComputeSomething());
 *       if (!value) {
 *         FCP_ASSIGN_OR_RETURN(value, TryComputeSomethingElse());
 *       }
 *       return value;
 *     }
 */
#define FCP_ASSIGN_OR_RETURN(lhs, expr) \
  _FCP_ASSIGN_OR_RETURN_1(              \
      _FCP_ASSIGN_OR_RETURN_CONCAT(statusor_for_aor, __LINE__), lhs, expr)

#define _FCP_ASSIGN_OR_RETURN_1(statusor, lhs, expr) \
  auto statusor = (expr);                            \
  if (!statusor.ok()) {                              \
    return statusor.status();                        \
  }                                                  \
  lhs = std::move(statusor).value()

// See https://goo.gl/x3iba2 for the reason of this construction.
#define _FCP_ASSIGN_OR_RETURN_CONCAT(x, y) \
  _FCP_ASSIGN_OR_RETURN_CONCAT_INNER(x, y)
#define _FCP_ASSIGN_OR_RETURN_CONCAT_INNER(x, y) x##y

// Status Implementation Details
// =============================

namespace internal {

/**
 * Helper class which allows to construct a status with message by streaming
 * into it. Implicitly converts to Status and StatusOr so can be used as a drop
 * in replacement when those types are expected.
 */
class FCP_MUST_USE_RESULT StatusBuilder {
 public:
  /** Construct a StatusBuilder from status code. */
  StatusBuilder(StatusCode code, const char* file, int line);

  /**
   * Copy constructor for status builder. Most of the time not needed because of
   * copy ellision. */
  StatusBuilder(StatusBuilder const& other);

  /** Return true if the constructed status will be OK. */
  inline bool ok() const { return code_ == OK; }

  /** Returns the code of the constructed status. */
  inline StatusCode code() const { return code_; }

  /** Stream into status message of this builder. */
  template <typename T>
  StatusBuilder& operator<<(T x) {
    message_ << x;
    return *this;
  }

  /** Implicit conversion to Status. */
  operator Status();  // NOLINT

  /** Implicit conversion to StatusOr. */
  template <typename T>
  inline operator StatusOr<T>() {  // NOLINT
    return StatusOr<T>(static_cast<Status>(*this));
  }

 private:
  const char* const file_;
  const int line_;
  const StatusCode code_;

  std::ostringstream message_;
};

inline StatusBuilder MakeStatusBuilder(StatusCode code, const char* file,
                                       int line) {
  return StatusBuilder(code, file, line);
}

}  // namespace internal
}  // namespace fcp

#endif  // FCP_BASE_MONITORING_H_
