/*
 * Copyright 2019 Google LLC
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

#include "fcp/base/result.h"

#include "fcp/base/tracing_schema.h"
#include "fcp/tracing/tracing_span.h"

namespace fcp {
Error ExpectBase::TraceExpectError(const char* expectation) const {
  return TraceError<ResultExpectError>(expectation, loc_.file_name(),
                                       loc_.line());
}

static TracingStatusCode ConvertToTracingStatus(fcp::StatusCode code) {
  switch (code) {
    case fcp::StatusCode::kOk:
      return TracingStatusCode_Ok;
    case fcp::StatusCode::kCancelled:
      return TracingStatusCode_Cancelled;
    case fcp::StatusCode::kInvalidArgument:
      return TracingStatusCode_InvalidArgument;
    case fcp::StatusCode::kDeadlineExceeded:
      return TracingStatusCode_DeadlineExceeded;
    case fcp::StatusCode::kNotFound:
      return TracingStatusCode_NotFound;
    case fcp::StatusCode::kAlreadyExists:
      return TracingStatusCode_AlreadyExists;
    case fcp::StatusCode::kPermissionDenied:
      return TracingStatusCode_PermissionDenied;
    case fcp::StatusCode::kResourceExhausted:
      return TracingStatusCode_ResourceExhausted;
    case fcp::StatusCode::kFailedPrecondition:
      return TracingStatusCode_FailedPrecondition;
    case fcp::StatusCode::kAborted:
      return TracingStatusCode_Aborted;
    case fcp::StatusCode::kOutOfRange:
      return TracingStatusCode_OutOfRange;
    case fcp::StatusCode::kUnimplemented:
      return TracingStatusCode_Unimplemented;
    case fcp::StatusCode::kInternal:
      return TracingStatusCode_Internal;
    case fcp::StatusCode::kUnavailable:
      return TracingStatusCode_Unavailable;
    case fcp::StatusCode::kDataLoss:
      return TracingStatusCode_DataLoss;
    case fcp::StatusCode::kUnauthenticated:
      return TracingStatusCode_Unauthenticated;
    default:
      return TracingStatusCode_Unknown;
  }
}

Error ExpectBase::TraceUnexpectedStatus(fcp::StatusCode expected_code,
                                        const fcp::Status& actual) const {
  return TraceError<ResultExpectStatusError>(
      ConvertToTracingStatus(expected_code),
      ConvertToTracingStatus(actual.code()), actual.message(), loc_.file_name(),
      loc_.line());
}
}  // namespace fcp
