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

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "google/longrunning/operations.pb.h"
#include "google/rpc/code.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/random/random.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "fcp/base/clock.h"
#include "fcp/base/monitoring.h"
#include "fcp/base/simulated_clock.h"
#include "fcp/base/wall_clock_stopwatch.h"
#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/http/http_client.h"
#include "fcp/client/http/protocol_request_helper.h"
#include "fcp/client/http/testing/test_helpers.h"
#include "fcp/client/interruptible_runner.h"
#include "fcp/client/test_helpers.h"
#include "fcp/protos/federatedcompute/secure_aggregations.pb.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/testing/testing.h"

namespace fcp {
namespace client {
namespace http {
namespace {

using ::google::internal::federatedcompute::v1::AbortSecureAggregationRequest;
using ::google::internal::federatedcompute::v1::AbortSecureAggregationResponse;
using ::google::internal::federatedcompute::v1::AdvertiseKeysRequest;
using ::google::internal::federatedcompute::v1::AdvertiseKeysResponse;
using ::google::internal::federatedcompute::v1::ByteStreamResource;
using ::google::internal::federatedcompute::v1::ForwardingInfo;
using ::google::internal::federatedcompute::v1::SecureAggregandExecutionInfo;
using ::google::internal::federatedcompute::v1::ShareKeysRequest;
using ::google::internal::federatedcompute::v1::ShareKeysResponse;
using ::google::internal::federatedcompute::v1::
    SubmitSecureAggregationResultRequest;
using ::google::internal::federatedcompute::v1::
    SubmitSecureAggregationResultResponse;
using ::google::internal::federatedcompute::v1::UnmaskRequest;
using ::google::internal::federatedcompute::v1::UnmaskResponse;
using ::google::longrunning::Operation;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

constexpr absl::string_view kAggregationId = "aggregation_id";
constexpr absl::string_view kClientToken = "client_token";
constexpr absl::string_view kSecureAggregationTargetUri =
    "https://secureaggregation.uri/";
constexpr absl::string_view kByteStreamTargetUri = "https://bytestream.uri/";
constexpr absl::string_view kMaskedResourceName = "masked_resource";
constexpr absl::string_view kNonmaskedResourceName = "nonmasked_resource";
constexpr absl::string_view kOperationName = "my_operation";
constexpr absl::string_view kApiKey = "API_KEY";
constexpr absl::Duration kDelayedInterruptibleRunnerDeadline =
    absl::Seconds(10);

TEST(HttpSecAggProtocolDelegateTest, GetModulus) {
  absl::StatusOr<secagg::ServerToClientWrapperMessage> holder;
  std::string tensor_key = "tensor_1";
  google::protobuf::Map<std::string, SecureAggregandExecutionInfo> secure_aggregands;
  SecureAggregandExecutionInfo secure_aggregand_execution_info;
  secure_aggregand_execution_info.set_modulus(12345);
  secure_aggregands[tensor_key] = secure_aggregand_execution_info;
  HttpSecAggProtocolDelegate delegate(secure_aggregands, &holder);
  auto modulus = delegate.GetModulus(tensor_key);
  ASSERT_OK(modulus);
  ASSERT_EQ(*modulus, 12345);
}

TEST(HttpSecAggProtocolDelegateTest, GetModulusKeyNotFound) {
  absl::StatusOr<secagg::ServerToClientWrapperMessage> holder;
  google::protobuf::Map<std::string, SecureAggregandExecutionInfo> secure_aggregands;
  SecureAggregandExecutionInfo secure_aggregand_execution_info;
  secure_aggregand_execution_info.set_modulus(12345);
  secure_aggregands["tensor_1"] = secure_aggregand_execution_info;
  HttpSecAggProtocolDelegate delegate(secure_aggregands, &holder);
  ASSERT_THAT(delegate.GetModulus("do_not_exist"),
              IsCode(absl::StatusCode::kInternal));
}

TEST(HttpSecAggProtocolDelegateTest, ReceiveMessageOkResponse) {
  absl::StatusOr<secagg::ServerToClientWrapperMessage> holder;
  google::protobuf::Map<std::string, SecureAggregandExecutionInfo> secure_aggregands;
  HttpSecAggProtocolDelegate delegate(secure_aggregands, &holder);
  secagg::ServerToClientWrapperMessage server_response;
  server_response.mutable_masked_input_request()->add_encrypted_key_shares(
      "encrypted_key");
  holder = server_response;

  auto server_message = delegate.ReceiveServerMessage();
  ASSERT_OK(server_message);
  ASSERT_THAT(*server_message, EqualsProto(server_response));
  ASSERT_EQ(delegate.last_received_message_size(),
            server_response.ByteSizeLong());
}

TEST(HttpSecAggProtocolDelegateTest, ReceiveMessageErrorResponse) {
  absl::StatusOr<secagg::ServerToClientWrapperMessage> holder;
  google::protobuf::Map<std::string, SecureAggregandExecutionInfo> secure_aggregands;
  HttpSecAggProtocolDelegate delegate(secure_aggregands, &holder);
  holder = absl::InternalError("Something is broken.");

  ASSERT_THAT(delegate.ReceiveServerMessage(),
              IsCode(absl::StatusCode::kInternal));
  ASSERT_EQ(delegate.last_received_message_size(), 0);
}

class HttpSecAggSendToServerImplTest : public ::testing::Test {
 protected:
  void SetUp() override {
    request_helper_ = std::make_unique<ProtocolRequestHelper>(
        &http_client_, &bytes_downloaded_, &bytes_uploaded_,
        network_stopwatch_.get(), Clock::RealClock(), &bit_gen_,
        /*retry_max_attempts=*/0,
        /*retry_delay_ms=*/5000);
    runner_ = std::make_unique<InterruptibleRunner>(
        &log_manager_, []() { return false; },
        InterruptibleRunner::TimingConfig{
            .polling_period = absl::ZeroDuration(),
            .graceful_shutdown_period = absl::InfiniteDuration(),
            .extended_shutdown_period = absl::InfiniteDuration()},
        InterruptibleRunner::DiagnosticsConfig{
            .interrupted = ProdDiagCode::BACKGROUND_TRAINING_INTERRUPT_HTTP,
            .interrupt_timeout =
                ProdDiagCode::BACKGROUND_TRAINING_INTERRUPT_HTTP_TIMED_OUT,
            .interrupted_extended = ProdDiagCode::
                BACKGROUND_TRAINING_INTERRUPT_HTTP_EXTENDED_COMPLETED,
            .interrupt_timeout_extended = ProdDiagCode::
                BACKGROUND_TRAINING_INTERRUPT_HTTP_EXTENDED_TIMED_OUT});
    *secagg_upload_forwarding_info_.mutable_target_uri_prefix() =
        kSecureAggregationTargetUri;
    *masked_result_resource_.mutable_resource_name() = kMaskedResourceName;
    ForwardingInfo masked_resource_forwarding_info;
    *masked_resource_forwarding_info.mutable_target_uri_prefix() =
        kByteStreamTargetUri;
    *masked_result_resource_.mutable_data_upload_forwarding_info() =
        masked_resource_forwarding_info;
    *nonmasked_result_resource_.mutable_resource_name() =
        kNonmaskedResourceName;
    ForwardingInfo nonmasked_resource_forwarding_info;
    *nonmasked_resource_forwarding_info.mutable_target_uri_prefix() =
        kByteStreamTargetUri;
    *nonmasked_result_resource_.mutable_data_upload_forwarding_info() =
        nonmasked_resource_forwarding_info;
  }

