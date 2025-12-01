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

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/random/random.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "fcp/base/clock.h"
#include "fcp/base/monitoring.h"
#include "fcp/base/time_util.h"
#include "fcp/base/wall_clock_stopwatch.h"
#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/http/http_client.h"
#include "fcp/client/http/in_memory_request_response.h"
#include "fcp/client/http/testing/test_helpers.h"
#include "fcp/client/interruptible_runner.h"
#include "fcp/client/test_helpers.h"
#include "fcp/protos/federatedcompute/secure_aggregations.pb.h"
#include "fcp/protos/federatedcompute/task_assignments.pb.h"
#include "fcp/testing/testing.h"

namespace fcp {
namespace client {
namespace http {
namespace {

using ::google::internal::federatedcompute::v1::AdvertiseKeysMetadata;
using ::google::internal::federatedcompute::v1::ForwardingInfo;
using ::google::internal::federatedcompute::v1::ShareKeysMetadata;
using ::google::internal::federatedcompute::v1::StartTaskAssignmentMetadata;
using ::google::internal::federatedcompute::v1::
    SubmitSecureAggregationResultMetadata;
using ::google::longrunning::Operation;
using ::google::protobuf::Any;
using ::testing::_;
using ::testing::ContainerEq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::UnorderedElementsAre;

constexpr absl::string_view kApiKey = "API_KEY";

class MockClock : public Clock {
 public:
  MOCK_METHOD(absl::Time, Now, (), (override));
  MOCK_METHOD(void, Sleep, (absl::Duration duration), (override));

