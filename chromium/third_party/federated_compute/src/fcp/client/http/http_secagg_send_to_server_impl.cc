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
#include "fcp/client/http/http_secagg_send_to_server_impl.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "google/rpc/code.pb.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "absl/time/time.h"
#include "fcp/base/clock.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/http/http_client.h"
#include "fcp/client/http/http_client_util.h"
#include "fcp/client/http/in_memory_request_response.h"
#include "fcp/client/http/protocol_request_helper.h"
#include "fcp/client/interruptible_runner.h"
#include "fcp/protos/federatedcompute/secure_aggregations.pb.h"

namespace fcp {
namespace client {
namespace http {

using ::google::internal::federatedcompute::v1::AbortSecureAggregationRequest;
using ::google::internal::federatedcompute::v1::AdvertiseKeysRequest;
using ::google::internal::federatedcompute::v1::AdvertiseKeysResponse;
using ::google::internal::federatedcompute::v1::ByteStreamResource;
using ::google::internal::federatedcompute::v1::ForwardingInfo;
using ::google::internal::federatedcompute::v1::ShareKeysRequest;
using ::google::internal::federatedcompute::v1::ShareKeysResponse;
using ::google::internal::federatedcompute::v1::
    SubmitSecureAggregationResultRequest;
using ::google::internal::federatedcompute::v1::
    SubmitSecureAggregationResultResponse;
using ::google::internal::federatedcompute::v1::UnmaskRequest;
using ::google::longrunning::Operation;

namespace {
absl::StatusOr<std::string> CreateAbortSecureAggregationUriSuffix(
    absl::string_view aggregation_id, absl::string_view client_token) {
  constexpr absl::string_view pattern =
      "/v1/secureaggregations/$0/clients/$1:abort";
  FCP_ASSIGN_OR_RETURN(std::string encoded_aggregation_id,
                       EncodeUriSinglePathSegment(aggregation_id));
  FCP_ASSIGN_OR_RETURN(std::string encoded_client_token,
                       EncodeUriSinglePathSegment(client_token));
  // Construct the URI suffix.
  return absl::Substitute(pattern, encoded_aggregation_id,
                          encoded_client_token);
}

absl::StatusOr<std::string> CreateAdvertiseKeysUriSuffix(
    absl::string_view aggregation_id, absl::string_view client_token) {
  constexpr absl::string_view pattern =
      "/v1/secureaggregations/$0/clients/$1:advertisekeys";
  FCP_ASSIGN_OR_RETURN(std::string encoded_aggregation_id,
                       EncodeUriSinglePathSegment(aggregation_id));
  FCP_ASSIGN_OR_RETURN(std::string encoded_client_token,
                       EncodeUriSinglePathSegment(client_token));
  // Construct the URI suffix.
  return absl::Substitute(pattern, encoded_aggregation_id,
                          encoded_client_token);
}

absl::StatusOr<std::string> CreateShareKeysUriSuffix(
    absl::string_view aggregation_id, absl::string_view client_token) {
  constexpr absl::string_view pattern =
      "/v1/secureaggregations/$0/clients/$1:sharekeys";
  FCP_ASSIGN_OR_RETURN(std::string encoded_aggregation_id,
                       EncodeUriSinglePathSegment(aggregation_id));
  FCP_ASSIGN_OR_RETURN(std::string encoded_client_token,
                       EncodeUriSinglePathSegment(client_token));
  // Construct the URI suffix.
  return absl::Substitute(pattern, encoded_aggregation_id,
                          encoded_client_token);
}

absl::StatusOr<std::string> CreateSubmitSecureAggregationResultUriSuffix(
    absl::string_view aggregation_id, absl::string_view client_token) {
  constexpr absl::string_view pattern =
      "/v1/secureaggregations/$0/clients/$1:submit";
  FCP_ASSIGN_OR_RETURN(std::string encoded_aggregation_id,
                       EncodeUriSinglePathSegment(aggregation_id));
  FCP_ASSIGN_OR_RETURN(std::string encoded_client_token,
                       EncodeUriSinglePathSegment(client_token));
  // Construct the URI suffix.
  return absl::Substitute(pattern, encoded_aggregation_id,
                          encoded_client_token);
}

absl::StatusOr<std::string> CreateUnmaskUriSuffix(
    absl::string_view aggregation_id, absl::string_view client_token) {
  constexpr absl::string_view pattern =
      "/v1/secureaggregations/$0/clients/$1:unmask";
  FCP_ASSIGN_OR_RETURN(std::string encoded_aggregation_id,
                       EncodeUriSinglePathSegment(aggregation_id));
  FCP_ASSIGN_OR_RETURN(std::string encoded_client_token,
                       EncodeUriSinglePathSegment(client_token));
  // Construct the URI suffix.
  return absl::Substitute(pattern, encoded_aggregation_id,
                          encoded_client_token);
}

}  // anonymous namespace

absl::StatusOr<std::unique_ptr<HttpSecAggSendToServerImpl>>
HttpSecAggSendToServerImpl::Create(
    absl::string_view api_key, Clock* clock,
    ProtocolRequestHelper* request_helper,
    InterruptibleRunner* interruptible_runner,
    std::function<std::unique_ptr<InterruptibleRunner>(absl::Time)>
        delayed_interruptible_runner_creator,
    absl::StatusOr<secagg::ServerToClientWrapperMessage>*
        server_response_holder,
    absl::string_view aggregation_id, absl::string_view client_token,
    const ForwardingInfo& secagg_upload_forwarding_info,
    const ByteStreamResource& masked_result_resource,
    const ByteStreamResource& nonmasked_result_resource,
    std::optional<std::string> tf_checkpoint,
    bool disable_request_body_compression,
    absl::Duration waiting_period_for_cancellation) {
  FCP_ASSIGN_OR_RETURN(
      std::unique_ptr<ProtocolRequestCreator> secagg_request_creator,
      ProtocolRequestCreator::Create(api_key, secagg_upload_forwarding_info,
                                     !disable_request_body_compression));
  // We don't use request body compression for resource upload.
  FCP_ASSIGN_OR_RETURN(
      std::unique_ptr<ProtocolRequestCreator>
          masked_result_upload_request_creator,
      ProtocolRequestCreator::Create(
          api_key, masked_result_resource.data_upload_forwarding_info(),
          /*use_compression=*/false));
  // We don't use request body compression for resource upload.
  FCP_ASSIGN_OR_RETURN(
      std::unique_ptr<ProtocolRequestCreator>
          nonmasked_result_upload_request_creator,
      ProtocolRequestCreator::Create(
          api_key, nonmasked_result_resource.data_upload_forwarding_info(),
          /*use_compression=*/false));

  return absl::WrapUnique(new HttpSecAggSendToServerImpl(
      api_key, clock, request_helper, interruptible_runner,
      std::move(delayed_interruptible_runner_creator), server_response_holder,
      aggregation_id, client_token, masked_result_resource.resource_name(),
      nonmasked_result_resource.resource_name(),
      std::move(secagg_request_creator),
      std::move(masked_result_upload_request_creator),
      std::move(nonmasked_result_upload_request_creator),
      std::move(tf_checkpoint), waiting_period_for_cancellation));
}

// Despite the method name is "Send", this method is doing more. It sends the
// request, waits for the response and set the response to the response holder
// for the secagg client to access in the next round of secagg communications.
//
// The current SecAgg library is built around the assumption that the underlying
// network protocol is fully asynchronous and bidirectional. This was true for
// the gRPC protocol but isn't the case anymore for the HTTP protocol (which has
// a more traditional request/response structure). Nevertheless, because we
// still need to support the gRPC protocol the structure of the SecAgg library
// cannot be changed yet, and this means that currently we need to store away
// the result and let the secagg client to access on a later time. However, once
// the gRPC protocol support is removed, we should consider updating the SecAgg
// library to assume the more traditional request/response structure (e.g. by
// having SecAggSendToServer::Send return the corresponding response message).
//
// TODO(team): Simplify SecAgg library around request/response structure
// once gRPC support is removed.
void HttpSecAggSendToServerImpl::Send(
    secagg::ClientToServerWrapperMessage* message) {
  absl::StatusOr<secagg::ServerToClientWrapperMessage> server_message;
  if (message->has_advertise_keys()) {
    server_response_holder_ =
        DoR0AdvertiseKeys(std::move(message->advertise_keys()));
  } else if (message->has_share_keys_response()) {
    server_response_holder_ =
        DoR1ShareKeys(std::move(message->share_keys_response()));
  } else if (message->has_masked_input_response()) {
    server_response_holder_ = DoR2SubmitSecureAggregationResult(
        std::move(message->masked_input_response()));
  } else if (message->has_unmasking_response()) {
    server_response_holder_ =
        DoR3Unmask(std::move(message->unmasking_response()));
  } else if (message->has_abort()) {
    server_response_holder_ =
        AbortSecureAggregation(std::move(message->abort()));
  } else {
    // When the protocol succeeds, the ClientToServerWrapperMessage will be
    // empty, and we'll just set the empty server message.
    server_response_holder_ = secagg::ServerToClientWrapperMessage();
  }
}

absl::StatusOr<secagg::ServerToClientWrapperMessage>
HttpSecAggSendToServerImpl::AbortSecureAggregation(
    secagg::AbortMessage abort_message) {
  FCP_ASSIGN_OR_RETURN(
      std::string uri_suffix,
      CreateAbortSecureAggregationUriSuffix(aggregation_id_, client_token_));

  AbortSecureAggregationRequest request;
  google::rpc::Status* status = request.mutable_status();
  status->set_code(google::rpc::Code::INTERNAL);
  status->set_message(abort_message.diagnostic_info());

  FCP_ASSIGN_OR_RETURN(
      std::unique_ptr<HttpRequest> http_request,
      secagg_request_creator_->CreateProtocolRequest(
          uri_suffix, QueryParams(), HttpRequest::Method::kPost,
          request.SerializeAsString(),
          /*is_protobuf_encoded=*/true));
  std::unique_ptr<InterruptibleRunner> delayed_interruptible_runner =
      delayed_interruptible_runner_creator_(clock_.Now() +
                                            waiting_period_for_cancellation_);
  FCP_ASSIGN_OR_RETURN(
      InMemoryHttpResponse response,
      request_helper_.PerformProtocolRequest(std::move(http_request),
                                             *delayed_interruptible_runner));

  secagg::ServerToClientWrapperMessage server_message;
  server_message.mutable_abort();
  return server_message;
}

absl::StatusOr<secagg::ServerToClientWrapperMessage>
HttpSecAggSendToServerImpl::DoR0AdvertiseKeys(
    secagg::AdvertiseKeys advertise_keys) {
  FCP_ASSIGN_OR_RETURN(
      std::string uri_suffix,
      CreateAdvertiseKeysUriSuffix(aggregation_id_, client_token_));

  AdvertiseKeysRequest request;
  *request.mutable_advertise_keys() = advertise_keys;
  FCP_ASSIGN_OR_RETURN(
      std::unique_ptr<HttpRequest> http_request,
      secagg_request_creator_->CreateProtocolRequest(
          uri_suffix, QueryParams(), HttpRequest::Method::kPost,
          request.SerializeAsString(),
          /*is_protobuf_encoded=*/true));
  FCP_ASSIGN_OR_RETURN(InMemoryHttpResponse response,
                       request_helper_.PerformProtocolRequest(
                           std::move(http_request), interruptible_runner_));
  FCP_ASSIGN_OR_RETURN(Operation initial_operation,
                       ParseOperationProtoFromHttpResponse(response));

  FCP_ASSIGN_OR_RETURN(
      Operation completed_operation,
      request_helper_.PollOperationResponseUntilDone(
          initial_operation, *secagg_request_creator_, interruptible_runner_));

  // The Operation has finished. Check if it resulted in an error, and if so
  // forward it after converting it to an absl::Status error.
  if (completed_operation.has_error()) {
    return ConvertRpcStatusToAbslStatus(completed_operation.error());
  }
  AdvertiseKeysResponse response_proto;
  if (!completed_operation.response().UnpackTo(&response_proto)) {
    return absl::InternalError("could not parse AdvertiseKeysResponse proto");
  }
  secagg::ServerToClientWrapperMessage server_message;
  *server_message.mutable_share_keys_request() =
      response_proto.share_keys_server_request();
  return server_message;
}

absl::StatusOr<secagg::ServerToClientWrapperMessage>
HttpSecAggSendToServerImpl::DoR1ShareKeys(
    secagg::ShareKeysResponse share_keys_response) {
  FCP_ASSIGN_OR_RETURN(
      std::string uri_suffix,
      CreateShareKeysUriSuffix(aggregation_id_, client_token_));

  ShareKeysRequest request;
  *request.mutable_share_keys_client_response() = share_keys_response;
  FCP_ASSIGN_OR_RETURN(
      std::unique_ptr<HttpRequest> http_request,
      secagg_request_creator_->CreateProtocolRequest(
          uri_suffix, QueryParams(), HttpRequest::Method::kPost,
          request.SerializeAsString(),
          /*is_protobuf_encoded=*/true));

  FCP_ASSIGN_OR_RETURN(InMemoryHttpResponse response,
                       request_helper_.PerformProtocolRequest(
                           std::move(http_request), interruptible_runner_));
  FCP_ASSIGN_OR_RETURN(Operation initial_operation,
                       ParseOperationProtoFromHttpResponse(response));

  FCP_ASSIGN_OR_RETURN(
      Operation completed_operation,
      request_helper_.PollOperationResponseUntilDone(
          initial_operation, *secagg_request_creator_, interruptible_runner_));

  // The Operation has finished. Check if it resulted in an error, and if so
  // forward it after converting it to an absl::Status error.
  if (completed_operation.has_error()) {
    return ConvertRpcStatusToAbslStatus(completed_operation.error());
  }
  ShareKeysResponse response_proto;
  if (!completed_operation.response().UnpackTo(&response_proto)) {
    return absl::InternalError(
        "could not parse StartSecureAggregationResponse proto");
  }
  secagg::ServerToClientWrapperMessage server_message;
  *server_message.mutable_masked_input_request() =
      response_proto.masked_input_collection_server_request();
  return server_message;
}

absl::StatusOr<secagg::ServerToClientWrapperMessage>
HttpSecAggSendToServerImpl::DoR2SubmitSecureAggregationResult(
    secagg::MaskedInputCollectionResponse masked_input_response) {
  std::vector<std::unique_ptr<HttpRequest>> requests;
  FCP_ASSIGN_OR_RETURN(std::string masked_result_upload_uri_suffix,
                       CreateByteStreamUploadUriSuffix(masked_resource_name_));

  FCP_ASSIGN_OR_RETURN(
      std::unique_ptr<HttpRequest> masked_input_upload_request,
      masked_result_upload_request_creator_->CreateProtocolRequest(
          masked_result_upload_uri_suffix, {{"upload_protocol", "raw"}},
          HttpRequest::Method::kPost,
          std::move(masked_input_response).SerializeAsString(),
          /*is_protobuf_encoded=*/false));
  requests.push_back(std::move(masked_input_upload_request));
  bool has_checkpoint = tf_checkpoint_.has_value();
  if (has_checkpoint) {
    FCP_ASSIGN_OR_RETURN(
        std::string nonmasked_result_upload_uri_suffix,
        CreateByteStreamUploadUriSuffix(nonmasked_resource_name_));
    FCP_ASSIGN_OR_RETURN(
        std::unique_ptr<HttpRequest> nonmasked_input_upload_request,
        nonmasked_result_upload_request_creator_->CreateProtocolRequest(
            nonmasked_result_upload_uri_suffix, {{"upload_protocol", "raw"}},
            HttpRequest::Method::kPost, std::move(tf_checkpoint_).value(),
            /*is_protobuf_encoded=*/false));
    requests.push_back(std::move(nonmasked_input_upload_request));
  }
  FCP_ASSIGN_OR_RETURN(
      std::vector<absl::StatusOr<InMemoryHttpResponse>> responses,
      request_helper_.PerformMultipleProtocolRequests(std::move(requests),
                                                      interruptible_runner_));
  for (const auto& response : responses) {
    if (!response.ok()) {
      return response.status();
    }
  }
  FCP_ASSIGN_OR_RETURN(std::string submit_result_uri_suffix,
                       CreateSubmitSecureAggregationResultUriSuffix(
                           aggregation_id_, client_token_));
  SubmitSecureAggregationResultRequest request;
  *request.mutable_masked_result_resource_name() = masked_resource_name_;
  if (has_checkpoint) {
    *request.mutable_nonmasked_result_resource_name() =
        nonmasked_resource_name_;
  }
  FCP_ASSIGN_OR_RETURN(
      std::unique_ptr<HttpRequest> submit_result_request,
      secagg_request_creator_->CreateProtocolRequest(
          submit_result_uri_suffix, QueryParams(), HttpRequest::Method::kPost,
          request.SerializeAsString(),
          /*is_protobuf_encoded=*/true));
  FCP_ASSIGN_OR_RETURN(
      InMemoryHttpResponse response,
      request_helper_.PerformProtocolRequest(std::move(submit_result_request),
                                             interruptible_runner_));
  FCP_ASSIGN_OR_RETURN(Operation initial_operation,
                       ParseOperationProtoFromHttpResponse(response));
  FCP_ASSIGN_OR_RETURN(
      Operation completed_operation,
      request_helper_.PollOperationResponseUntilDone(
          initial_operation, *secagg_request_creator_, interruptible_runner_));

  // The Operation has finished. Check if it resulted in an error, and if so
  // forward it after converting it to an absl::Status error.
  if (completed_operation.has_error()) {
    return ConvertRpcStatusToAbslStatus(completed_operation.error());
  }
  SubmitSecureAggregationResultResponse response_proto;
  if (!completed_operation.response().UnpackTo(&response_proto)) {
    return absl::InvalidArgumentError(
        "could not parse SubmitSecureAggregationResultResponse proto");
  }
  secagg::ServerToClientWrapperMessage server_message;
  *server_message.mutable_unmasking_request() =
      response_proto.unmasking_server_request();
  return server_message;
}

absl::StatusOr<secagg::ServerToClientWrapperMessage>
HttpSecAggSendToServerImpl::DoR3Unmask(
    secagg::UnmaskingResponse unmasking_response) {
  FCP_ASSIGN_OR_RETURN(std::string unmask_uri_suffix,
                       CreateUnmaskUriSuffix(aggregation_id_, client_token_));
  UnmaskRequest request;
  *request.mutable_unmasking_client_response() = unmasking_response;
  FCP_ASSIGN_OR_RETURN(
      std::unique_ptr<HttpRequest> unmask_request,
      secagg_request_creator_->CreateProtocolRequest(
          unmask_uri_suffix, QueryParams(), HttpRequest::Method::kPost,
          request.SerializeAsString(),
          /*is_protobuf_encoded=*/true));
  FCP_ASSIGN_OR_RETURN(InMemoryHttpResponse unmask_response,
                       request_helper_.PerformProtocolRequest(
                           std::move(unmask_request), interruptible_runner_));
  return secagg::ServerToClientWrapperMessage();
}

// TODO(team): remove GetModulus method, merge it into SecAggRunner.
absl::StatusOr<uint64_t> HttpSecAggProtocolDelegate::GetModulus(
    const std::string& key) {
  if (!secure_aggregands_.contains(key)) {
    return absl::InternalError(
        absl::StrCat("Execution not found for aggregand: ", key));
  }
  return secure_aggregands_[key].modulus();
}

absl::StatusOr<secagg::ServerToClientWrapperMessage>
HttpSecAggProtocolDelegate::ReceiveServerMessage() {
  return server_response_holder_;
}

void HttpSecAggProtocolDelegate::Abort() {
  // Intentional to be blank because we don't have internal states to clear.
}

size_t HttpSecAggProtocolDelegate::last_received_message_size() {
  if (server_response_holder_.ok()) {
    return server_response_holder_->ByteSizeLong();
  } else {
    // If the last request failed, return zero.
    return 0;
  }
}

}  // namespace http
}  // namespace client
}  // namespace fcp