  std::unique_ptr<InterruptibleRunner> CreateInterruptibleRunner() {
    return std::make_unique<InterruptibleRunner>(
        &log_manager_, [this]() { return interrupted_; },
        InterruptibleRunner::TimingConfig{
            .polling_period = absl::ZeroDuration(),
            .graceful_shutdown_period = absl::InfiniteDuration(),
            .extended_shutdown_period = absl::InfiniteDuration()},
        InterruptibleRunner::DiagnosticsConfig{
            .interrupted = ProdDiagCode::BACKGROUND_TRAINING_INTERRUPT_HTTP,
            .interrupt_timeout =
                ProdDiagCode::BACKGROUND_TRAINING_INTERRUPT_HTTP_TIMED_OUT,
            .interrupted_extended = ProdDiagCode::
                BACKGROUND_TRAINING_INTERRUPT_HTTP_EXTENDED_COMPLETED,
            .interrupt_timeout_extended = ProdDiagCode::
                BACKGROUND_TRAINING_INTERRUPT_HTTP_EXTENDED_TIMED_OUT});
  }

  std::unique_ptr<HttpSecAggSendToServerImpl> CreateSecAggSendToServer(
      std::optional<std::string> tf_checkpoint) {
    auto send_to_server = HttpSecAggSendToServerImpl::Create(
        kApiKey, &clock_, request_helper_.get(), runner_.get(),
        /* delayed_interruptible_runner_creator=*/
        [this](absl::Time deadline) {
          // Ensure that the HttpSecAggSendToServerImpl implementation correctly
          // passes a deadline that matches the 'waiting period' value we
          // provide below (with a 1s grace period to account for the delay of
          // executing the actual test; unfortunately the underlying HTTP code
          // currently still uses absl::Now() directly, so we're forced to deal
          // with 'real' time...).
          //
          // We don't actually use the deadline value though, since that
          // would only make testing more complicated.
          EXPECT_GE(deadline, absl::Now() +
                                  kDelayedInterruptibleRunnerDeadline -
                                  absl::Seconds(1));
          return CreateInterruptibleRunner();
        },
        &server_response_holder_, kAggregationId, kClientToken,
        secagg_upload_forwarding_info_, masked_result_resource_,
        nonmasked_result_resource_, tf_checkpoint,
        /* disable_request_body_compression=*/true,
        /* waiting_period_for_cancellation=*/
        kDelayedInterruptibleRunnerDeadline);
    FCP_CHECK(send_to_server.ok());
    return std::move(*send_to_server);
  }
  bool interrupted_ = false;
  // We set the simulated clock to "now", since a bunch of the HTTP-related FCP
  // code currently still uses absl::Now() directly, rather than using a more
  // testable "Clock" object. This ensures various timestamps we may encounter
  // are more understandable.
  SimulatedClock clock_ = SimulatedClock(absl::Now());
  absl::BitGen bit_gen_;
  StrictMock<MockHttpClient> http_client_;
  NiceMock<MockLogManager> log_manager_;
  int64_t bytes_downloaded_ = 0;
  int64_t bytes_uploaded_ = 0;
  std::unique_ptr<WallClockStopwatch> network_stopwatch_ =
      WallClockStopwatch::Create();
  std::unique_ptr<ProtocolRequestHelper> request_helper_;
  std::unique_ptr<InterruptibleRunner> runner_;
  absl::StatusOr<secagg::ServerToClientWrapperMessage> server_response_holder_;
  ForwardingInfo secagg_upload_forwarding_info_;
  ByteStreamResource masked_result_resource_;
  ByteStreamResource nonmasked_result_resource_;
};

TEST_F(HttpSecAggSendToServerImplTest, TestSendR0AdvertiseKeys) {
  std::unique_ptr<HttpSecAggSendToServerImpl> send_to_server =
      CreateSecAggSendToServer(std::optional<std::string>());
  secagg::ClientToServerWrapperMessage server_message;
  auto pair_of_keys =
      server_message.mutable_advertise_keys()->mutable_pair_of_public_keys();
  pair_of_keys->set_enc_pk("enc_pk");
  pair_of_keys->set_noise_pk("noise_pk");
  // Create expected request.
  AdvertiseKeysRequest expected_request;
  *expected_request.mutable_advertise_keys() = server_message.advertise_keys();

  EXPECT_CALL(
      http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://secureaggregation.uri/v1/secureaggregations/aggregation_id/"
          "clients/client_token:advertisekeys?%24alt=proto",
          HttpRequest::Method::kPost, _, expected_request.SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreatePendingOperation("operations/foo#bar").SerializeAsString())));
  // Create expected response.
  secagg::ShareKeysRequest share_keys_request;
  *share_keys_request.add_pairs_of_public_keys()->mutable_noise_pk() =
      "noise_pk";
  AdvertiseKeysResponse advertise_keys_response;
  *advertise_keys_response.mutable_share_keys_server_request() =
      share_keys_request;
  Operation complete_operation =
      CreateDoneOperation(kOperationName, advertise_keys_response);
  EXPECT_CALL(
      http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://secureaggregation.uri/v1/operations/foo%23bar?%24alt=proto",
          HttpRequest::Method::kGet, _, "")))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), complete_operation.SerializeAsString())));
  send_to_server->Send(&server_message);
  ASSERT_OK(server_response_holder_);

  secagg::ServerToClientWrapperMessage expected_message;
  *expected_message.mutable_share_keys_request() = share_keys_request;
  EXPECT_THAT(*server_response_holder_, EqualsProto(expected_message));
}