 protected:
  MOCK_METHOD(absl::Time, NowLocked, (), (override));
  MOCK_METHOD(void, ScheduleWakeup, (absl::Time wakeup_time), (override));
};

void VerifyInMemoryHttpResponse(const InMemoryHttpResponse& response, int code,
                                absl::string_view content_encoding,
                                absl::string_view body) {
  EXPECT_EQ(response.code, code);
  EXPECT_EQ(response.content_encoding, content_encoding);
  EXPECT_EQ(response.body, body);
}

Operation CreatePendingOperation(const std::string operation_name) {
  Operation operation;
  operation.set_done(false);
  operation.set_name(operation_name);
  return operation;
}

Operation CreatePendingOperation(const std::string operation_name,
                                 const Any& metadata) {
  Operation operation;
  operation.set_done(false);
  operation.set_name(operation_name);
  *operation.mutable_metadata() = metadata;
  return operation;
}

// Creates a 'done' `Operation`, with the given already-packed-into-`Any`
// result.
Operation CreateDoneOperation(const Any& packed_inner_result) {
  Operation operation;
  operation.set_done(true);
  *operation.mutable_response() = packed_inner_result;
  return operation;
}

Operation CreateErrorOperation(const absl::StatusCode error_code,
                               const std::string error_message) {
  Operation operation;
  operation.set_done(true);
  operation.mutable_error()->set_code(static_cast<int>(error_code));
  operation.mutable_error()->set_message(error_message);
  return operation;
}

TEST(ProtocolRequestCreatorTest, TestInvalidForwardingInfo) {
  // If a ForwardingInfo does not have a target_uri_prefix field set then the
  // ProcessForwardingInfo call should fail.
  ForwardingInfo forwarding_info;
  EXPECT_THAT(ProtocolRequestCreator::Create(kApiKey, forwarding_info,
                                             /*use_compression=*/false),
              IsCode(INVALID_ARGUMENT));

  (*forwarding_info.mutable_extra_request_headers())["x-header1"] =
      "header-value1";
  EXPECT_THAT(ProtocolRequestCreator::Create(kApiKey, forwarding_info,
                                             /*use_compression=*/false),
              IsCode(INVALID_ARGUMENT));
}

TEST(ProtocolRequestCreatorTest, CreateProtocolRequestInvalidSuffix) {
  ProtocolRequestCreator creator("https://initial.uri", kApiKey, HeaderList(),
                                 /*use_compression=*/false);
  std::string uri_suffix = "v1/request";
  ASSERT_THAT(
      creator.CreateProtocolRequest(uri_suffix, QueryParams(),
                                    HttpRequest::Method::kPost, "request_body",
                                    /*is_protobuf_encoded=*/false),
      IsCode(absl::StatusCode::kInvalidArgument));
}

TEST(ProtocolRequestCreatorTest, CreateProtocolRequest) {
  ProtocolRequestCreator creator("https://initial.uri", kApiKey, HeaderList(),
                                 /*use_compression=*/false);
  std::string expected_body = "expected_body";
  auto request = creator.CreateProtocolRequest(
      "/v1/request", QueryParams(), HttpRequest::Method::kPost, expected_body,
      /*is_protobuf_encoded=*/false);

  ASSERT_OK(request);
  EXPECT_EQ((*request)->uri(), "https://initial.uri/v1/request");
  EXPECT_EQ((*request)->method(), HttpRequest::Method::kPost);
  EXPECT_THAT(
      (*request)->extra_headers(),
      UnorderedElementsAre(
          Header{"x-goog-api-key", "API_KEY"},
          Header{"Content-Length", std::to_string(expected_body.size())}));
  EXPECT_TRUE((*request)->HasBody());
  std::string actual_body;
  actual_body.resize(expected_body.size());
  ASSERT_OK((*request)->ReadBody(actual_body.data(), expected_body.size()));
  EXPECT_EQ(actual_body, expected_body);
}

TEST(ProtocolRequestCreatorTest, CreateProtocolRequestWithAdditionalHeaders) {
  ProtocolRequestCreator creator("https://initial.uri", kApiKey, HeaderList(),
                                 /*use_compression=*/false);
  std::string expected_body = "expected_body";
  auto request = creator.CreateProtocolRequestWithAdditionalHeaders(
      "/v1/request", QueryParams(), HttpRequest::Method::kPost, expected_body,
      /*is_protobuf_encoded=*/false,
      HeaderList{{"x-header1", "header-value1"}});

  ASSERT_OK(request);
  EXPECT_EQ((*request)->uri(), "https://initial.uri/v1/request");
  EXPECT_EQ((*request)->method(), HttpRequest::Method::kPost);
  EXPECT_THAT(
      (*request)->extra_headers(),
      UnorderedElementsAre(
          Header{"x-goog-api-key", "API_KEY"},
          Header{"x-header1", "header-value1"},
          Header{"Content-Length", std::to_string(expected_body.size())}));
  EXPECT_TRUE((*request)->HasBody());
  std::string actual_body;
  actual_body.resize(expected_body.size());
  ASSERT_OK((*request)->ReadBody(actual_body.data(), expected_body.size()));
  EXPECT_EQ(actual_body, expected_body);
}

TEST(ProtocolRequestCreatorTest, CreateProtobufEncodedProtocolRequest) {
  ProtocolRequestCreator creator("https://initial.uri", kApiKey, HeaderList(),
                                 /*use_compression=*/false);
  std::string expected_body = "expected_body";
  auto request = creator.CreateProtocolRequest(
      "/v1/request", QueryParams(), HttpRequest::Method::kPost, expected_body,
      /*is_protobuf_encoded=*/true);

  ASSERT_OK(request);
  EXPECT_EQ((*request)->uri(), "https://initial.uri/v1/request?%24alt=proto");
  EXPECT_EQ((*request)->method(), HttpRequest::Method::kPost);
  EXPECT_THAT((*request)->extra_headers(),
              UnorderedElementsAre(
                  Header{"x-goog-api-key", "API_KEY"},
                  Header{"Content-Length", absl::StrCat(expected_body.size())},
                  Header{"Content-Type", "application/x-protobuf"}));
  EXPECT_TRUE((*request)->HasBody());
  std::string actual_body;
  actual_body.resize(expected_body.size());
  ASSERT_OK((*request)->ReadBody(actual_body.data(), actual_body.size()));
  EXPECT_EQ(actual_body, expected_body);
}

TEST(ProtocolRequestCreatorTest, CreateGetOperationRequest) {
  ProtocolRequestCreator creator("https://initial.uri", kApiKey, HeaderList(),
                                 /*use_compression=*/false);
  std::string operation_name = "my_operation";
  auto request = creator.CreateGetOperationRequest(operation_name);
  ASSERT_OK(request);
  EXPECT_EQ((*request)->uri(),
            "https://initial.uri/v1/my_operation?%24alt=proto");
  EXPECT_EQ((*request)->method(), HttpRequest::Method::kGet);
  EXPECT_THAT((*request)->extra_headers(),
              UnorderedElementsAre(Header{"x-goog-api-key", "API_KEY"}));
  EXPECT_FALSE((*request)->HasBody());
}

TEST(ProtocolRequestCreatorTest, CreateCancelOperationRequest) {
  ProtocolRequestCreator creator("https://initial.uri", kApiKey, HeaderList(),
                                 /*use_compression=*/false);
  std::string operation_name = "my_operation";
  auto request = creator.CreateCancelOperationRequest(operation_name);
  ASSERT_OK(request);
  EXPECT_EQ((*request)->uri(),
            "https://initial.uri/v1/my_operation:cancel?%24alt=proto");
  EXPECT_EQ((*request)->method(), HttpRequest::Method::kPost);
  EXPECT_THAT((*request)->extra_headers(),
              UnorderedElementsAre(Header{"x-goog-api-key", "API_KEY"}));
  EXPECT_FALSE((*request)->HasBody());
}

class ProtocolRequestHelperTest : public ::testing::Test {
 public:
  ProtocolRequestHelperTest()
      : interruptible_runner_(
            &mock_log_manager_, mock_should_abort_.AsStdFunction(),
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
                    BACKGROUND_TRAINING_INTERRUPT_HTTP_EXTENDED_TIMED_OUT}),
        initial_request_creator_("https://initial.uri", kApiKey, HeaderList(),
                                 /*use_compression=*/false),
        protocol_request_helper_(&mock_http_client_, &bytes_downloaded_,
                                 &bytes_uploaded_, network_stopwatch_.get(),
                                 &mock_clock_, &bit_gen_, 0, 0) {}

