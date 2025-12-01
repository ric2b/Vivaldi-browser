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
#include "fcp/client/http/protocol_request_helper.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "absl/random/random.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "absl/time/time.h"
#include "fcp/base/clock.h"
#include "fcp/base/monitoring.h"
#include "fcp/base/time_util.h"
#include "fcp/base/wall_clock_stopwatch.h"
#include "fcp/client/http/http_client.h"
#include "fcp/client/http/http_client_util.h"
#include "fcp/client/http/in_memory_request_response.h"
#include "fcp/client/interruptible_runner.h"
#include "fcp/protos/federatedcompute/secure_aggregations.pb.h"
#include "fcp/protos/federatedcompute/task_assignments.pb.h"

namespace fcp {
namespace client {
namespace http {

// The default interval when polling pending operations.
const absl::Duration kDefaultLroPollingInterval = absl::Milliseconds(500);
// The maximum interval when polling pending operations.
const absl::Duration kMaxLroPollingInterval = absl::Minutes(1);

constexpr absl::string_view kStartTaskAssignmentMetadata =
    "type.googleapis.com/"
    "google.internal.federatedcompute.v1.StartTaskAssignmentMetadata";  // NOLINT
constexpr absl::string_view kAdvertiseKeysMetadata =
    "type.googleapis.com/"
    "google.internal.federatedcompute.v1.AdvertiseKeysMetadata";  // NOLINT
constexpr absl::string_view kShareKeysMetadata =
    "type.googleapis.com/google.internal.federatedcompute.v1.ShareKeysMetadata";
constexpr absl::string_view kSubmitSecureAggregationResultMetadata =
    "type.googleapis.com/"
    "google.internal.federatedcompute.v1."
    "SubmitSecureAggregationResultMetadata";  // NOLINT

using ::google::internal::federatedcompute::v1::AdvertiseKeysMetadata;
using ::google::internal::federatedcompute::v1::ForwardingInfo;
using ::google::internal::federatedcompute::v1::ShareKeysMetadata;
using ::google::internal::federatedcompute::v1::StartTaskAssignmentMetadata;
using ::google::internal::federatedcompute::v1::
    SubmitSecureAggregationResultMetadata;
using ::google::longrunning::Operation;

namespace {
// A note on error handling:
//
// The implementation here makes a distinction between what we call 'transient'
// and 'permanent' errors. While the exact categorization of transient vs.
// permanent errors is defined by a flag, the intent is that transient errors
// are those types of errors that may occur in the regular course of business,
// e.g. due to an interrupted network connection, a load balancer temporarily
// rejecting our request etc. Generally, these are expected to be resolvable by
// merely retrying the request at a slightly later time. Permanent errors are
// intended to be those that are not expected to be resolvable as quickly or by
// merely retrying the request. E.g. if a client checks in to the server with a
// population name that doesn't exist, then the server may return NOT_FOUND, and
// until the server-side configuration is changed, it will continue returning
// such an error. Hence, such errors can warrant a longer retry period (to waste
// less of both the client's and server's resources).
//
// The errors also differ in how they interact with the server-specified retry
// windows that are returned via the EligbilityEvalTaskResponse message.
// - If a permanent error occurs, then we will always return a retry window
//   based on the target 'permanent errors retry period' flag, regardless of
//   whether we received an EligbilityEvalTaskResponse from the server at an
//   earlier time.
// - If a transient error occurs, then we will only return a retry window
//   based on the target 'transient errors retry period' flag if the server
//   didn't already return an EligibilityEvalTaskResponse. If it did return such
//   a response, then one of the retry windows in that message will be used
//   instead.
//
// Finally, note that for simplicity's sake we generally check whether a
// permanent error was received at the level of this class's public methods,
// rather than deeper down in each of our helper methods that actually call
// directly into the HTTP stack. This keeps our state-managing code simpler, but
// does mean that if any of our helper methods like
// PerformEligibilityEvalTaskRequest produce a permanent error code locally
// (i.e. without it being sent by the server), it will be treated as if the
// server sent it and the permanent error retry period will be used. We consider
// this a reasonable tradeoff.

std::string CreateUriSuffixFromPathAndParams(absl::string_view path,
                                             const QueryParams& params) {
  return absl::StrCat(path, "?",
                      absl::StrJoin(params.begin(), params.end(), "&",
                                    absl::PairFormatter("=")));
}

// Creates the URI suffix for a GetOperation protocol request.
absl::StatusOr<std::string> CreateGetOperationUriSuffix(
    absl::string_view operation_name) {
  constexpr absl::string_view kGetOperationUriSuffix = "/v1/$0";
  FCP_ASSIGN_OR_RETURN(std::string encoded_operation_name,
                       EncodeUriMultiplePathSegments(operation_name));
  return absl::Substitute(kGetOperationUriSuffix, encoded_operation_name);
}

// Creates the URI suffix for a CancelOperation protocol request.
absl::StatusOr<std::string> CreateCancelOperationUriSuffix(
    absl::string_view operation_name) {
  constexpr absl::string_view kCancelOperationUriSuffix = "/v1/$0:cancel";
  FCP_ASSIGN_OR_RETURN(std::string encoded_operation_name,
                       EncodeUriMultiplePathSegments(operation_name));
  return absl::Substitute(kCancelOperationUriSuffix, encoded_operation_name);
}

absl::StatusOr<InMemoryHttpResponse> CheckResponseContentEncoding(
    absl::StatusOr<InMemoryHttpResponse> response) {
  if (response.ok() && !response->content_encoding.empty()) {
    // Note that the `HttpClient` API contract ensures that if we don't specify
    // an Accept-Encoding request header, then the response should be delivered
    // to us without any Content-Encoding applied to it. Hence, if we somehow do
    // still see a Content-Encoding response header then the `HttpClient`
    // implementation isn't adhering to its part of the API contract.
    return absl::UnavailableError(
        "HTTP response unexpectedly has a Content-Encoding");
  }
  return response;
}

// Extract polling interval from the operation proto.
// The returned polling interval will be within the range of [1ms, 1min]. If
// the polling interval inside the operation proto is outside this range, it'll
// be clipped to the nearest boundary. If the polling interval is unset, 1ms
// will be returned.
absl::Duration GetPollingInterval(Operation operation) {
  absl::string_view type_url = operation.metadata().type_url();
  google::protobuf::Duration polling_interval_proto;
  if (type_url == kStartTaskAssignmentMetadata) {
    StartTaskAssignmentMetadata metadata;
    if (!operation.metadata().UnpackTo(&metadata)) {
      return kDefaultLroPollingInterval;
    }
    polling_interval_proto = metadata.polling_interval();
  } else if (type_url == kAdvertiseKeysMetadata) {
    AdvertiseKeysMetadata metadata;
    if (!operation.metadata().UnpackTo(&metadata)) {
      return kDefaultLroPollingInterval;
    }
    polling_interval_proto = metadata.polling_interval();
  } else if (type_url == kShareKeysMetadata) {
    ShareKeysMetadata metadata;
    if (!operation.metadata().UnpackTo(&metadata)) {
      return kDefaultLroPollingInterval;
    }
    polling_interval_proto = metadata.polling_interval();
  } else if (type_url == kSubmitSecureAggregationResultMetadata) {
    SubmitSecureAggregationResultMetadata metadata;
    if (!operation.metadata().UnpackTo(&metadata)) {
      return kDefaultLroPollingInterval;
    }
    polling_interval_proto = metadata.polling_interval();
  } else {
    // Unknown type
    return kDefaultLroPollingInterval;
  }

  absl::Duration polling_interval =
      TimeUtil::ConvertProtoToAbslDuration(polling_interval_proto);
  if (polling_interval < absl::ZeroDuration()) {
    return kDefaultLroPollingInterval;
  } else if (polling_interval > kMaxLroPollingInterval) {
    return kMaxLroPollingInterval;
  } else {
    return polling_interval;
  }
}

}  // anonymous namespace

ProtocolRequestCreator::ProtocolRequestCreator(
    absl::string_view request_base_uri, absl::string_view api_key,
    HeaderList request_headers, bool use_compression)
    : next_request_base_uri_(request_base_uri),
      api_key_(api_key),
      next_request_headers_(std::move(request_headers)),
      use_compression_(use_compression) {}

absl::StatusOr<std::unique_ptr<HttpRequest>>
ProtocolRequestCreator::CreateProtocolRequest(absl::string_view uri_path_suffix,
                                              QueryParams params,
                                              HttpRequest::Method method,
                                              std::string request_body,
                                              bool is_protobuf_encoded) const {
  return CreateProtocolRequestWithAdditionalHeaders(
      uri_path_suffix, std::move(params), method, std::move(request_body),
      is_protobuf_encoded, /*additional_headers=*/{});
}

absl::StatusOr<std::unique_ptr<HttpRequest>>
ProtocolRequestCreator::CreateProtocolRequestWithAdditionalHeaders(
    absl::string_view uri_path_suffix, QueryParams params,
    HttpRequest::Method method, std::string request_body,
    bool is_protobuf_encoded, HeaderList additional_headers) const {
  return CreateHttpRequest(uri_path_suffix, std::move(params), method,
                           std::move(request_body), is_protobuf_encoded,
                           use_compression_, additional_headers);
}

absl::StatusOr<std::unique_ptr<HttpRequest>>
ProtocolRequestCreator::CreateGetOperationRequest(
    absl::string_view operation_name) const {
  FCP_ASSIGN_OR_RETURN(std::string uri_path_suffix,
                       CreateGetOperationUriSuffix(operation_name));
  return CreateHttpRequest(uri_path_suffix, {}, HttpRequest::Method::kGet, "",
                           /*is_protobuf_encoded=*/true,
                           /*use_compression=*/false,
                           /*additional_headers=*/{});
}

absl::StatusOr<std::unique_ptr<HttpRequest>>
ProtocolRequestCreator::CreateCancelOperationRequest(
    absl::string_view operation_name) const {
  FCP_ASSIGN_OR_RETURN(std::string uri_path_suffix,
                       CreateCancelOperationUriSuffix(operation_name));
  return CreateHttpRequest(uri_path_suffix, {}, HttpRequest::Method::kPost, "",
                           /*is_protobuf_encoded=*/true,
                           /*use_compression=*/false,
                           /*additional_headers=*/{});
}

absl::StatusOr<std::unique_ptr<HttpRequest>>
ProtocolRequestCreator::CreateHttpRequest(absl::string_view uri_path_suffix,
                                          QueryParams params,
                                          HttpRequest::Method method,
                                          std::string request_body,
                                          bool is_protobuf_encoded,
                                          bool use_compression,
                                          HeaderList additional_headers) const {
  HeaderList request_headers = next_request_headers_;
  request_headers.push_back({kApiKeyHdr, api_key_});
  if (is_protobuf_encoded) {
    if (!request_body.empty()) {
      request_headers.push_back({kContentTypeHdr, kProtobufContentType});
    }

    // %24alt is the percent encoded $alt.  "$" is prepended to alt to indicate
    // that "alt" is a system parameter.
    // https://cloud.google.com/apis/docs/system-parameters#http_mapping
    params["%24alt"] = "proto";
  }
  request_headers.insert(request_headers.end(), additional_headers.begin(),
                         additional_headers.end());
  std::string uri_with_params = std::string(uri_path_suffix);
  if (!params.empty()) {
    uri_with_params = CreateUriSuffixFromPathAndParams(uri_path_suffix, params);
  }
  FCP_ASSIGN_OR_RETURN(
      std::string uri,
      JoinBaseUriWithSuffix(next_request_base_uri_, uri_with_params));

  return InMemoryHttpRequest::Create(uri, method, request_headers,
                                     std::move(request_body), use_compression);
}

absl::StatusOr<std::unique_ptr<ProtocolRequestCreator>>
ProtocolRequestCreator::Create(absl::string_view api_key,
                               const ForwardingInfo& forwarding_info,
                               bool use_compression) {
  // Extract the base URI and headers to use for the subsequent request.
  if (forwarding_info.target_uri_prefix().empty()) {
    return absl::InvalidArgumentError(
        "Missing `ForwardingInfo.target_uri_prefix`");
  }
  const auto& new_headers = forwarding_info.extra_request_headers();
  return std::make_unique<ProtocolRequestCreator>(ProtocolRequestCreator(
      forwarding_info.target_uri_prefix(), api_key,
      HeaderList(new_headers.begin(), new_headers.end()), use_compression));
}

ProtocolRequestHelper::ProtocolRequestHelper(
    HttpClient* http_client, int64_t* bytes_downloaded, int64_t* bytes_uploaded,
    WallClockStopwatch* network_stopwatch, Clock* clock, absl::BitGen* bit_gen,
    int32_t retry_max_attempts, int32_t retry_delay_ms)
    : http_client_(*http_client),
      bytes_downloaded_(*bytes_downloaded),
      bytes_uploaded_(*bytes_uploaded),
      network_stopwatch_(*network_stopwatch),
      clock_(*clock),
      bit_gen_(*bit_gen),
      retry_max_attempts_(retry_max_attempts),
      retry_delay_ms_(retry_delay_ms) {}

absl::StatusOr<InMemoryHttpResponse>
ProtocolRequestHelper::PerformProtocolRequest(
    std::unique_ptr<HttpRequest> request, InterruptibleRunner& runner) {
  std::vector<std::unique_ptr<HttpRequest>> requests;
  requests.push_back(std::move(request));
  FCP_ASSIGN_OR_RETURN(
      std::vector<absl::StatusOr<InMemoryHttpResponse>> response,
      PerformMultipleProtocolRequests(std::move(requests), runner));
  return std::move(response[0]);
}

absl::StatusOr<std::vector<absl::StatusOr<InMemoryHttpResponse>>>
ProtocolRequestHelper::PerformMultipleProtocolRequests(
    std::vector<std::unique_ptr<http::HttpRequest>> requests,
    InterruptibleRunner& runner) {
  // Check whether issuing the request failed as a whole (generally indicating
  // a programming error).
  std::vector<absl::StatusOr<InMemoryHttpResponse>> responses;
  {
    auto started_stopwatch = network_stopwatch_.Start();
    FCP_ASSIGN_OR_RETURN(
        responses, PerformMultipleRequestsInMemoryWithRetry(
                       http_client_, runner, std::move(requests),
                       &bytes_downloaded_, &bytes_uploaded_, &clock_, &bit_gen_,
                       retry_max_attempts_, retry_delay_ms_));
  }
  std::vector<absl::StatusOr<InMemoryHttpResponse>> results;
  std::transform(responses.begin(), responses.end(),
                 std::back_inserter(results), CheckResponseContentEncoding);
  return results;
}

absl::StatusOr<::google::longrunning::Operation>
ProtocolRequestHelper::PollOperationResponseUntilDone(
    const Operation& initial_operation,
    const ProtocolRequestCreator& request_creator,
    InterruptibleRunner& runner) {
  // There are three cases that lead to this method returning:
  // - The HTTP response indicates an error.
  // - The HTTP response cannot be parsed into an Operation proto.
  // - The response `Operation.done` field is true.
  //
  // In all other cases we continue to poll the Operation via a subsequent
  // GetOperationRequest.
  Operation response_operation_proto = initial_operation;
  while (true) {
    // If the Operation is done then return it.
    if (response_operation_proto.done()) {
      return std::move(response_operation_proto);
    }

    FCP_ASSIGN_OR_RETURN(std::string operation_name,
                         ExtractOperationName(response_operation_proto));

    // Wait for server returned polling interval before sending next request.
    clock_.Sleep(GetPollingInterval(response_operation_proto));
    // The response Operation indicates that the result isn't ready yet. Poll
    // again.
    FCP_ASSIGN_OR_RETURN(
        std::unique_ptr<HttpRequest> get_operation_request,
        request_creator.CreateGetOperationRequest(operation_name));
    absl::StatusOr<InMemoryHttpResponse> http_response =
        PerformProtocolRequest(std::move(get_operation_request), runner);
    FCP_ASSIGN_OR_RETURN(response_operation_proto,
                         ParseOperationProtoFromHttpResponse(http_response));
  }
}

absl::StatusOr<InMemoryHttpResponse> ProtocolRequestHelper::CancelOperation(
    absl::string_view operation_name,
    const ProtocolRequestCreator& request_creator,
    InterruptibleRunner& runner) {
  FCP_ASSIGN_OR_RETURN(
      std::unique_ptr<HttpRequest> cancel_operation_request,
      request_creator.CreateCancelOperationRequest(operation_name));
  return PerformProtocolRequest(std::move(cancel_operation_request), runner);
}

absl::StatusOr<Operation> ParseOperationProtoFromHttpResponse(
    absl::StatusOr<InMemoryHttpResponse> http_response) {
  // If the HTTP response indicates an error then return that error.
  FCP_RETURN_IF_ERROR(http_response);
  Operation response_operation_proto;
  // Parse the response.
    if
    (!response_operation_proto.ParseFromString(std::string(http_response->body)))
    {
    return absl::InvalidArgumentError("could not parse Operation proto");
  }
  return response_operation_proto;
}

absl::StatusOr<std::string> ExtractOperationName(const Operation& operation) {
  if (!absl::StartsWith(operation.name(), "operations/")) {
    return absl::InvalidArgumentError(
        "Cannot cancel an Operation with an invalid name");
  }
  return operation.name();
}
}  // namespace http
}  // namespace client
}  // namespace fcp