TEST_F(HttpSecAggSendToServerImplTest,
       TestSendR0AdvertiseKeysFailedImmediately) {
  std::unique_ptr<HttpSecAggSendToServerImpl> send_to_server =
      CreateSecAggSendToServer(std::optional<std::string>());
  secagg::ClientToServerWrapperMessage server_message;
  auto pair_of_keys =
      server_message.mutable_advertise_keys()->mutable_pair_of_public_keys();
  pair_of_keys->set_enc_pk("enc_pk");
  pair_of_keys->set_noise_pk("noise_pk");
  // Create expected request.
  AdvertiseKeysRequest expected_request;
  *expected_request.mutable_advertise_keys() = server_message.advertise_keys();

  EXPECT_CALL(
      http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://secureaggregation.uri/v1/secureaggregations/aggregation_id/"
          "clients/client_token:advertisekeys?%24alt=proto",
          HttpRequest::Method::kPost, _, expected_request.SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(503, HeaderList(), "")));
  send_to_server->Send(&server_message);
  EXPECT_THAT(server_response_holder_, IsCode(absl::StatusCode::kUnavailable));
}

TEST_F(HttpSecAggSendToServerImplTest, TestSendR0AdvertiseKeysFailed) {
  std::unique_ptr<HttpSecAggSendToServerImpl> send_to_server =
      CreateSecAggSendToServer(std::optional<std::string>());
  secagg::ClientToServerWrapperMessage server_message;
  auto pair_of_keys =
      server_message.mutable_advertise_keys()->mutable_pair_of_public_keys();
  pair_of_keys->set_enc_pk("enc_pk");
  pair_of_keys->set_noise_pk("noise_pk");
  // Create expected request.
  AdvertiseKeysRequest expected_request;
  *expected_request.mutable_advertise_keys() = server_message.advertise_keys();

  EXPECT_CALL(
      http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://secureaggregation.uri/v1/secureaggregations/aggregation_id/"
          "clients/client_token:advertisekeys?%24alt=proto",
          HttpRequest::Method::kPost, _, expected_request.SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreateErrorOperation(kOperationName, absl::StatusCode::kInternal,
                               "Something's wrong")
              .SerializeAsString())));
  send_to_server->Send(&server_message);
  EXPECT_THAT(server_response_holder_, IsCode(absl::StatusCode::kInternal));
}