 protected:
  void TearDown() override {
    // Regardless of the outcome of the test (or the protocol interaction being
    // tested), network usage must always be reflected in the network stats.
    HttpRequestHandle::SentReceivedBytes sent_received_bytes =
        mock_http_client_.TotalSentReceivedBytes();
    EXPECT_THAT(bytes_downloaded_, sent_received_bytes.received_bytes);
    EXPECT_THAT(bytes_uploaded_, sent_received_bytes.sent_bytes);
  }
  absl::BitGen bit_gen_;
  StrictMock<MockClock> mock_clock_;
  StrictMock<MockHttpClient> mock_http_client_;

  NiceMock<MockLogManager> mock_log_manager_;
  NiceMock<MockFunction<bool()>> mock_should_abort_;

  int64_t bytes_downloaded_ = 0;
  int64_t bytes_uploaded_ = 0;
  std::unique_ptr<WallClockStopwatch> network_stopwatch_ =
      WallClockStopwatch::Create();

  InterruptibleRunner interruptible_runner_;
  ProtocolRequestCreator initial_request_creator_;
  // The class under test.
  ProtocolRequestHelper protocol_request_helper_;
};

Any GetFakeAnyProto() {
  Any fake_any;
  fake_any.set_type_url("the_type_url");
  *fake_any.mutable_value() = "the_value";
  return fake_any;
}

TEST_F(ProtocolRequestHelperTest, TestForwardingInfoIsPassedAlongCorrectly) {
  // The initial request should use the initial entry point URI and an empty set
  // of headers.
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://initial.uri/suffix1", HttpRequest::Method::kPost,
                  // This request has a response body, so the HttpClient will
                  // add this header automatically.
                  ContainerEq(HeaderList{{"x-goog-api-key", "API_KEY"},
                                         {"Content-Length", "5"}}),
                  "body1")))
      .WillOnce(Return(FakeHttpResponse(200, HeaderList(), "response1")));
  auto request_creator = std::make_unique<ProtocolRequestCreator>(
      "https://initial.uri", kApiKey, HeaderList(),
      /*use_compression=*/false);
  auto http_request = request_creator->CreateProtocolRequest(
      "/suffix1", QueryParams(), HttpRequest::Method::kPost, "body1",
      /*is_protobuf_encoded=*/false);
  ASSERT_OK(http_request);
  auto result = protocol_request_helper_.PerformProtocolRequest(
      *std::move(http_request), interruptible_runner_);
  ASSERT_OK(result);
  VerifyInMemoryHttpResponse(*result, 200, "", "response1");

