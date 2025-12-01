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
#include "fcp/base/status_converters.h"

#include "grpcpp/support/status.h"

namespace fcp {
namespace base {

#define MAP_FROM_GRPC_STATUS(grpc_name, absl_name) \
  case grpc::StatusCode::grpc_name:                \
    return StatusCode::absl_name;

#define MAP_TO_GRPC_STATUS(absl_name, grpc_name) \
  case StatusCode::absl_name:                    \
    return grpc::StatusCode::grpc_name;

StatusCode FromGrpcStatusCode(grpc::StatusCode code) {
  switch (code) {
    MAP_FROM_GRPC_STATUS(OK, kOk)
    MAP_FROM_GRPC_STATUS(CANCELLED, kCancelled)
    MAP_FROM_GRPC_STATUS(UNKNOWN, kUnknown)
    MAP_FROM_GRPC_STATUS(INVALID_ARGUMENT, kInvalidArgument)
    MAP_FROM_GRPC_STATUS(DEADLINE_EXCEEDED, kDeadlineExceeded)
    MAP_FROM_GRPC_STATUS(NOT_FOUND, kNotFound)
    MAP_FROM_GRPC_STATUS(ALREADY_EXISTS, kAlreadyExists)
    MAP_FROM_GRPC_STATUS(PERMISSION_DENIED, kPermissionDenied)
    MAP_FROM_GRPC_STATUS(UNAUTHENTICATED, kUnauthenticated)
    MAP_FROM_GRPC_STATUS(RESOURCE_EXHAUSTED, kResourceExhausted)
    MAP_FROM_GRPC_STATUS(FAILED_PRECONDITION, kFailedPrecondition)
    MAP_FROM_GRPC_STATUS(ABORTED, kAborted)
    MAP_FROM_GRPC_STATUS(OUT_OF_RANGE, kOutOfRange)
    MAP_FROM_GRPC_STATUS(UNIMPLEMENTED, kUnimplemented)
    MAP_FROM_GRPC_STATUS(INTERNAL, kInternal)
    MAP_FROM_GRPC_STATUS(UNAVAILABLE, kUnavailable)
    MAP_FROM_GRPC_STATUS(DATA_LOSS, kDataLoss)
    default:
      return StatusCode::kUnknown;
  }
}

Status FromGrpcStatus(grpc::Status status) {
  return Status(FromGrpcStatusCode(status.error_code()),
                status.error_message());
}

grpc::StatusCode ToGrpcStatusCode(StatusCode code) {
  switch (code) {
    MAP_TO_GRPC_STATUS(kOk, OK)
    MAP_TO_GRPC_STATUS(kCancelled, CANCELLED)
    MAP_TO_GRPC_STATUS(kUnknown, UNKNOWN)
    MAP_TO_GRPC_STATUS(kInvalidArgument, INVALID_ARGUMENT)
    MAP_TO_GRPC_STATUS(kDeadlineExceeded, DEADLINE_EXCEEDED)
    MAP_TO_GRPC_STATUS(kNotFound, NOT_FOUND)
    MAP_TO_GRPC_STATUS(kAlreadyExists, ALREADY_EXISTS)
    MAP_TO_GRPC_STATUS(kPermissionDenied, PERMISSION_DENIED)
    MAP_TO_GRPC_STATUS(kUnauthenticated, UNAUTHENTICATED)
    MAP_TO_GRPC_STATUS(kResourceExhausted, RESOURCE_EXHAUSTED)
    MAP_TO_GRPC_STATUS(kFailedPrecondition, FAILED_PRECONDITION)
    MAP_TO_GRPC_STATUS(kAborted, ABORTED)
    MAP_TO_GRPC_STATUS(kOutOfRange, OUT_OF_RANGE)
    MAP_TO_GRPC_STATUS(kUnimplemented, UNIMPLEMENTED)
    MAP_TO_GRPC_STATUS(kInternal, INTERNAL)
    MAP_TO_GRPC_STATUS(kUnavailable, UNAVAILABLE)
    MAP_TO_GRPC_STATUS(kDataLoss, DATA_LOSS)
    default:
      return grpc::StatusCode::UNKNOWN;
  }
}

grpc::Status ToGrpcStatus(Status status) {
  grpc::StatusCode code = ToGrpcStatusCode(status.code());
  if (code != grpc::StatusCode::OK) {
    return grpc::Status(code, std::string(status.message()));
  }

  return grpc::Status::OK;
}

}  // namespace base
}  // namespace fcp
