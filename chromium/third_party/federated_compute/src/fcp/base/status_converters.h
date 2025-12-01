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

#ifndef FCP_BASE_STATUS_CONVERTERS_H_
#define FCP_BASE_STATUS_CONVERTERS_H_

#include "grpcpp/support/status.h"
#include "absl/status/status.h"
#include "fcp/base/monitoring.h"

namespace fcp {
namespace base {

/**
 * Converts a grpc::StatusCode to an StatusCode.
 */
StatusCode FromGrpcStatusCode(grpc::StatusCode code);

/**
 * Converts grpc::Status to an fcp::Status.
 */
Status FromGrpcStatus(grpc::Status status);

/**
 * Converts an StatusCode to a grpc::StatusCode.
 */
grpc::StatusCode ToGrpcStatusCode(StatusCode code);

/**
 * Converts fcp::Status to grpc::Status.
 */
grpc::Status ToGrpcStatus(Status status);

}  // namespace base
}  // namespace fcp

#endif  // FCP_BASE_STATUS_CONVERTERS_H_