TEST_F(HttpSecAggSendToServerImplTest, TestSendR1ShareKeys) {
  std::unique_ptr<HttpSecAggSendToServerImpl> send_to_server =
      CreateSecAggSendToServer(std::optional<std::string>());
  secagg::ClientToServerWrapperMessage server_message;
  server_message.mutable_share_keys_response()
      ->mutable_encrypted_key_shares()
      ->Add("encrypted_key");
  // Create expected request
  ShareKeysRequest expected_request;
  *expected_request.mutable_share_keys_client_response() =
      server_message.share_keys_response();

  EXPECT_CALL(
      http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://secureaggregation.uri/v1/secureaggregations/aggregation_id/"
          "clients/client_token:sharekeys?%24alt=proto",
          HttpRequest::Method::kPost, _, expected_request.SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreatePendingOperation("operations/foo#bar").SerializeAsString())));
  // Create expected response
  secagg::MaskedInputCollectionRequest masked_input_collection_request;
  masked_input_collection_request.add_encrypted_key_shares(
      "encryoted_key_share");
  ShareKeysResponse share_keys_response;
  *share_keys_response.mutable_masked_input_collection_server_request() =
      masked_input_collection_request;
  Operation complete_operation =
      CreateDoneOperation(kOperationName, share_keys_response);
  EXPECT_CALL(
      http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://secureaggregation.uri/v1/operations/foo%23bar?%24alt=proto",
          HttpRequest::Method::kGet, _, "")))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), complete_operation.SerializeAsString())));
  send_to_server->Send(&server_message);
  ASSERT_OK(server_response_holder_);

  secagg::ServerToClientWrapperMessage expected_message;
  *expected_message.mutable_masked_input_request() =
      masked_input_collection_request;
  EXPECT_THAT(*server_response_holder_, EqualsProto(expected_message));
}