  {
    // Process some fake ForwardingInfo.
    ForwardingInfo forwarding_info1;
    forwarding_info1.set_target_uri_prefix("https://second.uri/");
    (*forwarding_info1.mutable_extra_request_headers())["x-header1"] =
        "header-value1";
    (*forwarding_info1.mutable_extra_request_headers())["x-header2"] =
        "header-value2";
    auto new_request_creator = ProtocolRequestCreator::Create(
        kApiKey, forwarding_info1, /*use_compression=*/false);
    ASSERT_OK(new_request_creator);
    request_creator = std::move(*new_request_creator);
  }

  // The next series of requests should now use the ForwardingInfo (incl. use
  // the "https://second.uri/" prefix, and include the headers). Note that we
  // must use UnorderedElementsAre since the iteration order of the headers in
  // the `ForwardingInfo` is undefined.
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://second.uri/suffix2", HttpRequest::Method::kGet,
                  UnorderedElementsAre(Header{"x-goog-api-key", "API_KEY"},
                                       Header{"x-header1", "header-value1"},
                                       Header{"x-header2", "header-value2"}),
                  "")))
      .WillOnce(Return(FakeHttpResponse(200, HeaderList(), "response2")));
  http_request = request_creator->CreateProtocolRequest(
      "/suffix2", QueryParams(), HttpRequest::Method::kGet, "",
      /*is_protobuf_encoded=*/false);
  ASSERT_OK(http_request);
  result = protocol_request_helper_.PerformProtocolRequest(
      *std::move(http_request), interruptible_runner_);
  ASSERT_OK(result);
  EXPECT_EQ(result->body, "response2");

  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://second.uri/suffix3", HttpRequest::Method::kPut,
                  UnorderedElementsAre(Header{"x-goog-api-key", "API_KEY"},
                                       Header{"x-header1", "header-value1"},
                                       Header{"x-header2", "header-value2"},
                                       // This request has a response body, so
                                       // the HttpClient will add this header
                                       // automatically.
                                       Header{"Content-Length", "5"}),
                  "body3")))
      .WillOnce(Return(FakeHttpResponse(200, HeaderList(), "response3")));
  http_request = request_creator->CreateProtocolRequest(
      "/suffix3", QueryParams(), HttpRequest::Method::kPut, "body3",
      /*is_protobuf_encoded=*/false);
  ASSERT_OK(http_request);
  result = protocol_request_helper_.PerformProtocolRequest(
      *std::move(http_request), interruptible_runner_);
  ASSERT_OK(result);
  EXPECT_EQ(result->body, "response3");

  {
    // Process some more fake ForwardingInfo (without any headers this time).
    ForwardingInfo forwarding_info2;
    forwarding_info2.set_target_uri_prefix("https://third.uri");
    auto new_request_creator = ProtocolRequestCreator::Create(
        kApiKey, forwarding_info2, /*use_compression=*/false);
    ASSERT_OK(new_request_creator);
    request_creator = std::move(*new_request_creator);
  }

  // The next request should now use the latest ForwardingInfo again (i.e. use
  // the "https://third.uri/" prefix, and not specify any headers).
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://third.uri/suffix4", HttpRequest::Method::kPost,
                  // This request has a response body, so the HttpClient will
                  // add this header automatically.
                  ContainerEq(HeaderList{
                      {"x-goog-api-key", "API_KEY"},
                      {"Content-Length", "5"},
                  }),
                  "body4")))
      .WillOnce(Return(FakeHttpResponse(200, HeaderList(), "response4")));
  http_request = request_creator->CreateProtocolRequest(
      "/suffix4", QueryParams(), HttpRequest::Method::kPost, "body4",
      /*is_protobuf_encoded=*/false);
  ASSERT_OK(http_request);
  result = protocol_request_helper_.PerformProtocolRequest(
      *std::move(http_request), interruptible_runner_);
  ASSERT_OK(result);
  EXPECT_EQ(result->body, "response4");
}

