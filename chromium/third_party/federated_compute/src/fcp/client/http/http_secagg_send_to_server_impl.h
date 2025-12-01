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
#ifndef FCP_CLIENT_HTTP_HTTP_SECAGG_SEND_TO_SERVER_IMPL_H_
#define FCP_CLIENT_HTTP_HTTP_SECAGG_SEND_TO_SERVER_IMPL_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "fcp/base/clock.h"
#include "fcp/client/http/protocol_request_helper.h"
#include "fcp/client/interruptible_runner.h"
#include "fcp/client/secagg_runner.h"
#include "fcp/protos/federatedcompute/common.pb.h"
#include "fcp/protos/federatedcompute/secure_aggregations.pb.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"

namespace fcp {
namespace client {
namespace http {

// Implementation of SecAggSendToServerBase for HTTP federated protocol.
class HttpSecAggSendToServerImpl : public SecAggSendToServerBase {
 public:
  // Create an instance of HttpSecAggSendToServerImpl.
  // This method returns error status when failed to create
  // ProtocolRequestCreator based on the input ForwardingInfo or
  // ByteStreamResources.
  static absl::StatusOr<std::unique_ptr<HttpSecAggSendToServerImpl>> Create(
      absl::string_view api_key, Clock* clock,
      ProtocolRequestHelper* request_helper,
      InterruptibleRunner* interruptible_runner,
      std::function<std::unique_ptr<InterruptibleRunner>(absl::Time)>
          delayed_interruptible_runner_creator,
      absl::StatusOr<secagg::ServerToClientWrapperMessage>*
          server_response_holder,
      absl::string_view aggregation_id, absl::string_view client_token,
      const google::internal::federatedcompute::v1::ForwardingInfo&
          secagg_upload_forwarding_info,
      const google::internal::federatedcompute::v1::ByteStreamResource&
          masked_result_resource,
      const google::internal::federatedcompute::v1::ByteStreamResource&
          nonmasked_result_resource,
      std::optional<std::string> tf_checkpoint,
      bool disable_request_body_compression,
      absl::Duration waiting_period_for_cancellation);
  ~HttpSecAggSendToServerImpl() override = default;

  // Sends a client to server request based on the
  // secagg::ClientToServerWrapperMessage, waits for the response, and set it to
  // the server response holder.
  void Send(secagg::ClientToServerWrapperMessage* message) override;

 private:
  HttpSecAggSendToServerImpl(
      absl::string_view api_key, Clock* clock,
      ProtocolRequestHelper* request_helper,
      InterruptibleRunner* interruptible_runner,
      std::function<std::unique_ptr<InterruptibleRunner>(absl::Time)>
          delayed_interruptible_runner_creator,
      absl::StatusOr<secagg::ServerToClientWrapperMessage>*
          server_response_holder,
      absl::string_view aggregation_id, absl::string_view client_token,
      absl::string_view masked_resource_name,
      absl::string_view nonmasked_resource_name,
      std::unique_ptr<ProtocolRequestCreator> secagg_request_creator,
      std::unique_ptr<ProtocolRequestCreator>
          masked_result_upload_request_creator,
      std::unique_ptr<ProtocolRequestCreator>
          nonmasked_result_upload_request_creator,
      std::optional<std::string> tf_checkpoint,
      absl::Duration waiting_period_for_cancellation)
      : api_key_(api_key),
        clock_(*clock),
        request_helper_(*request_helper),
        interruptible_runner_(*interruptible_runner),
        delayed_interruptible_runner_creator_(
            delayed_interruptible_runner_creator),
        server_response_holder_(*server_response_holder),
        aggregation_id_(std::string(aggregation_id)),
        client_token_(std::string(client_token)),
        masked_resource_name_(std::string(masked_resource_name)),
        nonmasked_resource_name_(std::string(nonmasked_resource_name)),
        secagg_request_creator_(std::move(secagg_request_creator)),
        masked_result_upload_request_creator_(
            std::move(masked_result_upload_request_creator)),
        nonmasked_result_upload_request_creator_(
            std::move(nonmasked_result_upload_request_creator)),
        tf_checkpoint_(std::move(tf_checkpoint)),
        waiting_period_for_cancellation_(waiting_period_for_cancellation) {}