TEST_F(HttpSecAggSendToServerImplTest, TestSendR1ShareKeysFailedImmediatedly) {
  std::unique_ptr<HttpSecAggSendToServerImpl> send_to_server =
      CreateSecAggSendToServer(std::optional<std::string>());
  secagg::ClientToServerWrapperMessage server_message;
  server_message.mutable_share_keys_response()
      ->mutable_encrypted_key_shares()
      ->Add("encrypted_key");
  // Create expected request
  ShareKeysRequest expected_request;
  *expected_request.mutable_share_keys_client_response() =
      server_message.share_keys_response();

  EXPECT_CALL(
      http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://secureaggregation.uri/v1/secureaggregations/aggregation_id/"
          "clients/client_token:sharekeys?%24alt=proto",
          HttpRequest::Method::kPost, _, expected_request.SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(503, HeaderList(), "")));
  send_to_server->Send(&server_message);
  EXPECT_THAT(server_response_holder_, IsCode(absl::StatusCode::kUnavailable));
}

TEST_F(HttpSecAggSendToServerImplTest, TestSendR1ShareKeysFailed) {
  std::unique_ptr<HttpSecAggSendToServerImpl> send_to_server =
      CreateSecAggSendToServer(std::optional<std::string>());
  secagg::ClientToServerWrapperMessage server_message;
  server_message.mutable_share_keys_response()
      ->mutable_encrypted_key_shares()
      ->Add("encrypted_key");
  // Create expected request
  ShareKeysRequest expected_request;
  *expected_request.mutable_share_keys_client_response() =
      server_message.share_keys_response();

  EXPECT_CALL(
      http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://secureaggregation.uri/v1/secureaggregations/aggregation_id/"
          "clients/client_token:sharekeys?%24alt=proto",
          HttpRequest::Method::kPost, _, expected_request.SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreateErrorOperation(kOperationName, absl::StatusCode::kInternal,
                               "Something's wrong")
              .SerializeAsString())));
  send_to_server->Send(&server_message);
  EXPECT_THAT(server_response_holder_, IsCode(absl::StatusCode::kInternal));
}

TEST_F(HttpSecAggSendToServerImplTest, TestSendR2SubmitResultNoCheckpoint) {
  std::unique_ptr<HttpSecAggSendToServerImpl> send_to_server =
      CreateSecAggSendToServer(std::optional<std::string>());
  secagg::ClientToServerWrapperMessage server_message;
  secagg::MaskedInputVector masked_vector;
  *masked_vector.mutable_encoded_vector() = "encoded_vector";
  auto vector_map =
      server_message.mutable_masked_input_response()->mutable_vectors();
  (*vector_map)["vector_1"] = masked_vector;
  // Create expected request
  SubmitSecureAggregationResultRequest expected_request;
  *expected_request.mutable_masked_result_resource_name() = kMaskedResourceName;

  // Create expected responses
  EXPECT_CALL(http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://bytestream.uri/upload/v1/media/"
                  "masked_resource?upload_protocol=raw",
                  HttpRequest::Method::kPost, _,
                  server_message.masked_input_response().SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(200, HeaderList(), "")));

  EXPECT_CALL(
      http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://secureaggregation.uri/v1/secureaggregations/aggregation_id/"
          "clients/client_token:submit?%24alt=proto",
          HttpRequest::Method::kPost, _, expected_request.SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreatePendingOperation("operations/foo#bar").SerializeAsString())));
  secagg::UnmaskingRequest unmasking_request;
  unmasking_request.add_dead_3_client_ids(12345);
  SubmitSecureAggregationResultResponse submit_secagg_result_response;
  *submit_secagg_result_response.mutable_unmasking_server_request() =
      unmasking_request;
  Operation complete_operation =
      CreateDoneOperation(kOperationName, submit_secagg_result_response);
  EXPECT_CALL(
      http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://secureaggregation.uri/v1/operations/foo%23bar?%24alt=proto",
          HttpRequest::Method::kGet, _, "")))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), complete_operation.SerializeAsString())));
  send_to_server->Send(&server_message);
  ASSERT_OK(server_response_holder_);

  secagg::ServerToClientWrapperMessage expected_message;
  *expected_message.mutable_unmasking_request() = unmasking_request;
  EXPECT_THAT(*server_response_holder_, EqualsProto(expected_message));
}