TEST_F(ProtocolRequestHelperTest, TestPollOperationInvalidOperationName) {
  absl::StatusOr<Operation> result =
      protocol_request_helper_.PollOperationResponseUntilDone(
          CreatePendingOperation("invalid_operation_name"),
          initial_request_creator_, interruptible_runner_);
  EXPECT_THAT(result.status(), IsCode(INVALID_ARGUMENT));
  EXPECT_THAT(result.status().message(), HasSubstr("invalid name"));
}

TEST_F(ProtocolRequestHelperTest, TestPollOperationResponseImmediateSuccess) {
  Operation expected_response = CreateDoneOperation(GetFakeAnyProto());
  absl::StatusOr<Operation> result =
      protocol_request_helper_.PollOperationResponseUntilDone(
          expected_response, initial_request_creator_, interruptible_runner_);
  ASSERT_OK(result);
  EXPECT_THAT(*result, EqualsProto(expected_response));
}

TEST_F(ProtocolRequestHelperTest,
       TestPollOperationResponseImmediateOperationError) {
  Operation expected_response =
      CreateErrorOperation(ALREADY_EXISTS, "some error");
  absl::StatusOr<Operation> result =
      protocol_request_helper_.PollOperationResponseUntilDone(
          expected_response, initial_request_creator_, interruptible_runner_);
  ASSERT_OK(result);
  EXPECT_THAT(*result, EqualsProto(expected_response));
}

TEST_F(ProtocolRequestHelperTest,
       TestPollOperationResponseSuccessAfterPolling) {
  // Make the initial request return a pending Operation result. Note that we
  // use a '#' character in the operation name to allow us to verify that it
  // is properly URL-encoded.
  Operation pending_operation_response =
      CreatePendingOperation("operations/foo#bar");

  // Then, after letting the operation get polled twice more, eventually
  // return a fake response.
  Operation expected_response = CreateDoneOperation(GetFakeAnyProto());
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  // Note that the '#' character is encoded as "%23".
                  "https://initial.uri/v1/operations/foo%23bar?%24alt=proto",
                  HttpRequest::Method::kGet, _, IsEmpty())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), pending_operation_response.SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), pending_operation_response.SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), expected_response.SerializeAsString())));
  EXPECT_CALL(mock_clock_, Sleep(absl::Milliseconds(500))).Times(3);
  absl::StatusOr<Operation> result =
      protocol_request_helper_.PollOperationResponseUntilDone(
          pending_operation_response, initial_request_creator_,
          interruptible_runner_);
  ASSERT_OK(result);
  EXPECT_THAT(*result, EqualsProto(expected_response));
}

TEST_F(ProtocolRequestHelperTest, TestPollOperationResponseErrorAfterPolling) {
  // Make the initial request return a pending Operation result.
  Operation pending_operation_response =
      CreatePendingOperation("operations/foo#bar");

  // Then, after letting the operation get polled twice more, eventually
  // return a fake error.
  Operation expected_response =
      CreateErrorOperation(ALREADY_EXISTS, "some error");
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  // Note that the '#' character is encoded as "%23".
                  "https://initial.uri/v1/operations/foo%23bar?%24alt=proto",
                  HttpRequest::Method::kGet, _, IsEmpty())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), pending_operation_response.SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), pending_operation_response.SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), expected_response.SerializeAsString())));
  EXPECT_CALL(mock_clock_, Sleep(absl::Milliseconds(500))).Times(3);

  absl::StatusOr<Operation> result =
      protocol_request_helper_.PollOperationResponseUntilDone(
          pending_operation_response, initial_request_creator_,
          interruptible_runner_);
  ASSERT_OK(result);
  EXPECT_THAT(*result, EqualsProto(expected_response));
}