  // Sends an AbortSecureAggregationRequest.
  absl::StatusOr<secagg::ServerToClientWrapperMessage> AbortSecureAggregation(
      secagg::AbortMessage abort_message);
  // Sends an AdvertiseKeysRequest and waits for the AdvertiseKeysResponse,
  // polling the corresponding LRO if needed.
  absl::StatusOr<secagg::ServerToClientWrapperMessage> DoR0AdvertiseKeys(
      secagg::AdvertiseKeys advertise_keys);
  // Sends an ShareKeysRequest and waits for the ShareKeysResponse, polling
  // the corresponding LRO if needed.
  absl::StatusOr<secagg::ServerToClientWrapperMessage> DoR1ShareKeys(
      secagg::ShareKeysResponse share_keys_response);
  // Uploads masked resource and (optional) nonmasked resource. After successful
  // upload, sends an SubmitSecureAggregationResultRequest and waits for the
  // SubmitSecureAggregationResultResponse, polling the corresponding LRO if
  // needed.
  absl::StatusOr<secagg::ServerToClientWrapperMessage>
  DoR2SubmitSecureAggregationResult(
      secagg::MaskedInputCollectionResponse masked_input_response);
  // Sends an UnmaskRequest and waits for the UnmaskResponse.
  absl::StatusOr<secagg::ServerToClientWrapperMessage> DoR3Unmask(
      secagg::UnmaskingResponse unmasking_response);
  const std::string api_key_;
  Clock& clock_;
  ProtocolRequestHelper& request_helper_;
  InterruptibleRunner& interruptible_runner_;
  std::function<std::unique_ptr<InterruptibleRunner>(absl::Time)>
      delayed_interruptible_runner_creator_;
  absl::StatusOr<secagg::ServerToClientWrapperMessage>& server_response_holder_;
  std::string aggregation_id_;
  std::string client_token_;
  std::string masked_resource_name_;
  std::string nonmasked_resource_name_;
  std::unique_ptr<ProtocolRequestCreator> secagg_request_creator_;
  std::unique_ptr<ProtocolRequestCreator> masked_result_upload_request_creator_;
  std::unique_ptr<ProtocolRequestCreator>
      nonmasked_result_upload_request_creator_;
  std::optional<std::string> tf_checkpoint_;
  absl::Duration waiting_period_for_cancellation_;
};

// Implementation of SecAggProtocolDelegate for the HTTP federated protocol.
class HttpSecAggProtocolDelegate : public SecAggProtocolDelegate {
 public:
  HttpSecAggProtocolDelegate(
      google::protobuf::Map<
          std::string,
          google::internal::federatedcompute::v1::SecureAggregandExecutionInfo>
          secure_aggregands,
      absl::StatusOr<secagg::ServerToClientWrapperMessage>*
          server_response_holder)
      : secure_aggregands_(std::move(secure_aggregands)),
        server_response_holder_(*server_response_holder) {}
  // Retrieve the modulus for a given SecAgg vector.
  absl::StatusOr<uint64_t> GetModulus(const std::string& key) override;
  // Receive Server message.
  absl::StatusOr<secagg::ServerToClientWrapperMessage> ReceiveServerMessage()
      override;
  // Called when the SecAgg protocol is interrupted.
  void Abort() override;
  size_t last_received_message_size() override;

 private:
  google::protobuf::Map<
      std::string,
      google::internal::federatedcompute::v1::SecureAggregandExecutionInfo>
      secure_aggregands_;
  absl::StatusOr<secagg::ServerToClientWrapperMessage>& server_response_holder_;
};

}  // namespace http
}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_HTTP_HTTP_SECAGG_SEND_TO_SERVER_IMPL_H_