TEST_F(HttpSecAggSendToServerImplTest, TestSendR2SubmitResultWithCheckpoint) {
  std::string tf_checkpoint = "trained.ckpt";
  std::unique_ptr<HttpSecAggSendToServerImpl> send_to_server =
      CreateSecAggSendToServer(std::optional<std::string>(tf_checkpoint));
  secagg::ClientToServerWrapperMessage server_message;
  secagg::MaskedInputVector masked_vector;
  *masked_vector.mutable_encoded_vector() = "encoded_vector";
  auto vector_map =
      server_message.mutable_masked_input_response()->mutable_vectors();
  (*vector_map)["vector_1"] = masked_vector;
  // Create expected request
  SubmitSecureAggregationResultRequest expected_request;
  *expected_request.mutable_masked_result_resource_name() = kMaskedResourceName;
  *expected_request.mutable_nonmasked_result_resource_name() =
      kNonmaskedResourceName;

  // Create expected responses
  EXPECT_CALL(http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://bytestream.uri/upload/v1/media/"
                  "masked_resource?upload_protocol=raw",
                  HttpRequest::Method::kPost, _,
                  server_message.masked_input_response().SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(200, HeaderList(), "")));
  EXPECT_CALL(http_client_, PerformSingleRequest(SimpleHttpRequestMatcher(
                                "https://bytestream.uri/upload/v1/media/"
                                "nonmasked_resource?upload_protocol=raw",
                                HttpRequest::Method::kPost, _, tf_checkpoint)))
      .WillOnce(Return(FakeHttpResponse(200, HeaderList(), "")));

  secagg::UnmaskingRequest unmasking_request;
  unmasking_request.add_dead_3_client_ids(12345);
  SubmitSecureAggregationResultResponse submit_secagg_result_response;
  *submit_secagg_result_response.mutable_unmasking_server_request() =
      unmasking_request;
  Operation complete_operation =
      CreateDoneOperation(kOperationName, submit_secagg_result_response);
  EXPECT_CALL(
      http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://secureaggregation.uri/v1/secureaggregations/aggregation_id/"
          "clients/client_token:submit?%24alt=proto",
          HttpRequest::Method::kPost, _, expected_request.SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), complete_operation.SerializeAsString())));
  send_to_server->Send(&server_message);
  ASSERT_OK(server_response_holder_);

  secagg::ServerToClientWrapperMessage expected_message;
  *expected_message.mutable_unmasking_request() = unmasking_request;
  EXPECT_THAT(*server_response_holder_, EqualsProto(expected_message));
}

TEST_F(HttpSecAggSendToServerImplTest,
       TestSendR2SubmitResultWithCheckpointUploadFailed) {
  std::string tf_checkpoint = "trained.ckpt";
  std::unique_ptr<HttpSecAggSendToServerImpl> send_to_server =
      CreateSecAggSendToServer(std::optional<std::string>(tf_checkpoint));
  secagg::ClientToServerWrapperMessage server_message;
  secagg::MaskedInputVector masked_vector;
  *masked_vector.mutable_encoded_vector() = "encoded_vector";
  auto vector_map =
      server_message.mutable_masked_input_response()->mutable_vectors();
  (*vector_map)["vector_1"] = masked_vector;
  // Create expected request
  SubmitSecureAggregationResultRequest expected_request;
  *expected_request.mutable_masked_result_resource_name() = kMaskedResourceName;
  *expected_request.mutable_nonmasked_result_resource_name() =
      kNonmaskedResourceName;

  // Fail one of the upload
  EXPECT_CALL(http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://bytestream.uri/upload/v1/media/"
                  "masked_resource?upload_protocol=raw",
                  HttpRequest::Method::kPost, _,
                  server_message.masked_input_response().SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(503, HeaderList(), "")));
  EXPECT_CALL(http_client_, PerformSingleRequest(SimpleHttpRequestMatcher(
                                "https://bytestream.uri/upload/v1/media/"
                                "nonmasked_resource?upload_protocol=raw",
                                HttpRequest::Method::kPost, _, tf_checkpoint)))
      .WillOnce(Return(FakeHttpResponse(200, HeaderList(), "")));

  send_to_server->Send(&server_message);
  EXPECT_THAT(server_response_holder_, IsCode(absl::StatusCode::kUnavailable));
}