TEST_F(ProtocolRequestHelperTest,
       TestPollOperationResponseDifferentPollingIntervals) {
  StartTaskAssignmentMetadata metadata;
  *metadata.mutable_polling_interval() =
      TimeUtil::ConvertAbslToProtoDuration(absl::Milliseconds(2));
  Any packed_metadata;
  ASSERT_TRUE(packed_metadata.PackFrom(metadata));
  StartTaskAssignmentMetadata metadata_2;
  *metadata_2.mutable_polling_interval() =
      TimeUtil::ConvertAbslToProtoDuration(absl::Milliseconds(3));
  Any packed_metadata_2;
  ASSERT_TRUE(packed_metadata_2.PackFrom(metadata_2));

  // Make the initial request return a pending Operation result. Note that we
  // use a '#' character in the operation name to allow us to verify that it
  // is properly URL-encoded.
  Operation pending_operation_response =
      CreatePendingOperation("operations/foo#bar");

  // Then, after letting the operation get polled twice more, eventually
  // return a fake response.
  Operation expected_response = CreateDoneOperation(GetFakeAnyProto());

  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  // Note that the '#' character is encoded as "%23".
                  "https://initial.uri/v1/operations/foo%23bar?%24alt=proto",
                  HttpRequest::Method::kGet, _, IsEmpty())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreatePendingOperation("operations/foo#bar", packed_metadata)
              .SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreatePendingOperation("operations/foo#bar", packed_metadata_2)
              .SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), expected_response.SerializeAsString())));
  EXPECT_CALL(mock_clock_, Sleep(absl::Milliseconds(500)));
  EXPECT_CALL(mock_clock_, Sleep(absl::Milliseconds(2)));
  EXPECT_CALL(mock_clock_, Sleep(absl::Milliseconds(3)));
  absl::StatusOr<Operation> result =
      protocol_request_helper_.PollOperationResponseUntilDone(
          pending_operation_response, initial_request_creator_,
          interruptible_runner_);
  ASSERT_OK(result);
  EXPECT_THAT(*result, EqualsProto(expected_response));
}

TEST_F(ProtocolRequestHelperTest,
       TestPollOperationResponsePollingIntervalTooHigh) {
  StartTaskAssignmentMetadata metadata;
  *metadata.mutable_polling_interval() =
      TimeUtil::ConvertAbslToProtoDuration(absl::Hours(1));
  Any packed_metadata;
  ASSERT_TRUE(packed_metadata.PackFrom(metadata));

  // Make the initial request return a pending Operation result. Note that we
  // use a '#' character in the operation name to allow us to verify that it
  // is properly URL-encoded.
  Operation pending_operation_response =
      CreatePendingOperation("operations/foo#bar");

  // Then, after letting the operation get polled twice more, eventually
  // return a fake response.
  Operation expected_response = CreateDoneOperation(GetFakeAnyProto());

  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  // Note that the '#' character is encoded as "%23".
                  "https://initial.uri/v1/operations/foo%23bar?%24alt=proto",
                  HttpRequest::Method::kGet, _, IsEmpty())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreatePendingOperation("operations/foo#bar", packed_metadata)
              .SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), expected_response.SerializeAsString())));
  EXPECT_CALL(mock_clock_, Sleep(absl::Milliseconds(500)));
  EXPECT_CALL(mock_clock_, Sleep(absl::Minutes(1)));
  absl::StatusOr<Operation> result =
      protocol_request_helper_.PollOperationResponseUntilDone(
          pending_operation_response, initial_request_creator_,
          interruptible_runner_);
  ASSERT_OK(result);
  EXPECT_THAT(*result, EqualsProto(expected_response));
}

