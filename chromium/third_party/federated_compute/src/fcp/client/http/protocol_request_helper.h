/*
 * Copyright 2022 Google LLC
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
#ifndef FCP_CLIENT_HTTP_PROTOCOL_REQUEST_HELPER_H_
#define FCP_CLIENT_HTTP_PROTOCOL_REQUEST_HELPER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "google/longrunning/operations.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/random/random.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "fcp/base/clock.h"
#include "fcp/base/wall_clock_stopwatch.h"
#include "fcp/client/http/http_client.h"
#include "fcp/client/http/in_memory_request_response.h"
#include "fcp/client/interruptible_runner.h"
#include "fcp/protos/federatedcompute/common.pb.h"

namespace fcp {
namespace client {
namespace http {

// Note the uri query parameters should be percent encoded.
using QueryParams = absl::flat_hash_map<std::string, std::string>;

// A helper for creating HTTP request with base uri, request headers and
// compression setting.
class ProtocolRequestCreator {
 public:
  ProtocolRequestCreator(absl::string_view request_base_uri,
                         absl::string_view api_key, HeaderList request_headers,
                         bool use_compression);

  // Creates a `ProtocolRequestCreator` based on the forwarding info.
  // Validates and extracts the base URI and headers to use for the subsequent
  // request(s).
  static absl::StatusOr<std::unique_ptr<ProtocolRequestCreator>> Create(
      absl::string_view api_key,
      const ::google::internal::federatedcompute::v1::ForwardingInfo&
          forwarding_info,
      bool use_compression);

  // Creates an `HttpRequest` with base uri, request headers and compression
  // setting. The `uri_path_suffix` argument must always either be empty or
  // start with a leading '/'. The method will return `InvalidArgumentError` if
  // this isn't the case.  The `uri_path_suffix` should not contain any query
  // parameters, instead, query parameters should be specified in `params`.
  //
  // The URI to which the protocol request will be sent will be constructed by
  // joining `next_request_base_uri_` with `uri_path_suffix` (see
  // `JoinBaseUriWithSuffix` for details), and any query parameters if `params`
  // is not empty.
  //
  // When `is_protobuf_encoded` is true, `%24alt=proto` will be added to the uri
  // as a query parameter to indicate that the proto encoded payload is
  // expected. When the `request_body` is not empty, a `Content-Type` header
  // will also be added to the request
  absl::StatusOr<std::unique_ptr<HttpRequest>> CreateProtocolRequest(
      absl::string_view uri_path_suffix, QueryParams params,
      HttpRequest::Method method, std::string request_body,
      bool is_protobuf_encoded) const;

  // Creates an `HttpRequest` with base uri, request headers, compression
  // setting and additional headers.
  absl::StatusOr<std::unique_ptr<HttpRequest>>
  CreateProtocolRequestWithAdditionalHeaders(
      absl::string_view uri_path_suffix, QueryParams params,
      HttpRequest::Method method, std::string request_body,
      bool is_protobuf_encoded, HeaderList additional_headers) const;

  // Creates an `HttpRequest` for getting the result of a
  // `google.longrunning.operation`. Note that the request body is empty,
  // because its only field (`name`) is included in the URI instead. Also note
  // that the `next_request_headers_` will be attached to this request.
  absl::StatusOr<std::unique_ptr<HttpRequest>> CreateGetOperationRequest(
      absl::string_view operation_name) const;

  // Creates an `HttpRequest` for canceling a `google.longrunning.operation`.
  // Note that the request body is empty, because its only field (`name`) is
  // included in the URI instead. Also note that the `next_request_headers_`
  // will be attached to this request.
  absl::StatusOr<std::unique_ptr<HttpRequest>> CreateCancelOperationRequest(
      absl::string_view operation_name) const;

 private:
  absl::StatusOr<std::unique_ptr<HttpRequest>> CreateHttpRequest(
      absl::string_view uri_path_suffix, QueryParams params,
      HttpRequest::Method method, std::string request_body,
      bool is_protobuf_encoded, bool use_compression,
      HeaderList additional_headers) const;
  // The URI to use for the next protocol request. See `ForwardingInfo`.
  std::string next_request_base_uri_;
  // The API key used for requests.
  const std::string api_key_;
  // The set of headers to attach to the next protocol request. See
  // `ForwardingInfo`.
  HeaderList next_request_headers_;
  const bool use_compression_;
};

// A helper for issuing protocol requests.
class ProtocolRequestHelper {
 public:
  ProtocolRequestHelper(HttpClient* http_client, int64_t* bytes_downloaded,
                        int64_t* bytes_uploaded,
                        WallClockStopwatch* network_stopwatch, Clock* clock,
                        absl::BitGen* bit_gen, int32_t retry_max_attempts,
                        int32_t retry_delay_ms);

  // Performs the given request (handling any interruptions that may occur) and
  // updates the network stats.
  absl::StatusOr<InMemoryHttpResponse> PerformProtocolRequest(
      std::unique_ptr<HttpRequest> request, InterruptibleRunner& runner);

  // Performs the vector of requests (handling any interruptions that may occur)
  // concurrently and updates the network stats.
  // The returned vector of responses has the same order of the issued requests.
  absl::StatusOr<std::vector<absl::StatusOr<InMemoryHttpResponse>>>
  PerformMultipleProtocolRequests(
      std::vector<std::unique_ptr<HttpRequest>> requests,
      InterruptibleRunner& runner);

  // Helper function for handling an HTTP response that contains an `Operation`
  // proto.
  //
  // Takes an HTTP response (which must have been produced by a call to
  // `PerformRequestInMemory`), parses the proto, and returns it if its
  // `Operation.done` field is true. If the field is false then this method
  // keeps polling the Operation via performing requests created by
  // `CreateGetOperationRequest` until it a response is received where the field
  // is true, at which point that most recent response is returned. If at any
  // point an HTTP or response parsing error is encountered, then that error is
  // returned instead.
  absl::StatusOr<::google::longrunning::Operation>
  PollOperationResponseUntilDone(
      const ::google::longrunning::Operation& initial_operation,
      const ProtocolRequestCreator& request_creator,
      InterruptibleRunner& runner);

  // Helper function for cancelling an operation.
  absl::StatusOr<InMemoryHttpResponse> CancelOperation(
      absl::string_view operation_name,
      const ProtocolRequestCreator& request_creator,
      InterruptibleRunner& runner);

 private:
  HttpClient& http_client_;
  int64_t& bytes_downloaded_;
  int64_t& bytes_uploaded_;
  WallClockStopwatch& network_stopwatch_;
  Clock& clock_;
  absl::BitGen& bit_gen_;
  int32_t retry_max_attempts_;
  int32_t retry_delay_ms_;
};

// Parse a google::longrunning::Operation out of a InMemoryHttpResponse.
// If the initial http_response is not OK, this method will immediately return
// with the error status.
absl::StatusOr<google::longrunning::Operation>
ParseOperationProtoFromHttpResponse(
    absl::StatusOr<InMemoryHttpResponse> http_response);

// Extract the operation name from Operation proto.
// If the operation name is not started with "operations/", invalid argument
// error will be returned.
absl::StatusOr<std::string> ExtractOperationName(
    const google::longrunning::Operation& operation);

}  // namespace http
}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_HTTP_PROTOCOL_REQUEST_HELPER_H_