TEST_F(HttpSecAggSendToServerImplTest,
       TestSendR2SubmitResultWithCheckpointSubmitResultFailedImmediately) {
  std::string tf_checkpoint = "trained.ckpt";
  std::unique_ptr<HttpSecAggSendToServerImpl> send_to_server =
      CreateSecAggSendToServer(std::optional<std::string>(tf_checkpoint));
  secagg::ClientToServerWrapperMessage server_message;
  secagg::MaskedInputVector masked_vector;
  masked_vector.set_encoded_vector("encoded_vector");
  auto vector_map =
      server_message.mutable_masked_input_response()->mutable_vectors();
  (*vector_map)["vector_1"] = masked_vector;
  // Create expected request
  SubmitSecureAggregationResultRequest expected_request;
  *expected_request.mutable_masked_result_resource_name() = kMaskedResourceName;
  *expected_request.mutable_nonmasked_result_resource_name() =
      kNonmaskedResourceName;

  // Create expected responses
  EXPECT_CALL(http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://bytestream.uri/upload/v1/media/"
                  "masked_resource?upload_protocol=raw",
                  HttpRequest::Method::kPost, _,
                  server_message.masked_input_response().SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(200, HeaderList(), "")));
  EXPECT_CALL(http_client_, PerformSingleRequest(SimpleHttpRequestMatcher(
                                "https://bytestream.uri/upload/v1/media/"
                                "nonmasked_resource?upload_protocol=raw",
                                HttpRequest::Method::kPost, _, tf_checkpoint)))
      .WillOnce(Return(FakeHttpResponse(200, HeaderList(), "")));

  secagg::UnmaskingRequest unmasking_request;
  unmasking_request.add_dead_3_client_ids(12345);
  SubmitSecureAggregationResultResponse submit_secagg_result_response;
  *submit_secagg_result_response.mutable_unmasking_server_request() =
      unmasking_request;
  Operation complete_operation =
      CreateDoneOperation(kOperationName, submit_secagg_result_response);
  EXPECT_CALL(
      http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://secureaggregation.uri/v1/secureaggregations/aggregation_id/"
          "clients/client_token:submit?%24alt=proto",
          HttpRequest::Method::kPost, _, expected_request.SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(503, HeaderList(), "")));
  send_to_server->Send(&server_message);
  EXPECT_THAT(server_response_holder_, IsCode(absl::StatusCode::kUnavailable));
}

TEST_F(HttpSecAggSendToServerImplTest,
       TestSendR2SubmitResultWithCheckpointSubmitResultFailed) {
  std::string tf_checkpoint = "trained.ckpt";
  std::unique_ptr<HttpSecAggSendToServerImpl> send_to_server =
      CreateSecAggSendToServer(std::optional<std::string>(tf_checkpoint));
  secagg::ClientToServerWrapperMessage server_message;
  secagg::MaskedInputVector masked_vector;
  masked_vector.set_encoded_vector("encoded_vector");
  auto vector_map =
      server_message.mutable_masked_input_response()->mutable_vectors();
  (*vector_map)["vector_1"] = masked_vector;
  // Create expected request
  SubmitSecureAggregationResultRequest expected_request;
  *expected_request.mutable_masked_result_resource_name() = kMaskedResourceName;
  *expected_request.mutable_nonmasked_result_resource_name() =
      kNonmaskedResourceName;

  // Create expected responses
  EXPECT_CALL(http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://bytestream.uri/upload/v1/media/"
                  "masked_resource?upload_protocol=raw",
                  HttpRequest::Method::kPost, _,
                  server_message.masked_input_response().SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(200, HeaderList(), "")));
  EXPECT_CALL(http_client_, PerformSingleRequest(SimpleHttpRequestMatcher(
                                "https://bytestream.uri/upload/v1/media/"
                                "nonmasked_resource?upload_protocol=raw",
                                HttpRequest::Method::kPost, _, tf_checkpoint)))
      .WillOnce(Return(FakeHttpResponse(200, HeaderList(), "")));

  EXPECT_CALL(
      http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://secureaggregation.uri/v1/secureaggregations/aggregation_id/"
          "clients/client_token:submit?%24alt=proto",
          HttpRequest::Method::kPost, _, expected_request.SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreatePendingOperation("operations/foo#bar").SerializeAsString())));

  secagg::UnmaskingRequest unmasking_request;
  unmasking_request.add_dead_3_client_ids(12345);
  SubmitSecureAggregationResultResponse submit_secagg_result_response;
  *submit_secagg_result_response.mutable_unmasking_server_request() =
      unmasking_request;
  Operation complete_operation =
      CreateDoneOperation(kOperationName, submit_secagg_result_response);
  EXPECT_CALL(
      http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://secureaggregation.uri/v1/operations/foo%23bar?%24alt=proto",
          HttpRequest::Method::kGet, _, "")))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreateErrorOperation(kOperationName, absl::StatusCode::kInternal,
                               "Something's wroing.")
              .SerializeAsString())));
  send_to_server->Send(&server_message);
  EXPECT_THAT(server_response_holder_, IsCode(absl::StatusCode::kInternal));
}