TEST_F(ProtocolRequestHelperTest,
       TestPollOperationResponseAdvertiseKeysMetadata) {
  AdvertiseKeysMetadata metadata;
  *metadata.mutable_polling_interval() =
      TimeUtil::ConvertAbslToProtoDuration(absl::Milliseconds(2));
  Any packed_metadata;
  ASSERT_TRUE(packed_metadata.PackFrom(metadata));

  // Make the initial request return a pending Operation result. Note that we
  // use a '#' character in the operation name to allow us to verify that it
  // is properly URL-encoded.
  Operation pending_operation_response =
      CreatePendingOperation("operations/foo#bar");

  // Then, after letting the operation get polled twice more, eventually
  // return a fake response.
  Operation expected_response = CreateDoneOperation(GetFakeAnyProto());

  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  // Note that the '#' character is encoded as "%23".
                  "https://initial.uri/v1/operations/foo%23bar?%24alt=proto",
                  HttpRequest::Method::kGet, _, IsEmpty())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreatePendingOperation("operations/foo#bar", packed_metadata)
              .SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), expected_response.SerializeAsString())));
  EXPECT_CALL(mock_clock_, Sleep(absl::Milliseconds(500)));
  EXPECT_CALL(mock_clock_, Sleep(absl::Milliseconds(2)));
  absl::StatusOr<Operation> result =
      protocol_request_helper_.PollOperationResponseUntilDone(
          pending_operation_response, initial_request_creator_,
          interruptible_runner_);
  ASSERT_OK(result);
  EXPECT_THAT(*result, EqualsProto(expected_response));
}

TEST_F(ProtocolRequestHelperTest, TestPollOperationResponseShareKeysMetadata) {
  ShareKeysMetadata metadata;
  *metadata.mutable_polling_interval() =
      TimeUtil::ConvertAbslToProtoDuration(absl::Milliseconds(2));
  Any packed_metadata;
  ASSERT_TRUE(packed_metadata.PackFrom(metadata));

  // Make the initial request return a pending Operation result. Note that we
  // use a '#' character in the operation name to allow us to verify that it
  // is properly URL-encoded.
  Operation pending_operation_response =
      CreatePendingOperation("operations/foo#bar");

  // Then, after letting the operation get polled twice more, eventually
  // return a fake response.
  Operation expected_response = CreateDoneOperation(GetFakeAnyProto());

  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  // Note that the '#' character is encoded as "%23".
                  "https://initial.uri/v1/operations/foo%23bar?%24alt=proto",
                  HttpRequest::Method::kGet, _, IsEmpty())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreatePendingOperation("operations/foo#bar", packed_metadata)
              .SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), expected_response.SerializeAsString())));
  EXPECT_CALL(mock_clock_, Sleep(absl::Milliseconds(500)));
  EXPECT_CALL(mock_clock_, Sleep(absl::Milliseconds(2)));
  absl::StatusOr<Operation> result =
      protocol_request_helper_.PollOperationResponseUntilDone(
          pending_operation_response, initial_request_creator_,
          interruptible_runner_);
  ASSERT_OK(result);
  EXPECT_THAT(*result, EqualsProto(expected_response));
}

TEST_F(ProtocolRequestHelperTest,
       TestPollOperationResponseSubmitSecureAggregationResultMetadata) {
  SubmitSecureAggregationResultMetadata metadata;
  *metadata.mutable_polling_interval() =
      TimeUtil::ConvertAbslToProtoDuration(absl::Milliseconds(2));
  Any packed_metadata;
  ASSERT_TRUE(packed_metadata.PackFrom(metadata));

  // Make the initial request return a pending Operation result. Note that we
  // use a '#' character in the operation name to allow us to verify that it
  // is properly URL-encoded.
  Operation pending_operation_response =
      CreatePendingOperation("operations/foo#bar");

  // Then, after letting the operation get polled twice more, eventually
  // return a fake response.
  Operation expected_response = CreateDoneOperation(GetFakeAnyProto());

  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  // Note that the '#' character is encoded as "%23".
                  "https://initial.uri/v1/operations/foo%23bar?%24alt=proto",
                  HttpRequest::Method::kGet, _, IsEmpty())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreatePendingOperation("operations/foo#bar", packed_metadata)
              .SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), expected_response.SerializeAsString())));
  EXPECT_CALL(mock_clock_, Sleep(absl::Milliseconds(500)));
  EXPECT_CALL(mock_clock_, Sleep(absl::Milliseconds(2)));
  absl::StatusOr<Operation> result =
      protocol_request_helper_.PollOperationResponseUntilDone(
          pending_operation_response, initial_request_creator_,
          interruptible_runner_);
  ASSERT_OK(result);
  EXPECT_THAT(*result, EqualsProto(expected_response));
}

TEST_F(ProtocolRequestHelperTest, PerformMultipleRequestsSuccess) {
  auto request_a = initial_request_creator_.CreateProtocolRequest(
      "/v1/request_a", QueryParams(), HttpRequest::Method::kPost, "body1",
      /*is_protobuf_encoded=*/false);
  ASSERT_OK(request_a);
  auto request_b = initial_request_creator_.CreateProtocolRequest(
      "/v1/request_b", QueryParams(), HttpRequest::Method::kPost, "body2",
      /*is_protobuf_encoded=*/false);
  ASSERT_OK(request_b);
  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://initial.uri/v1/request_a", HttpRequest::Method::kPost,
          // This request has a response body, so the HttpClient will
          // add this header automatically.
          ContainerEq(HeaderList{{"x-goog-api-key", "API_KEY"},
                                 {"Content-Length", "5"}}),
          "body1")))
      .WillOnce(Return(FakeHttpResponse(200, HeaderList(), "response1")));
  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://initial.uri/v1/request_b", HttpRequest::Method::kPost,
          // This request has a response body, so the HttpClient will
          // add this header automatically.
          ContainerEq(HeaderList{{"x-goog-api-key", "API_KEY"},
                                 {"Content-Length", "5"}}),
          "body2")))
      .WillOnce(Return(FakeHttpResponse(200, HeaderList(), "response2")));

  std::vector<std::unique_ptr<HttpRequest>> requests;
  requests.push_back(std::move(*request_a));
  requests.push_back(std::move(*request_b));

  auto result = protocol_request_helper_.PerformMultipleProtocolRequests(
      std::move(requests), interruptible_runner_);
  ASSERT_OK(result);
  auto response_1 = (*result)[0];
  ASSERT_OK(response_1);
  VerifyInMemoryHttpResponse(*response_1, 200, "", "response1");
  auto response_2 = (*result)[1];
  ASSERT_OK(response_2);
  VerifyInMemoryHttpResponse(*response_2, 200, "", "response2");
}

TEST_F(ProtocolRequestHelperTest, PerformMultipleRequestsPartialFail) {
  std::string uri_suffix_a = "/v1/request_a";
  auto request_a = initial_request_creator_.CreateProtocolRequest(
      uri_suffix_a, QueryParams(), HttpRequest::Method::kPost, "body1",
      /*is_protobuf_encoded=*/false);
  ASSERT_OK(request_a);
  std::string uri_suffix_b = "/v1/request_b";
  auto request_b = initial_request_creator_.CreateProtocolRequest(
      uri_suffix_b, QueryParams(), HttpRequest::Method::kPost, "body2",
      /*is_protobuf_encoded=*/false);
  ASSERT_OK(request_b);
  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://initial.uri/v1/request_a", HttpRequest::Method::kPost,
          // This request has a response body, so the HttpClient will
          // add this header automatically.
          ContainerEq(HeaderList{{"x-goog-api-key", "API_KEY"},
                                 {"Content-Length", "5"}}),
          "body1")))
      .WillOnce(Return(FakeHttpResponse(200, HeaderList(), "response1")));
  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://initial.uri/v1/request_b", HttpRequest::Method::kPost,
          // This request has a response body, so the HttpClient will
          // add this header automatically.
          ContainerEq(HeaderList{{"x-goog-api-key", "API_KEY"},
                                 {"Content-Length", "5"}}),
          "body2")))
      .WillOnce(
          Return(FakeHttpResponse(404, HeaderList(), "failure_response")));

  std::vector<std::unique_ptr<HttpRequest>> requests;
  requests.push_back(std::move(*request_a));
  requests.push_back(std::move(*request_b));

  auto result = protocol_request_helper_.PerformMultipleProtocolRequests(
      std::move(requests), interruptible_runner_);

  ASSERT_OK(result);
  auto response_1 = (*result)[0];
  ASSERT_OK(response_1);
  VerifyInMemoryHttpResponse(*response_1, 200, "", "response1");
  auto response_2 = (*result)[1];
  ASSERT_THAT(response_2, IsCode(absl::StatusCode::kNotFound));
}
}  // anonymous namespace
}  // namespace http
}  // namespace client
}  // namespace fcp