TEST_F(HttpSecAggSendToServerImplTest, TestSendR3Unmask) {
  std::unique_ptr<HttpSecAggSendToServerImpl> send_to_server =
      CreateSecAggSendToServer(std::optional<std::string>());
  secagg::ClientToServerWrapperMessage server_message;
  server_message.mutable_unmasking_response()
      ->add_noise_or_prf_key_shares()
      ->set_noise_sk_share("noise_sk_share");
  // Create expected request
  UnmaskRequest expected_request;
  *expected_request.mutable_unmasking_client_response() =
      server_message.unmasking_response();

  // Create expected response
  EXPECT_CALL(
      http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://secureaggregation.uri/v1/secureaggregations/aggregation_id/"
          "clients/client_token:unmask?%24alt=proto",
          HttpRequest::Method::kPost, _, expected_request.SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(200, HeaderList(),
                                        UnmaskResponse().SerializeAsString())));
  send_to_server->Send(&server_message);
  auto response = server_response_holder_;
  ASSERT_OK(response);
  EXPECT_THAT(*response, EqualsProto(secagg::ServerToClientWrapperMessage()));
}

TEST_F(HttpSecAggSendToServerImplTest, TestSendAbortWithoutInterruption) {
  std::unique_ptr<HttpSecAggSendToServerImpl> send_to_server =
      CreateSecAggSendToServer(std::optional<std::string>());
  std::string diagnostic_info = "Some computation failed.";
  secagg::ClientToServerWrapperMessage server_message;
  server_message.mutable_abort()->set_diagnostic_info(diagnostic_info);
  // Create expected request
  AbortSecureAggregationRequest expected_request;
  google::rpc::Status status;
  status.set_message(diagnostic_info);
  status.set_code(google::rpc::INTERNAL);
  *expected_request.mutable_status() = status;

  // We expect the abort request to actually be issued, because interrupted_ is
  // set to false, and hence the "InterruptibleRunner" we provided at the top of
  // the test should let the request go through.
  EXPECT_CALL(
      http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://secureaggregation.uri/v1/secureaggregations/aggregation_id/"
          "clients/client_token:abort?%24alt=proto",
          HttpRequest::Method::kPost, _, expected_request.SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          AbortSecureAggregationResponse().SerializeAsString())));

  // Send the request, and verify that sending it succeeded.
  send_to_server->Send(&server_message);
  ASSERT_OK(server_response_holder_);
  secagg::ServerToClientWrapperMessage expected_response;
  expected_response.mutable_abort();
  EXPECT_THAT(*server_response_holder_, EqualsProto(expected_response));
}

TEST_F(HttpSecAggSendToServerImplTest,
       TestSendAbortShouldBeCancelledIfAlreadyInterruptedForTooLong) {
  std::unique_ptr<HttpSecAggSendToServerImpl> send_to_server =
      CreateSecAggSendToServer(std::optional<std::string>());
  std::string diagnostic_info = "Some computation failed.";
  secagg::ClientToServerWrapperMessage server_message;
  server_message.mutable_abort()->set_diagnostic_info(diagnostic_info);
  // Create expected request
  AbortSecureAggregationRequest expected_request;
  google::rpc::Status status;
  status.set_message(diagnostic_info);
  status.set_code(google::rpc::INTERNAL);
  *expected_request.mutable_status() = status;

  // We do *not* expect any HTTP request to actually be issued, since the
  // interrupted_ flag is true, and therefore the request should be cancelled
  // before it is even issued.
  interrupted_ = true;

  // Send the request, and verify that sending it failed.
  send_to_server->Send(&server_message);
  EXPECT_THAT(server_response_holder_, IsCode(absl::StatusCode::kCancelled));
}

}  // anonymous namespace
}  // namespace http
}  // namespace client
}  // namespace fcp
