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
#include "fcp/client/http/http_federated_protocol.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "google/longrunning/operations.pb.h"
#include "google/protobuf/any.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/rpc/code.pb.h"
#include "google/type/datetime.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/random/random.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "fcp/base/clock.h"
#include "fcp/base/compression.h"
#include "fcp/base/digest.h"
#include "fcp/base/monitoring.h"
#include "fcp/base/time_util.h"
#include "fcp/client/attestation/attestation_verifier.h"
#include "fcp/client/cache/test_helpers.h"
#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/engine/engine.pb.h"
#include "fcp/client/event_time_range.pb.h"
#include "fcp/client/federated_protocol.h"
#include "fcp/client/http/http_client.h"
#include "fcp/client/http/testing/test_helpers.h"
#include "fcp/client/interruptible_runner.h"
#include "fcp/client/stats.h"
#include "fcp/client/test_helpers.h"
#include "fcp/confidentialcompute/client_payload.h"
#include "fcp/confidentialcompute/cose.h"
#include "fcp/confidentialcompute/crypto.h"
#include "fcp/protos/confidentialcompute/blob_header.pb.h"
#include "fcp/protos/confidentialcompute/payload_metadata.pb.h"
#include "fcp/protos/federated_api.pb.h"
#include "fcp/protos/federatedcompute/aggregations.pb.h"
#include "fcp/protos/federatedcompute/common.pb.h"
#include "fcp/protos/federatedcompute/confidential_aggregations.pb.h"
#include "fcp/protos/federatedcompute/eligibility_eval_tasks.pb.h"
#include "fcp/protos/federatedcompute/secure_aggregations.pb.h"
#include "fcp/protos/federatedcompute/task_assignments.pb.h"
#include "fcp/protos/plan.pb.h"
#include "fcp/protos/population_eligibility_spec.pb.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/testing/testing.h"
#include "proto/attestation/endorsement.pb.h"

namespace fcp::client::http {
namespace {

using ::fcp::EqualsProto;
using ::fcp::IsCode;
using ::fcp::client::http::FakeHttpResponse;
using ::fcp::client::http::MockableHttpClient;
using ::fcp::client::http::MockHttpClient;
using ::fcp::client::http::SimpleHttpRequestMatcher;
using ::fcp::confidential_compute::OkpCwt;
using ::google::internal::federated::plan::PopulationEligibilitySpec;
using ::google::internal::federatedcompute::v1::ByteStreamResource;
using ::google::internal::federatedcompute::v1::ClientStats;
using ::google::internal::federatedcompute::v1::ConfidentialEncryptionConfig;
using ::google::internal::federatedcompute::v1::EligibilityEvalTask;
using ::google::internal::federatedcompute::v1::EligibilityEvalTaskRequest;
using ::google::internal::federatedcompute::v1::EligibilityEvalTaskResponse;
using ::google::internal::federatedcompute::v1::ForwardingInfo;
using ::google::internal::federatedcompute::v1::
    PerformMultipleTaskAssignmentsRequest;
using ::google::internal::federatedcompute::v1::
    PerformMultipleTaskAssignmentsResponse;
using ::google::internal::federatedcompute::v1::
    ReportEligibilityEvalTaskResultRequest;
using ::google::internal::federatedcompute::v1::ReportTaskResultRequest;
using ::google::internal::federatedcompute::v1::ReportTaskResultResponse;
using ::google::internal::federatedcompute::v1::Resource;
using ::google::internal::federatedcompute::v1::ResourceCompressionFormat;
using ::google::internal::federatedcompute::v1::RetryWindow;
using ::google::internal::federatedcompute::v1::SecureAggregandExecutionInfo;
using ::google::internal::federatedcompute::v1::
    StartAggregationDataUploadRequest;
using ::google::internal::federatedcompute::v1::
    StartAggregationDataUploadResponse;
using ::google::internal::federatedcompute::v1::
    StartConfidentialAggregationDataUploadRequest;
using ::google::internal::federatedcompute::v1::
    StartConfidentialAggregationDataUploadResponse;
using ::google::internal::federatedcompute::v1::StartSecureAggregationRequest;
using ::google::internal::federatedcompute::v1::StartSecureAggregationResponse;
using ::google::internal::federatedcompute::v1::StartTaskAssignmentRequest;
using ::google::internal::federatedcompute::v1::StartTaskAssignmentResponse;
using ::google::internal::federatedcompute::v1::SubmitAggregationResultRequest;
using ::google::internal::federatedcompute::v1::
    SubmitConfidentialAggregationResultRequest;
using ::google::internal::federatedcompute::v1::TaskAssignment;
using ::google::internal::federatedml::v2::TaskEligibilityInfo;
using ::google::internal::federatedml::v2::TaskWeight;
using ::google::longrunning::GetOperationRequest;
using ::google::longrunning::Operation;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::DescribeMatcher;
using ::testing::DoubleEq;
using ::testing::DoubleNear;
using ::testing::Eq;
using ::testing::ExplainMatchResult;
using ::testing::Field;
using ::testing::FieldsAre;
using ::testing::Ge;
using ::testing::Gt;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::Lt;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;
using ::testing::WithArg;

constexpr char kEntryPointUri[] = "https://initial.uri/";
constexpr char kTaskAssignmentTargetUri[] = "https://taskassignment.uri/";
constexpr char kAggregationTargetUri[] = "https://aggregation.uri/";
constexpr char kSecondStageAggregationTargetUri[] =
    "https://aggregation.second.uri/";
constexpr char kByteStreamTargetUri[] = "https://bytestream.uri/";
constexpr char kApiKey[] = "TEST_APIKEY";
// Note that we include a '/' character in the population name, which allows us
// to verify that it is correctly URL-encoded into "%2F".
constexpr char kPopulationName[] = "TEST/POPULATION";
constexpr char kEligibilityEvalExecutionId[] = "ELIGIBILITY_EXECUTION_ID";
// Note that we include a '/' and '#' characters in the population name, which
// allows us to verify that it is correctly URL-encoded into "%2F" and "%23".
constexpr char kEligibilityEvalSessionId[] = "ELIGIBILITY/SESSION#ID";
constexpr char kPlan[] = "CLIENT_ONLY_PLAN";
constexpr char kInitCheckpoint[] = "INIT_CHECKPOINT";
constexpr char kRetryToken[] = "OLD_RETRY_TOKEN";
constexpr char kClientVersion[] = "CLIENT_VERSION";
constexpr char kAttestationMeasurement[] = "ATTESTATION_MEASUREMENT";
constexpr char kClientSessionId[] = "CLIENT_SESSION_ID";
constexpr char kAggregationSessionId[] = "AGGREGATION_SESSION_ID";
constexpr char kAuthorizationToken[] = "AUTHORIZATION_TOKEN";
constexpr char kTaskName[] = "TASK_NAME";
constexpr char kClientToken[] = "CLIENT_TOKEN";
constexpr char kResourceName[] = "CHECKPOINT_RESOURCE";
constexpr char kFederatedSelectUriTemplate[] = "https://federated.select";
constexpr char kOperationName[] = "my_operation";
constexpr char kMultiTaskId_1[] = "TASK_1";
constexpr char kMultiTaskClientSessionId_1[] = "CLIENT_SESSION_ID_1";
constexpr char kMultiTaskAggregationSessionId_1[] = "AGGREGATION_SESSION_ID_1";
constexpr char kMultiTaskId_2[] = "TASK_2";
constexpr char kMultiTaskClientSessionId_2[] = "CLIENT_SESSION_ID_2";
constexpr char kMultiTaskAggregationSessionId_2[] = "AGGREGATION_SESSION_ID_2";

const int32_t kCancellationWaitingPeriodSec = 1;
const int32_t kMinimumClientsInServerVisibleAggregate = 2;

MATCHER_P(EligibilityEvalTaskRequestMatcher, matcher,
          absl::StrCat(negation ? "doesn't parse" : "parses",
                       " as an EligibilityEvalTaskRequest, and that ",
                       DescribeMatcher<EligibilityEvalTaskRequest>(matcher,
                                                                   negation))) {
  EligibilityEvalTaskRequest request;
  if (!request.ParseFromString(arg)) {
    return false;
  }
  return ExplainMatchResult(matcher, request, result_listener);
}

MATCHER_P(
    ReportEligibilityEvalTaskResultRequestMatcher, matcher,
    absl::StrCat(negation ? "doesn't parse" : "parses",
                 " as a ReportEligibilityEvalTaskResultRequest, and that ",
                 DescribeMatcher<ReportEligibilityEvalTaskResultRequest>(
                     matcher, negation))) {
  ReportEligibilityEvalTaskResultRequest request;
  if (!request.ParseFromString(arg)) {
    return false;
  }
  return ExplainMatchResult(matcher, request, result_listener);
}

MATCHER_P(StartTaskAssignmentRequestMatcher, matcher,
          absl::StrCat(negation ? "doesn't parse" : "parses",
                       " as a StartTaskAssignmentRequest, and that ",
                       DescribeMatcher<StartTaskAssignmentRequest>(matcher,
                                                                   negation))) {
  StartTaskAssignmentRequest request;
  if (!request.ParseFromString(arg)) {
    return false;
  }
  return ExplainMatchResult(matcher, request, result_listener);
}

MATCHER_P(GetOperationRequestMatcher, matcher,
          absl::StrCat(negation ? "doesn't parse" : "parses",
                       " as a GetOperationRequest, and that ",
                       DescribeMatcher<GetOperationRequest>(matcher,
                                                            negation))) {
  GetOperationRequest request;
  if (!request.ParseFromString(arg)) {
    return false;
  }
  return ExplainMatchResult(matcher, request, result_listener);
}

MATCHER_P(ReportTaskResultRequestMatcher, matcher,
          absl::StrCat(negation ? "doesn't parse" : "parses",
                       " as a ReportTaskResultRequest, and that ",
                       DescribeMatcher<ReportTaskResultRequest>(matcher,
                                                                negation))) {
  ReportTaskResultRequest request;
  if (!request.ParseFromString(arg)) {
    return false;
  }
  return ExplainMatchResult(matcher, request, result_listener);
}

MATCHER(IsOk, "") { return arg.ok(); }

MATCHER_P(IsOkAndHolds, m, "") {
  return testing::ExplainMatchResult(IsOk(), arg, result_listener) &&
         testing::ExplainMatchResult(m, arg.value(), result_listener);
}
MATCHER_P(EqTaskIdentifier, task_identifier, "") {
  return testing::ExplainMatchResult(task_identifier, arg.task_identifier,
                                     result_listener);
}

constexpr int kTransientErrorsRetryPeriodSecs = 10;
constexpr double kTransientErrorsRetryDelayJitterPercent = 0.1;
constexpr double kExpectedTransientErrorsRetryPeriodSecsMin = 9.0;
constexpr double kExpectedTransientErrorsRetryPeriodSecsMax = 11.0;
constexpr int kPermanentErrorsRetryPeriodSecs = 100;
constexpr double kPermanentErrorsRetryDelayJitterPercent = 0.2;
constexpr double kExpectedPermanentErrorsRetryPeriodSecsMin = 80.0;
constexpr double kExpectedPermanentErrorsRetryPeriodSecsMax = 120.0;

void ExpectTransientErrorRetryWindow(
    const ::google::internal::federatedml::v2::RetryWindow& retry_window) {
  // The calculated retry delay must lie within the expected transient errors
  // retry delay range.
  EXPECT_THAT(retry_window.delay_min().seconds() +
                  retry_window.delay_min().nanos() / 1000000000,
              AllOf(Ge(kExpectedTransientErrorsRetryPeriodSecsMin),
                    Lt(kExpectedTransientErrorsRetryPeriodSecsMax)));
  EXPECT_THAT(retry_window.delay_max(), EqualsProto(retry_window.delay_min()));
}

void ExpectPermanentErrorRetryWindow(
    const ::google::internal::federatedml::v2::RetryWindow& retry_window) {
  // The calculated retry delay must lie within the expected permanent errors
  // retry delay range.
  EXPECT_THAT(retry_window.delay_min().seconds() +
                  retry_window.delay_min().nanos() / 1000000000,
              AllOf(Ge(kExpectedPermanentErrorsRetryPeriodSecsMin),
                    Lt(kExpectedPermanentErrorsRetryPeriodSecsMax)));
  EXPECT_THAT(retry_window.delay_max(), EqualsProto(retry_window.delay_min()));
}

RetryWindow GetAcceptedRetryWindow() {
  // Must not overlap with kTransientErrorsRetryPeriodSecs or
  // kPermanentErrorsRetryPeriodSecs.
  RetryWindow retry_window;
  retry_window.mutable_delay_min()->set_seconds(200L);
  retry_window.mutable_delay_max()->set_seconds(299L);
  return retry_window;
}

void ExpectAcceptedRetryWindow(
    const ::google::internal::federatedml::v2::RetryWindow& retry_window) {
  // The calculated retry delay must lie within the expected 'rejected' retry
  // delay range.
  EXPECT_THAT(retry_window.delay_min().seconds() +
                  retry_window.delay_min().nanos() / 1000000000,
              AllOf(Ge(200L), Lt(299L)));
  EXPECT_THAT(retry_window.delay_max(), EqualsProto(retry_window.delay_min()));
}

RetryWindow GetRejectedRetryWindow() {
  // Must not overlap with kTransientErrorsRetryPeriodSecs or
  // kPermanentErrorsRetryPeriodSecs.
  RetryWindow retry_window;
  retry_window.mutable_delay_min()->set_seconds(300L);
  retry_window.mutable_delay_max()->set_seconds(399L);
  return retry_window;
}

void ExpectRejectedRetryWindow(
    const ::google::internal::federatedml::v2::RetryWindow& retry_window) {
  // The calculated retry delay must lie within the expected 'rejected' retry
  // delay range.
  EXPECT_THAT(retry_window.delay_min().seconds() +
                  retry_window.delay_min().nanos() / 1000000000,
              AllOf(Ge(300L), Lt(399L)));
  EXPECT_THAT(retry_window.delay_max(), EqualsProto(retry_window.delay_min()));
}

EligibilityEvalTaskRequest GetExpectedEligibilityEvalTaskRequest(
    bool enable_confidential_aggregation = false) {
  EligibilityEvalTaskRequest request;
  // Note: we don't expect population_name to be set, since it should be set in
  // the URI instead.
  request.mutable_client_version()->set_version_code(kClientVersion);
  request.mutable_attestation_measurement()->set_value(kAttestationMeasurement);
  request.mutable_resource_capabilities()
      ->mutable_supported_compression_formats()
      ->Add(ResourceCompressionFormat::RESOURCE_COMPRESSION_FORMAT_GZIP);
  request.mutable_resource_capabilities()
      ->set_supports_confidential_aggregation(enable_confidential_aggregation);
  request.mutable_eligibility_eval_task_capabilities()
      ->set_supports_multiple_task_assignment(true);
  request.mutable_eligibility_eval_task_capabilities()
      ->set_supports_native_eets(true);

  return request;
}

EligibilityEvalTaskResponse GetFakeEnabledEligibilityEvalTaskResponse(
    const Resource& plan, const Resource& checkpoint,
    const std::string& execution_id,
    const std::string& target_uri_prefix = kTaskAssignmentTargetUri,
    std::optional<Resource> population_eligibility_spec = std::nullopt,
    const RetryWindow& accepted_retry_window = GetAcceptedRetryWindow(),
    const RetryWindow& rejected_retry_window = GetRejectedRetryWindow()) {
  EligibilityEvalTaskResponse response;
  response.set_session_id(kEligibilityEvalSessionId);
  EligibilityEvalTask* eval_task = response.mutable_eligibility_eval_task();
  *eval_task->mutable_plan() = plan;
  *eval_task->mutable_init_checkpoint() = checkpoint;
  if (population_eligibility_spec.has_value()) {
    *eval_task->mutable_population_eligibility_spec() =
        population_eligibility_spec.value();
  }
  eval_task->set_execution_id(execution_id);
  ForwardingInfo* forwarding_info =
      response.mutable_task_assignment_forwarding_info();
  forwarding_info->set_target_uri_prefix(target_uri_prefix);
  *response.mutable_retry_window_if_accepted() = accepted_retry_window;
  *response.mutable_retry_window_if_rejected() = rejected_retry_window;
  return response;
}

EligibilityEvalTaskResponse GetFakeDisabledEligibilityEvalTaskResponse() {
  EligibilityEvalTaskResponse response;
  response.set_session_id(kEligibilityEvalSessionId);
  response.mutable_no_eligibility_eval_configured();
  ForwardingInfo* forwarding_info =
      response.mutable_task_assignment_forwarding_info();
  forwarding_info->set_target_uri_prefix(kTaskAssignmentTargetUri);
  *response.mutable_retry_window_if_accepted() = GetAcceptedRetryWindow();
  *response.mutable_retry_window_if_rejected() = GetRejectedRetryWindow();
  return response;
}

EligibilityEvalTaskResponse GetFakeRejectedEligibilityEvalTaskResponse() {
  EligibilityEvalTaskResponse response;
  response.mutable_rejection_info();
  *response.mutable_retry_window_if_accepted() = GetAcceptedRetryWindow();
  *response.mutable_retry_window_if_rejected() = GetRejectedRetryWindow();
  return response;
}

TaskEligibilityInfo GetFakeTaskEligibilityInfo() {
  TaskEligibilityInfo eligibility_info;
  TaskWeight* task_weight = eligibility_info.mutable_task_weights()->Add();
  task_weight->set_task_name("foo");
  task_weight->set_weight(567.8);
  return eligibility_info;
}

StartTaskAssignmentRequest GetExpectedStartTaskAssignmentRequest(
    const std::optional<TaskEligibilityInfo>& task_eligibility_info,
    bool enable_confidential_aggregation = false) {
  // Note: we don't expect population_name or session_id to be set, since they
  // should be set in the URI instead.
  StartTaskAssignmentRequest request;
  request.mutable_client_version()->set_version_code(kClientVersion);
  if (task_eligibility_info.has_value()) {
    *request.mutable_task_eligibility_info() = *task_eligibility_info;
  }
  request.mutable_resource_capabilities()
      ->mutable_supported_compression_formats()
      ->Add(ResourceCompressionFormat::RESOURCE_COMPRESSION_FORMAT_GZIP);
  request.mutable_resource_capabilities()
      ->set_supports_confidential_aggregation(enable_confidential_aggregation);
  return request;
}

StartTaskAssignmentResponse GetFakeRejectedTaskAssignmentResponse() {
  StartTaskAssignmentResponse response;
  response.mutable_rejection_info();
  return response;
}

TaskAssignment CreateTaskAssignment(
    const Resource& plan, const Resource& checkpoint,
    const std::string& federated_select_uri_template,
    const std::string& client_session_id,
    const std::string& aggregation_session_id, const std::string& task_name,
    const std::string& target_uri_prefix,
    int32_t minimum_clients_in_server_visible_aggregate,
    std::optional<Resource> confidential_data_access_policy = std::nullopt,
    std::optional<Resource> signed_endorsements = std::nullopt) {
  TaskAssignment task_assignment;
  ForwardingInfo* forwarding_info =
      task_assignment.mutable_aggregation_data_forwarding_info();
  forwarding_info->set_target_uri_prefix(target_uri_prefix);
  task_assignment.set_session_id(client_session_id);
  task_assignment.set_aggregation_id(aggregation_session_id);
  task_assignment.set_authorization_token(kAuthorizationToken);
  task_assignment.set_task_name(task_name);
  *task_assignment.mutable_plan() = plan;
  *task_assignment.mutable_init_checkpoint() = checkpoint;
  task_assignment.mutable_federated_select_uri_info()->set_uri_template(
      federated_select_uri_template);
  if (minimum_clients_in_server_visible_aggregate > 0) {
    task_assignment.mutable_secure_aggregation_info()
        ->set_minimum_clients_in_server_visible_aggregate(
            minimum_clients_in_server_visible_aggregate);
  } else if (confidential_data_access_policy.has_value()) {
    *task_assignment.mutable_confidential_aggregation_info()
         ->mutable_data_access_policy() = *confidential_data_access_policy;
    if (signed_endorsements.has_value()) {
      *task_assignment.mutable_confidential_aggregation_info()
           ->mutable_signed_endorsements() = *signed_endorsements;
    }
  } else {
    task_assignment.mutable_aggregation_info();
  }
  return task_assignment;
}

StartTaskAssignmentResponse GetFakeTaskAssignmentResponse(
    const Resource& plan, const Resource& checkpoint,
    const std::string& federated_select_uri_template,
    const std::string& aggregation_session_id,
    int32_t minimum_clients_in_server_visible_aggregate,
    const std::string& target_uri_prefix = kAggregationTargetUri,
    std::optional<Resource> confidential_data_access_policy = std::nullopt,
    std::optional<Resource> signed_endorsements = std::nullopt) {
  StartTaskAssignmentResponse response;
  *response.mutable_task_assignment() = CreateTaskAssignment(
      plan, checkpoint, federated_select_uri_template, kClientSessionId,
      aggregation_session_id, kTaskName, target_uri_prefix,
      minimum_clients_in_server_visible_aggregate,
      confidential_data_access_policy, signed_endorsements);
  return response;
}

ReportTaskResultRequest GetExpectedReportTaskResultRequest(
    absl::string_view aggregation_id, absl::string_view task_name,
    ::google::rpc::Code code, absl::Duration train_duration) {
  ReportTaskResultRequest request;
  request.set_aggregation_id(std::string(aggregation_id));
  request.set_task_name(std::string(task_name));
  request.set_computation_status_code(code);
  ClientStats client_stats;
  *client_stats.mutable_computation_execution_duration() =
      TimeUtil::ConvertAbslToProtoDuration(train_duration);
  *request.mutable_client_stats() = client_stats;
  return request;
}

StartAggregationDataUploadResponse GetFakeStartAggregationDataUploadResponse(
    absl::string_view aggregation_resource_name,
    absl::string_view byte_stream_uri_prefix,
    absl::string_view second_stage_aggregation_uri_prefix) {
  StartAggregationDataUploadResponse response;
  ByteStreamResource* resource = response.mutable_resource();
  *resource->mutable_resource_name() = aggregation_resource_name;
  ForwardingInfo* data_upload_forwarding_info =
      resource->mutable_data_upload_forwarding_info();
  *data_upload_forwarding_info->mutable_target_uri_prefix() =
      byte_stream_uri_prefix;
  ForwardingInfo* aggregation_protocol_forwarding_info =
      response.mutable_aggregation_protocol_forwarding_info();
  *aggregation_protocol_forwarding_info->mutable_target_uri_prefix() =
      second_stage_aggregation_uri_prefix;
  response.set_client_token(kClientToken);
  return response;
}

StartConfidentialAggregationDataUploadResponse
GetFakeStartConfidentialAggregationDataUploadResponse(
    absl::string_view aggregation_resource_name,
    absl::string_view byte_stream_uri_prefix,
    absl::string_view second_stage_aggregation_uri_prefix,
    const ConfidentialEncryptionConfig& confidential_encryption_config) {
  StartConfidentialAggregationDataUploadResponse response;
  ByteStreamResource* resource = response.mutable_resource();
  *resource->mutable_resource_name() = aggregation_resource_name;
  ForwardingInfo* data_upload_forwarding_info =
      resource->mutable_data_upload_forwarding_info();
  *data_upload_forwarding_info->mutable_target_uri_prefix() =
      byte_stream_uri_prefix;
  ForwardingInfo* aggregation_protocol_forwarding_info =
      response.mutable_aggregation_protocol_forwarding_info();
  *aggregation_protocol_forwarding_info->mutable_target_uri_prefix() =
      second_stage_aggregation_uri_prefix;
  response.set_client_token(kClientToken);
  response.mutable_encryption_config()->mutable_inline_resource()->set_data(
      confidential_encryption_config.SerializeAsString());
  return response;
}

FakeHttpResponse CreateEmptySuccessHttpResponse() {
  return FakeHttpResponse(200, HeaderList(), "");
}

confidentialcompute::SignedEndorsements GetFakeSignedEndorsements() {
  confidentialcompute::SignedEndorsements signed_endorsements;
  auto signed_endorsement = signed_endorsements.add_signed_endorsement();
  signed_endorsement->mutable_endorsement()->set_serialized(
      "{\"payload\":{\"subject\":{\"name\":\"stefans signed endorsement\"}}}");
  signed_endorsement->mutable_signature()->set_key_id(1);
  signed_endorsement->mutable_signature()->set_raw("stefans public key");
  return signed_endorsements;
}

class HttpFederatedProtocolTest : public ::testing::Test {
 protected:
  void SetUp() override {
    EXPECT_CALL(mock_flags_,
                federated_training_transient_errors_retry_delay_secs)
        .WillRepeatedly(Return(kTransientErrorsRetryPeriodSecs));
    EXPECT_CALL(mock_flags_,
                federated_training_transient_errors_retry_delay_jitter_percent)
        .WillRepeatedly(Return(kTransientErrorsRetryDelayJitterPercent));
    EXPECT_CALL(mock_flags_,
                federated_training_permanent_errors_retry_delay_secs)
        .WillRepeatedly(Return(kPermanentErrorsRetryPeriodSecs));
    EXPECT_CALL(mock_flags_,
                federated_training_permanent_errors_retry_delay_jitter_percent)
        .WillRepeatedly(Return(kPermanentErrorsRetryDelayJitterPercent));
    EXPECT_CALL(mock_flags_, federated_training_permanent_error_codes)
        .WillRepeatedly(Return(std::vector<int32_t>{
            static_cast<int32_t>(absl::StatusCode::kNotFound),
            static_cast<int32_t>(absl::StatusCode::kInvalidArgument),
            static_cast<int32_t>(absl::StatusCode::kUnimplemented)}));
    // Note that we disable compression in test to make it easier to verify the
    // request body. The compression logic is tested in
    // in_memory_request_response_test.cc.
    EXPECT_CALL(mock_flags_, disable_http_request_body_compression)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(mock_flags_, waiting_period_sec_for_cancellation)
        .WillRepeatedly(Return(kCancellationWaitingPeriodSec));
    EXPECT_CALL(mock_flags_, enable_confidential_aggregation)
        .WillRepeatedly(Return(false));
    EXPECT_CALL(mock_flags_, enable_relative_uri_prefix)
        .WillRepeatedly(Return(true));
    // Disable http retries to simplify transient error tests that return
    // retriable http errors.
    EXPECT_CALL(mock_flags_, http_retry_max_attempts).WillRepeatedly(Return(0));

    // We only initialize federated_protocol_ in this SetUp method, rather than
    // in the test's constructor, to ensure that we can set mock flag values
    // before the HttpFederatedProtocol constructor is called. Using
    // std::unique_ptr conveniently allows us to assign the field a new value
    // after construction (which we could not do if the field's type was
    // HttpFederatedProtocol, since it doesn't have copy or move constructors).
    federated_protocol_ = std::make_unique<HttpFederatedProtocol>(
        clock_, &mock_log_manager_, &mock_flags_, &mock_http_client_,
        absl::WrapUnique(mock_secagg_runner_factory_),
        &mock_secagg_event_publisher_, &mock_resource_cache_,
        absl::WrapUnique(mock_attestation_verifier_), kEntryPointUri, kApiKey,
        kPopulationName, kRetryToken, kClientVersion, kAttestationMeasurement,
        mock_should_abort_.AsStdFunction(), absl::BitGen(),
        InterruptibleRunner::TimingConfig{
            .polling_period = absl::ZeroDuration(),
            .graceful_shutdown_period = absl::InfiniteDuration(),
            .extended_shutdown_period = absl::InfiniteDuration()});
  }

  void TearDown() override {
    // Regardless of the outcome of the test (or the protocol interaction being
    // tested), network usage must always be reflected in the network stats
    // methods.
    HttpRequestHandle::SentReceivedBytes sent_received_bytes =
        mock_http_client_.TotalSentReceivedBytes();

    NetworkStats network_stats = federated_protocol_->GetNetworkStats();
    EXPECT_EQ(network_stats.bytes_downloaded,
              sent_received_bytes.received_bytes);
    EXPECT_EQ(network_stats.bytes_uploaded, sent_received_bytes.sent_bytes);
    // If any network traffic occurred, we expect to see some time reflected in
    // the duration.
    if (network_stats.bytes_uploaded > 0) {
      EXPECT_THAT(network_stats.network_duration, Gt(absl::ZeroDuration()));
    }
  }

  // This function runs a successful EligibilityEvalCheckin() that results in an
  // eligibility eval payload being returned by the server (if
  // `eligibility_eval_enabled` is true), or results in a 'no eligibility eval
  // configured' response (if `eligibility_eval_enabled` is false). This is a
  // utility function used by Checkin*() tests that depend on a prior,
  // successful execution of EligibilityEvalCheckin(). It returns a
  // absl::Status, which the caller should verify is OK using ASSERT_OK.
  absl::Status RunSuccessfulEligibilityEvalCheckin(
      bool eligibility_eval_enabled = true,
      bool enable_confidential_aggregation = false,
      bool set_relative_uri = false) {
    EligibilityEvalTaskResponse eval_task_response;
    if (eligibility_eval_enabled) {
      // We return a fake response which returns the plan/initial checkpoint
      // data inline, to keep things simple.
      std::string expected_plan = kPlan;
      Resource plan_resource;
      plan_resource.mutable_inline_resource()->set_data(kPlan);
      std::string expected_checkpoint = kInitCheckpoint;
      Resource checkpoint_resource;
      checkpoint_resource.mutable_inline_resource()->set_data(
          expected_checkpoint);
      if (set_relative_uri) {
        eval_task_response = GetFakeEnabledEligibilityEvalTaskResponse(
            plan_resource, checkpoint_resource, kEligibilityEvalExecutionId,
            "/");
      } else {
        eval_task_response = GetFakeEnabledEligibilityEvalTaskResponse(
            plan_resource, checkpoint_resource, kEligibilityEvalExecutionId);
      }
    } else {
      eval_task_response = GetFakeDisabledEligibilityEvalTaskResponse();
    }
    std::string request_uri =
        "https://initial.uri/v1/eligibilityevaltasks/"
        "TEST%2FPOPULATION:request?%24alt=proto";
    EXPECT_CALL(mock_http_client_,
                PerformSingleRequest(SimpleHttpRequestMatcher(
                    request_uri, HttpRequest::Method::kPost, _,
                    EligibilityEvalTaskRequestMatcher(
                        EqualsProto(GetExpectedEligibilityEvalTaskRequest(
                            enable_confidential_aggregation))))))
        .WillOnce(Return(FakeHttpResponse(
            200, HeaderList(), eval_task_response.SerializeAsString())));

    // The 'EET received' callback should be called, even if the task resource
    // data was available inline.
    if (eligibility_eval_enabled) {
      EXPECT_CALL(mock_eet_received_callback_,
                  Call(FieldsAre(FieldsAre("", ""), kEligibilityEvalExecutionId,
                                 Eq(std::nullopt), _)));
    }

    return federated_protocol_
        ->EligibilityEvalCheckin(mock_eet_received_callback_.AsStdFunction())
        .status();
  }

  // This function runs a successful Checkin() that results in a
  // task assignment payload being returned by the server. This is a
  // utility function used by Report*() tests that depend on a prior,
  // successful execution of Checkin(). It returns a
  // absl::StatusOr<CheckinResult>, which the caller should verify is OK using
  // ASSERT_OK.
  absl::StatusOr<FederatedProtocol::CheckinResult> RunSuccessfulCheckin(
      bool report_eligibility_eval_result = true,
      std::optional<std::string> confidential_data_access_policy = std::nullopt,
      bool set_relative_uri = false,
      std::optional<std::string> signed_endorsements = std::nullopt) {
    // We return a fake response which returns the plan/initial checkpoint
    // data inline, to keep things simple.
    std::string expected_plan = kPlan;
    std::string plan_uri = "https://fake.uri/plan";
    Resource plan_resource;
    plan_resource.set_uri(plan_uri);
    std::string expected_checkpoint = kInitCheckpoint;
    Resource checkpoint_resource;
    checkpoint_resource.mutable_inline_resource()->set_data(
        expected_checkpoint);
    std::string expected_aggregation_session_id = kAggregationSessionId;
    std::optional<Resource> confidential_agg_resource;
    std::optional<Resource> signed_endorsements_resource;
    if (confidential_data_access_policy.has_value()) {
      confidential_agg_resource = Resource();
      confidential_agg_resource->mutable_inline_resource()->set_data(
          *confidential_data_access_policy);
      if (signed_endorsements.has_value()) {
        signed_endorsements_resource = Resource();
        signed_endorsements_resource->mutable_inline_resource()->set_data(
            *signed_endorsements);
      }
    }
    StartTaskAssignmentResponse task_assignment_response =
        GetFakeTaskAssignmentResponse(
            plan_resource, checkpoint_resource, kFederatedSelectUriTemplate,
            expected_aggregation_session_id, 0,
            set_relative_uri ? "/" : kAggregationTargetUri,
            confidential_agg_resource, signed_endorsements_resource);

    std::string request_uri;
    if (set_relative_uri) {
      request_uri =
          "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
          "taskassignments/ELIGIBILITY%2FSESSION%23ID:start?%24alt=proto";
    } else {
      request_uri =
          "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
          "taskassignments/ELIGIBILITY%2FSESSION%23ID:start?%24alt=proto";
    }
    TaskEligibilityInfo expected_eligibility_info =
        GetFakeTaskEligibilityInfo();
    EXPECT_CALL(mock_http_client_,
                PerformSingleRequest(SimpleHttpRequestMatcher(
                    request_uri, HttpRequest::Method::kPost, _,
                    StartTaskAssignmentRequestMatcher(
                        EqualsProto(GetExpectedStartTaskAssignmentRequest(
                            expected_eligibility_info,
                            confidential_data_access_policy.has_value()))))))
        .WillOnce(Return(FakeHttpResponse(
            200, HeaderList(),
            CreateDoneOperation(kOperationName, task_assignment_response)
                .SerializeAsString())));

    EXPECT_CALL(mock_http_client_,
                PerformSingleRequest(SimpleHttpRequestMatcher(
                    plan_uri, HttpRequest::Method::kGet, _, "")))
        .WillOnce(Return(FakeHttpResponse(200, HeaderList(), expected_plan)));

    if (report_eligibility_eval_result) {
      std::string report_eet_request_uri =
          "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
          "eligibilityevaltasks/"
          "ELIGIBILITY%2FSESSION%23ID:reportresult?%24alt=proto";
      ExpectSuccessfulReportEligibilityEvalTaskResultRequest(
          report_eet_request_uri, absl::OkStatus());
    }

    return federated_protocol_->Checkin(
        expected_eligibility_info, mock_task_received_callback_.AsStdFunction(),
        std::nullopt);
  }

  absl::StatusOr<FederatedProtocol::MultipleTaskAssignments>
  RunSuccessfulMultipleTaskAssignments(
      bool eligibility_eval_enabled = true,
      bool enable_confidential_aggregation = false,
      std::optional<Resource> confidential_data_access_policy = std::nullopt,
      std::optional<Resource> signed_endorsements = std::nullopt) {
    if (eligibility_eval_enabled) {
      std::string report_eet_request_uri =
          "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
          "eligibilityevaltasks/"
          "ELIGIBILITY%2FSESSION%23ID:reportresult?%24alt=proto";
      ExpectSuccessfulReportEligibilityEvalTaskResultRequest(
          report_eet_request_uri, absl::OkStatus());
    }

    std::vector<std::string> task_names{kMultiTaskId_1, kMultiTaskId_2};

    PerformMultipleTaskAssignmentsRequest request;
    request.mutable_client_version()->set_version_code(kClientVersion);
    request.mutable_resource_capabilities()->add_supported_compression_formats(
        ResourceCompressionFormat::RESOURCE_COMPRESSION_FORMAT_GZIP);
    if (enable_confidential_aggregation) {
      request.mutable_resource_capabilities()
          ->set_supports_confidential_aggregation(true);
    }
    for (const auto& task_name : task_names) {
      request.add_task_names(task_name);
    }

    PerformMultipleTaskAssignmentsResponse response;
    Resource plan_1;
    std::string expected_plan_1 = "plan1";
    *plan_1.mutable_inline_resource()->mutable_data() = expected_plan_1;
    Resource checkpoint_1;
    std::string expected_checkpoint_1 = "checkpoint1";
    *checkpoint_1.mutable_inline_resource()->mutable_data() =
        expected_checkpoint_1;
    *response.add_task_assignments() = CreateTaskAssignment(
        plan_1, checkpoint_1, kFederatedSelectUriTemplate,
        kMultiTaskClientSessionId_1, kMultiTaskAggregationSessionId_1,
        kMultiTaskId_1, kAggregationTargetUri,
        enable_confidential_aggregation
            ? 0
            : kMinimumClientsInServerVisibleAggregate,
        confidential_data_access_policy);
    Resource plan_2;
    std::string plan_uri = "https://fake.uri/plan";
    plan_2.set_uri(plan_uri);
    std::string checkpoint_uri = "https://fake.uri/checkpoint";
    Resource checkpoint_2;
    checkpoint_2.set_uri(checkpoint_uri);
    *response.add_task_assignments() = CreateTaskAssignment(
        plan_2, checkpoint_2, kFederatedSelectUriTemplate,
        kMultiTaskClientSessionId_2, kMultiTaskAggregationSessionId_2,
        kMultiTaskId_2, kAggregationTargetUri,
        enable_confidential_aggregation
            ? 0
            : kMinimumClientsInServerVisibleAggregate,
        confidential_data_access_policy);
    std::string expected_plan_2 = "expected_plan_2";
    std::string expected_checkpoint_2 = "expected_checkpoint_2";

    EXPECT_CALL(
        mock_http_client_,
        PerformSingleRequest(SimpleHttpRequestMatcher(
            "https://taskassignment.uri/v1/populations/"
            "TEST%2FPOPULATION/"
            "taskassignments/"
            "ELIGIBILITY%2FSESSION%23ID:performmultiple?%24alt=proto",
            HttpRequest::Method::kPost, _, request.SerializeAsString())))
        .WillOnce(Return(
            FakeHttpResponse(200, HeaderList(), response.SerializeAsString())));
    EXPECT_CALL(mock_multiple_tasks_received_callback_, Call(2));
    EXPECT_CALL(mock_http_client_,
                PerformSingleRequest(SimpleHttpRequestMatcher(
                    checkpoint_uri, HttpRequest::Method::kGet, _, "")))
        .WillOnce(
            Return(FakeHttpResponse(200, HeaderList(), expected_checkpoint_2)));
    EXPECT_CALL(mock_http_client_,
                PerformSingleRequest(SimpleHttpRequestMatcher(
                    plan_uri, HttpRequest::Method::kGet, _, "")))
        .WillOnce(Return(FakeHttpResponse(200, HeaderList(), expected_plan_2)));

    return federated_protocol_->PerformMultipleTaskAssignments(
        task_names, mock_multiple_tasks_received_callback_.AsStdFunction(),
        std::nullopt);
  }

  absl::Status RunSuccessfulUploadViaSimpleAgg(
      absl::string_view client_session_id,
      std::optional<std::string> task_identifier,
      absl::string_view aggregation_session_id, absl::string_view task_name,
      absl::Duration plan_duration, absl::string_view checkpoint_str,
      bool use_per_task_upload = true) {
    std::string report_task_result_request_url = absl::StrCat(
        "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
        "taskassignments/",
        client_session_id, ":reportresult?%24alt=proto");
    ExpectSuccessfulReportTaskResultRequest(report_task_result_request_url,
                                            aggregation_session_id, task_name,
                                            plan_duration);
    std::string start_aggregation_data_upload_request_url = absl::StrCat(
        "https://aggregation.uri/v1/aggregations/", aggregation_session_id,
        "/clients/AUTHORIZATION_TOKEN:startdataupload?%24alt=proto");
    ExpectSuccessfulStartAggregationDataUploadRequest(
        start_aggregation_data_upload_request_url, kResourceName,
        kByteStreamTargetUri, kSecondStageAggregationTargetUri);
    ExpectSuccessfulByteStreamUploadRequest(
        "https://bytestream.uri/upload/v1/media/"
        "CHECKPOINT_RESOURCE?upload_protocol=raw",
        checkpoint_str);
    std::string submit_aggregation_result_request_url = absl::StrCat(
        "https://aggregation.second.uri/v1/aggregations/",
        aggregation_session_id, "/clients/CLIENT_TOKEN:submit?%24alt=proto");
    ExpectSuccessfulSubmitAggregationResultRequest(
        submit_aggregation_result_request_url);
    ComputationResults results;
    results.emplace("tensorflow_checkpoint", std::string(checkpoint_str));
    if (use_per_task_upload) {
      return federated_protocol_->ReportCompleted(
          std::move(results), plan_duration, task_identifier,
          payload_metadata_);
    } else {
      return federated_protocol_->ReportCompleted(
          std::move(results), plan_duration, std::nullopt, payload_metadata_);
    }
  }

  void ExpectSuccessfulReportEligibilityEvalTaskResultRequest(
      absl::string_view expected_request_uri, absl::Status eet_status) {
    ReportEligibilityEvalTaskResultRequest report_eet_request;
    report_eet_request.set_status_code(
        static_cast<google::rpc::Code>(eet_status.code()));
    EXPECT_CALL(
        mock_http_client_,
        PerformSingleRequest(SimpleHttpRequestMatcher(
            std::string(expected_request_uri), HttpRequest::Method::kPost, _,
            ReportEligibilityEvalTaskResultRequestMatcher(
                EqualsProto(report_eet_request)))))
        .WillOnce(Return(FakeHttpResponse(200, HeaderList(), "")));
  }

  void ExpectSuccessfulReportTaskResultRequest(
      absl::string_view expected_report_result_uri,
      absl::string_view aggregation_session_id, absl::string_view task_name,
      absl::Duration plan_duration) {
    ReportTaskResultResponse report_task_result_response;
    EXPECT_CALL(mock_http_client_,
                PerformSingleRequest(SimpleHttpRequestMatcher(
                    std::string(expected_report_result_uri),
                    HttpRequest::Method::kPost, _,
                    ReportTaskResultRequestMatcher(
                        EqualsProto(GetExpectedReportTaskResultRequest(
                            aggregation_session_id, task_name,
                            google::rpc::Code::OK, plan_duration))))))
        .WillOnce(Return(CreateEmptySuccessHttpResponse()));
  }

  void ExpectSuccessfulStartAggregationDataUploadRequest(
      absl::string_view expected_start_data_upload_uri,
      absl::string_view aggregation_resource_name,
      absl::string_view byte_stream_uri_prefix,
      absl::string_view second_stage_aggregation_uri_prefix,
      bool set_relative_uri_prefix = false,
      std::optional<ConfidentialEncryptionConfig>
          confidential_encryption_config = std::nullopt) {
    Operation pending_operation_response =
        CreatePendingOperation("operations/foo#bar");
    EXPECT_CALL(
        mock_http_client_,
        PerformSingleRequest(SimpleHttpRequestMatcher(
            std::string(expected_start_data_upload_uri),
            HttpRequest::Method::kPost, _,
            confidential_encryption_config.has_value()
                ? StartConfidentialAggregationDataUploadRequest()
                      .SerializeAsString()
                : StartAggregationDataUploadRequest().SerializeAsString())))
        .WillOnce(Return(
            FakeHttpResponse(200, HeaderList(),
                             pending_operation_response.SerializeAsString())));
    std::string expected_operation_uri;
    if (set_relative_uri_prefix) {
      expected_operation_uri =
          "https://initial.uri/v1/operations/foo%23bar?%24alt=proto";
    } else {
      expected_operation_uri =
          "https://aggregation.uri/v1/operations/foo%23bar?%24alt=proto";
    }

    EXPECT_CALL(
        mock_http_client_,
        PerformSingleRequest(SimpleHttpRequestMatcher(
            // Note that the '#' character is encoded as "%23".
            expected_operation_uri, HttpRequest::Method::kGet, _,
            GetOperationRequestMatcher(EqualsProto(GetOperationRequest())))))
        .WillOnce(Return(FakeHttpResponse(
            200, HeaderList(),
            confidential_encryption_config.has_value()
                ? CreateDoneOperation(
                      kOperationName,
                      GetFakeStartConfidentialAggregationDataUploadResponse(
                          aggregation_resource_name, byte_stream_uri_prefix,
                          second_stage_aggregation_uri_prefix,
                          *confidential_encryption_config))
                      .SerializeAsString()
                : CreateDoneOperation(
                      kOperationName,
                      GetFakeStartAggregationDataUploadResponse(
                          aggregation_resource_name, byte_stream_uri_prefix,
                          second_stage_aggregation_uri_prefix))
                      .SerializeAsString())));
  }

  void ExpectSuccessfulByteStreamUploadRequest(
      absl::string_view byte_stream_upload_uri,
      absl::string_view checkpoint_str) {
    EXPECT_CALL(
        mock_http_client_,
        PerformSingleRequest(SimpleHttpRequestMatcher(
            std::string(byte_stream_upload_uri), HttpRequest::Method::kPost, _,
            std::string(checkpoint_str))))
        .WillOnce(Return(CreateEmptySuccessHttpResponse()));
  }

  void ExpectSuccessfulSubmitAggregationResultRequest(
      absl::string_view expected_submit_aggregation_result_uri,
      bool confidential_aggregation = false) {
    std::string expected_request_proto;
    if (confidential_aggregation) {
      SubmitConfidentialAggregationResultRequest
          submit_aggregation_result_request;
      submit_aggregation_result_request.set_resource_name(kResourceName);
      expected_request_proto =
          submit_aggregation_result_request.SerializeAsString();
    } else {
      SubmitAggregationResultRequest submit_aggregation_result_request;
      submit_aggregation_result_request.set_resource_name(kResourceName);
      expected_request_proto =
          submit_aggregation_result_request.SerializeAsString();
    }
    EXPECT_CALL(mock_http_client_,
                PerformSingleRequest(SimpleHttpRequestMatcher(
                    std::string(expected_submit_aggregation_result_uri),
                    HttpRequest::Method::kPost, _, expected_request_proto)))
        .WillOnce(Return(CreateEmptySuccessHttpResponse()));
  }

  void ExpectSuccessfulAbortAggregationRequest(
      absl::string_view base_uri, bool confidential_aggregation = false) {
    EXPECT_CALL(mock_http_client_,
                PerformSingleRequest(SimpleHttpRequestMatcher(
                    absl::StrCat(base_uri,
                                 confidential_aggregation
                                     ? "/v1/confidentialaggregations/"
                                     : "/v1/aggregations/",
                                 "AGGREGATION_SESSION_ID/clients/"
                                 "CLIENT_TOKEN:abort?%24alt=proto"),
                    HttpRequest::Method::kPost, _, _)))
        .WillOnce(Return(CreateEmptySuccessHttpResponse()));
  }

  StrictMock<MockHttpClient> mock_http_client_;
  StrictMock<MockSecAggRunnerFactory>* mock_secagg_runner_factory_ =
      new StrictMock<MockSecAggRunnerFactory>();
  StrictMock<MockSecAggEventPublisher> mock_secagg_event_publisher_;
  StrictMock<MockLogManager> mock_log_manager_;
  NiceMock<MockFlags> mock_flags_;
  NiceMock<MockFunction<bool()>> mock_should_abort_;
  StrictMock<cache::MockResourceCache> mock_resource_cache_;
  StrictMock<MockAttestationVerifier>* mock_attestation_verifier_ =
      new StrictMock<MockAttestationVerifier>();
  Clock* clock_ = Clock::RealClock();
  NiceMock<MockFunction<void(
      const ::fcp::client::FederatedProtocol::EligibilityEvalTask&)>>
      mock_eet_received_callback_;
  NiceMock<MockFunction<void(
      const ::fcp::client::FederatedProtocol::TaskAssignment&)>>
      mock_task_received_callback_;
  NiceMock<MockFunction<void(size_t)>> mock_multiple_tasks_received_callback_;

  // The class under test.
  std::unique_ptr<HttpFederatedProtocol> federated_protocol_;
  // The payload metadata to be uploaded to the server.
  std::optional<confidentialcompute::PayloadMetadata> payload_metadata_;
};

using HttpFederatedProtocolDeathTest = HttpFederatedProtocolTest;

TEST_F(HttpFederatedProtocolTest,
       TestTransientErrorRetryWindowDifferentAcrossDifferentInstances) {
  const ::google::internal::federatedml::v2::RetryWindow& retry_window1 =
      federated_protocol_->GetLatestRetryWindow();
  ExpectTransientErrorRetryWindow(retry_window1);
  federated_protocol_.reset(nullptr);
  mock_secagg_runner_factory_ = new StrictMock<MockSecAggRunnerFactory>();
  mock_attestation_verifier_ = new StrictMock<MockAttestationVerifier>();

  // Create a new HttpFederatedProtocol instance. It should not produce the same
  // retry window value as the one we just got. This is a simple correctness
  // check to ensure that the value is at least randomly generated (and that we
  // don't accidentally use the random number generator incorrectly).
  federated_protocol_ = std::make_unique<HttpFederatedProtocol>(
      clock_, &mock_log_manager_, &mock_flags_, &mock_http_client_,
      absl::WrapUnique(mock_secagg_runner_factory_),
      &mock_secagg_event_publisher_, &mock_resource_cache_,
      absl::WrapUnique(mock_attestation_verifier_), kEntryPointUri, kApiKey,
      kPopulationName, kRetryToken, kClientVersion, kAttestationMeasurement,
      mock_should_abort_.AsStdFunction(), absl::BitGen(),
      InterruptibleRunner::TimingConfig{
          .polling_period = absl::ZeroDuration(),
          .graceful_shutdown_period = absl::InfiniteDuration(),
          .extended_shutdown_period = absl::InfiniteDuration()});

  const ::google::internal::federatedml::v2::RetryWindow& retry_window2 =
      federated_protocol_->GetLatestRetryWindow();
  ExpectTransientErrorRetryWindow(retry_window2);

  EXPECT_THAT(retry_window1, Not(EqualsProto(retry_window2)));
}

TEST_F(HttpFederatedProtocolTest,
       TestEligibilityEvalCheckinRequestFailsTransientError) {
  // Make the HTTP client return a 503 Service Unavailable error when the
  // EligibilityEvalCheckin(...) code issues the control protocol's HTTP
  // request. This should result in the error being returned as the result.
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://initial.uri/v1/eligibilityevaltasks/"
                  "TEST%2FPOPULATION:request?%24alt=proto",
                  HttpRequest::Method::kPost, _, _)))
      .WillOnce(Return(FakeHttpResponse(503, HeaderList(), "")));

  auto eligibility_checkin_result = federated_protocol_->EligibilityEvalCheckin(
      mock_eet_received_callback_.AsStdFunction());

  EXPECT_THAT(eligibility_checkin_result.status(), IsCode(UNAVAILABLE));
  EXPECT_THAT(eligibility_checkin_result.status().message(),
              HasSubstr("protocol request failed"));
  // The original 503 HTTP response code should be included in the message as
  // well.
  EXPECT_THAT(eligibility_checkin_result.status().message(), HasSubstr("503"));
  // No RetryWindows were received from the server, so we expect to get a
  // RetryWindow generated based on the transient errors retry delay flag.
  ExpectTransientErrorRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

TEST_F(HttpFederatedProtocolTest,
       TestEligibilityEvalCheckinRequestFailsPermanentError) {
  // Make the HTTP client return a 404 Not Found error when the
  // EligibilityEvalCheckin(...) code issues the control protocol's HTTP
  // request. This should result in the error being returned as the result.
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://initial.uri/v1/eligibilityevaltasks/"
                  "TEST%2FPOPULATION:request?%24alt=proto",
                  HttpRequest::Method::kPost, _, _)))
      .WillOnce(Return(FakeHttpResponse(404, HeaderList(), "")));

  auto eligibility_checkin_result = federated_protocol_->EligibilityEvalCheckin(
      mock_eet_received_callback_.AsStdFunction());

  EXPECT_THAT(eligibility_checkin_result.status(), IsCode(NOT_FOUND));
  EXPECT_THAT(eligibility_checkin_result.status().message(),
              HasSubstr("protocol request failed"));
  // The original 404 HTTP response code should be included in the message as
  // well.
  EXPECT_THAT(eligibility_checkin_result.status().message(), HasSubstr("404"));
  // No RetryWindows were received from the server, so we expect to get a
  // RetryWindow generated based on the *permanent* errors retry delay flag,
  // since NOT_FOUND is marked as a permanent error in the flags.
  ExpectPermanentErrorRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

// Tests the case where we get interrupted while waiting for a response to the
// protocol request in EligibilityEvalCheckin.
TEST_F(HttpFederatedProtocolTest,
       TestEligibilityEvalCheckinRequestInterrupted) {
  absl::Notification request_issued;
  absl::Notification request_cancelled;

  // Make HttpClient::PerformRequests() block until the counter is decremented.
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://initial.uri/v1/eligibilityevaltasks/"
                  "TEST%2FPOPULATION:request?%24alt=proto",
                  HttpRequest::Method::kPost, _, _)))
      .WillOnce([&request_issued, &request_cancelled](
                    MockableHttpClient::SimpleHttpRequest ignored) {
        request_issued.Notify();
        request_cancelled.WaitForNotification();
        return FakeHttpResponse(503, HeaderList(), "");
      });

  // Make should_abort return false until we know that the request was issued
  // (i.e. once InterruptibleRunner has actually started running the code it
  // was given), and then make it return true, triggering an abort sequence and
  // unblocking the PerformRequests()() call we caused to block above.
  EXPECT_CALL(mock_should_abort_, Call()).WillRepeatedly([&request_issued] {
    return request_issued.HasBeenNotified();
  });

  // When the HttpClient receives a HttpRequestHandle::Cancel call, we let the
  // request complete.
  mock_http_client_.SetCancellationListener(
      [&request_cancelled]() { request_cancelled.Notify(); });

  EXPECT_CALL(mock_log_manager_,
              LogDiag(ProdDiagCode::BACKGROUND_TRAINING_INTERRUPT_HTTP));

  auto eligibility_checkin_result = federated_protocol_->EligibilityEvalCheckin(
      mock_eet_received_callback_.AsStdFunction());

  EXPECT_THAT(eligibility_checkin_result.status(), IsCode(CANCELLED));
  // No RetryWindows were received from the server, so we expect to get a
  // RetryWindow generated based on the transient errors retry delay flag.
  ExpectTransientErrorRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

TEST_F(HttpFederatedProtocolTest, TestEligibilityEvalCheckinRejection) {
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://initial.uri/v1/eligibilityevaltasks/"
                  "TEST%2FPOPULATION:request?%24alt=proto",
                  HttpRequest::Method::kPost, _,
                  EligibilityEvalTaskRequestMatcher(
                      EqualsProto(GetExpectedEligibilityEvalTaskRequest())))))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          GetFakeRejectedEligibilityEvalTaskResponse().SerializeAsString())));

  // The 'eet received' callback should not be invoked since no EET was given to
  // the client.
  EXPECT_CALL(mock_eet_received_callback_, Call(_)).Times(0);

  auto eligibility_checkin_result = federated_protocol_->EligibilityEvalCheckin(
      mock_eet_received_callback_.AsStdFunction());

  ASSERT_OK(eligibility_checkin_result);
  EXPECT_THAT(*eligibility_checkin_result,
              VariantWith<FederatedProtocol::Rejection>(_));
  ExpectRejectedRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

TEST_F(HttpFederatedProtocolTest, TestEligibilityEvalCheckinDisabled) {
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://initial.uri/v1/eligibilityevaltasks/"
                  "TEST%2FPOPULATION:request?%24alt=proto",
                  HttpRequest::Method::kPost, _,
                  EligibilityEvalTaskRequestMatcher(
                      EqualsProto(GetExpectedEligibilityEvalTaskRequest())))))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          GetFakeDisabledEligibilityEvalTaskResponse().SerializeAsString())));

  // The 'eet received' callback should not be invoked since no EET was given to
  // the client.
  EXPECT_CALL(mock_eet_received_callback_, Call(_)).Times(0);

  auto eligibility_checkin_result = federated_protocol_->EligibilityEvalCheckin(
      mock_eet_received_callback_.AsStdFunction());

  ASSERT_OK(eligibility_checkin_result);
  EXPECT_THAT(*eligibility_checkin_result,
              VariantWith<FederatedProtocol::EligibilityEvalDisabled>(_));
  ExpectRejectedRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

TEST_F(HttpFederatedProtocolTest, TestEligibilityEvalCheckinEnabled) {
  // We return a fake response which requires fetching the plan via HTTP, but
  // which has the checkpoint data available inline.
  std::string expected_plan = kPlan;
  std::string plan_uri = "https://fake.uri/plan";
  Resource plan_resource;
  plan_resource.set_uri(plan_uri);
  std::string expected_checkpoint = kInitCheckpoint;
  Resource checkpoint_resource;
  checkpoint_resource.mutable_inline_resource()->set_data(expected_checkpoint);
  std::string expected_execution_id = kEligibilityEvalExecutionId;
  EligibilityEvalTaskResponse eval_task_response =
      GetFakeEnabledEligibilityEvalTaskResponse(
          plan_resource, checkpoint_resource, expected_execution_id);

  InSequence seq;
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://initial.uri/v1/eligibilityevaltasks/"
                  "TEST%2FPOPULATION:request?%24alt=proto",
                  HttpRequest::Method::kPost, _,
                  EligibilityEvalTaskRequestMatcher(
                      EqualsProto(GetExpectedEligibilityEvalTaskRequest())))))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), eval_task_response.SerializeAsString())));

  // The 'EET received' callback should be called *before* the actual task
  // resources are fetched.
  EXPECT_CALL(mock_eet_received_callback_,
              Call(FieldsAre(FieldsAre("", ""), expected_execution_id,
                             Eq(std::nullopt), _)));

  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  plan_uri, HttpRequest::Method::kGet, _, "")))
      .WillOnce(Return(FakeHttpResponse(200, HeaderList(), expected_plan)));

  auto eligibility_checkin_result = federated_protocol_->EligibilityEvalCheckin(
      mock_eet_received_callback_.AsStdFunction());

  ASSERT_OK(eligibility_checkin_result);
  EXPECT_THAT(
      *eligibility_checkin_result,
      VariantWith<FederatedProtocol::EligibilityEvalTask>(FieldsAre(
          AllOf(Field(&FederatedProtocol::PlanAndCheckpointPayloads::plan,
                      absl::Cord(expected_plan)),
                Field(&FederatedProtocol::PlanAndCheckpointPayloads::checkpoint,
                      absl::Cord(expected_checkpoint))),
          expected_execution_id, Eq(std::nullopt), _)));
  ExpectRejectedRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

TEST_F(HttpFederatedProtocolTest,
       TestEligibilityEvalCheckinWithPopulationEligibilitySpec) {
  // We return a fake response which requires fetching the plan via HTTP,
  // but which has the checkpoint data available inline.
  std::string expected_plan = kPlan;
  std::string plan_uri = "https://fake.uri/plan";
  Resource plan_resource;
  plan_resource.set_uri(plan_uri);
  std::string expected_checkpoint = kInitCheckpoint;
  Resource checkpoint_resource;
  checkpoint_resource.mutable_inline_resource()->set_data(expected_checkpoint);

  PopulationEligibilitySpec expected_population_eligibility_spec;
  // Add a SWOR policy to the population eligibility spec, having an EET with
  // no policies in the spec is not a valid configuration.
  auto* swor_spec =
      expected_population_eligibility_spec.mutable_eligibility_policies()
          ->Add();
  swor_spec->set_name("swor_policy_5_seconds");
  swor_spec->set_min_version(1);
  swor_spec->mutable_swor_policy()->mutable_min_period()->set_seconds(5);

  auto task_info = expected_population_eligibility_spec.add_task_info();
  task_info->set_task_name("task_1");
  task_info->set_task_assignment_mode(
      PopulationEligibilitySpec::TaskInfo::TASK_ASSIGNMENT_MODE_MULTIPLE);
  task_info->add_eligibility_policy_indices(0);
  std::string population_eligibility_spec_uri =
      "https://fake.uri/population_eligibility_spec";
  Resource population_eligibility_spec;
  population_eligibility_spec.set_uri(population_eligibility_spec_uri);
  std::string expected_execution_id = kEligibilityEvalExecutionId;
  EligibilityEvalTaskResponse eval_task_response =
      GetFakeEnabledEligibilityEvalTaskResponse(
          plan_resource, checkpoint_resource, expected_execution_id,
          kTaskAssignmentTargetUri, population_eligibility_spec);

  InSequence seq;
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://initial.uri/v1/eligibilityevaltasks/"
                  "TEST%2FPOPULATION:request?%24alt=proto",
                  HttpRequest::Method::kPost, _,
                  EligibilityEvalTaskRequestMatcher(
                      EqualsProto(GetExpectedEligibilityEvalTaskRequest())))))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), eval_task_response.SerializeAsString())));

  // The 'EET received' callback should be called *before* the actual task
  // resources are fetched.
  EXPECT_CALL(mock_eet_received_callback_,
              Call(FieldsAre(FieldsAre("", ""), expected_execution_id,
                             Eq(std::nullopt), _)));

  EXPECT_CALL(mock_http_client_, PerformSingleRequest(SimpleHttpRequestMatcher(
                                     population_eligibility_spec_uri,
                                     HttpRequest::Method::kGet, _, "")))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          expected_population_eligibility_spec.SerializeAsString())));
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  plan_uri, HttpRequest::Method::kGet, _, "")))
      .WillOnce(Return(FakeHttpResponse(200, HeaderList(), expected_plan)));

  auto eligibility_checkin_result = federated_protocol_->EligibilityEvalCheckin(
      mock_eet_received_callback_.AsStdFunction());

  ASSERT_OK(eligibility_checkin_result);
  EXPECT_THAT(
      *eligibility_checkin_result,
      VariantWith<FederatedProtocol::EligibilityEvalTask>(FieldsAre(
          AllOf(Field(&FederatedProtocol::PlanAndCheckpointPayloads::plan,
                      absl::Cord(expected_plan)),
                Field(&FederatedProtocol::PlanAndCheckpointPayloads::checkpoint,
                      absl::Cord(expected_checkpoint))),
          expected_execution_id,
          Optional(EqualsProto(expected_population_eligibility_spec)), _)));
  ExpectRejectedRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

TEST_F(HttpFederatedProtocolTest,
       TestEligibilityEvalCheckinWithPopulationEligibilitySpecInvalidFormat) {
  // We return a fake response which requires fetching the plan via HTTP,
  // but which has the checkpoint data available inline.
  std::string expected_plan = kPlan;
  std::string plan_uri = "https://fake.uri/plan";
  Resource plan_resource;
  plan_resource.set_uri(plan_uri);
  std::string expected_checkpoint = kInitCheckpoint;
  Resource checkpoint_resource;
  checkpoint_resource.mutable_inline_resource()->set_data(expected_checkpoint);

  Resource population_eligibility_spec;
  population_eligibility_spec.mutable_inline_resource()->set_data(
      "Invalid_spec");
  std::string expected_execution_id = kEligibilityEvalExecutionId;
  EligibilityEvalTaskResponse eval_task_response =
      GetFakeEnabledEligibilityEvalTaskResponse(
          plan_resource, checkpoint_resource, expected_execution_id,
          kTaskAssignmentTargetUri, population_eligibility_spec);

  InSequence seq;
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://initial.uri/v1/eligibilityevaltasks/"
                  "TEST%2FPOPULATION:request?%24alt=proto",
                  HttpRequest::Method::kPost, _,
                  EligibilityEvalTaskRequestMatcher(
                      EqualsProto(GetExpectedEligibilityEvalTaskRequest())))))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), eval_task_response.SerializeAsString())));

  EXPECT_CALL(mock_eet_received_callback_,
              Call(FieldsAre(FieldsAre("", ""), expected_execution_id,
                             Eq(std::nullopt), _)));

  auto eligibility_checkin_result = federated_protocol_->EligibilityEvalCheckin(
      mock_eet_received_callback_.AsStdFunction());

  ASSERT_THAT(eligibility_checkin_result, IsCode(INVALID_ARGUMENT));
  ExpectPermanentErrorRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

TEST_F(HttpFederatedProtocolTest,
       TestEligibilityEvalNotConfiguredWithPopulationEligibilitySpec) {
  Resource plan_resource;
  Resource checkpoint_resource;

  PopulationEligibilitySpec expected_population_eligibility_spec;
  auto task_info = expected_population_eligibility_spec.add_task_info();
  task_info->set_task_name("task_1");
  task_info->set_task_assignment_mode(
      PopulationEligibilitySpec::TaskInfo::TASK_ASSIGNMENT_MODE_MULTIPLE);
  std::string population_eligibility_spec_uri =
      "https://fake.uri/population_eligibility_spec";
  Resource population_eligibility_spec;
  population_eligibility_spec.set_uri(population_eligibility_spec_uri);
  std::string expected_execution_id = kEligibilityEvalExecutionId;
  EligibilityEvalTaskResponse eval_task_response =
      GetFakeEnabledEligibilityEvalTaskResponse(
          plan_resource, checkpoint_resource, expected_execution_id,
          kTaskAssignmentTargetUri, population_eligibility_spec);

  InSequence seq;
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://initial.uri/v1/eligibilityevaltasks/"
                  "TEST%2FPOPULATION:request?%24alt=proto",
                  HttpRequest::Method::kPost, _,
                  EligibilityEvalTaskRequestMatcher(
                      EqualsProto(GetExpectedEligibilityEvalTaskRequest())))))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), eval_task_response.SerializeAsString())));

  // The 'EET received' callback should be called *before* the actual task
  // resources are fetched.
  EXPECT_CALL(mock_eet_received_callback_,
              Call(FieldsAre(FieldsAre("", ""), expected_execution_id,
                             Eq(std::nullopt), _)));

  EXPECT_CALL(mock_http_client_, PerformSingleRequest(SimpleHttpRequestMatcher(
                                     population_eligibility_spec_uri,
                                     HttpRequest::Method::kGet, _, "")))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          expected_population_eligibility_spec.SerializeAsString())));

  auto eligibility_checkin_result = federated_protocol_->EligibilityEvalCheckin(
      mock_eet_received_callback_.AsStdFunction());

  ASSERT_OK(eligibility_checkin_result);
  EXPECT_THAT(
      *eligibility_checkin_result,
      VariantWith<FederatedProtocol::EligibilityEvalDisabled>(FieldsAre(
          Optional(EqualsProto(expected_population_eligibility_spec)), _)));
  ExpectRejectedRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

// In this case, the tasks in the population are using eligibility policies,
// and even though there is no TensorFlow-based eligibility eval task
// configured, we still return EligibilityEvalTask.
TEST_F(HttpFederatedProtocolTest,
       TestEligibilityEvalConfiguredWhenSpecHasPolicy) {
  Resource plan_resource;
  Resource checkpoint_resource;

  PopulationEligibilitySpec expected_population_eligibility_spec;
  auto* swor_spec =
      expected_population_eligibility_spec.mutable_eligibility_policies()
          ->Add();
  swor_spec->set_name("swor_policy_5_seconds");
  swor_spec->set_min_version(1);
  swor_spec->mutable_swor_policy()->mutable_min_period()->set_seconds(5);

  auto task_info = expected_population_eligibility_spec.add_task_info();
  task_info->set_task_name("task_1");
  task_info->set_task_assignment_mode(
      PopulationEligibilitySpec::TaskInfo::TASK_ASSIGNMENT_MODE_MULTIPLE);
  std::string population_eligibility_spec_uri =
      "https://fake.uri/population_eligibility_spec";
  Resource population_eligibility_spec;
  population_eligibility_spec.set_uri(population_eligibility_spec_uri);
  std::string expected_execution_id = kEligibilityEvalExecutionId;
  EligibilityEvalTaskResponse eval_task_response =
      GetFakeEnabledEligibilityEvalTaskResponse(
          plan_resource, checkpoint_resource, expected_execution_id,
          kTaskAssignmentTargetUri, population_eligibility_spec);

  InSequence seq;
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://initial.uri/v1/eligibilityevaltasks/"
                  "TEST%2FPOPULATION:request?%24alt=proto",
                  HttpRequest::Method::kPost, _,
                  EligibilityEvalTaskRequestMatcher(
                      EqualsProto(GetExpectedEligibilityEvalTaskRequest())))))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), eval_task_response.SerializeAsString())));

  // The 'EET received' callback should be called *before* the actual task
  // resources are fetched.
  EXPECT_CALL(mock_eet_received_callback_,
              Call(FieldsAre(FieldsAre("", ""), expected_execution_id,
                             Eq(std::nullopt), _)));

  EXPECT_CALL(mock_http_client_, PerformSingleRequest(SimpleHttpRequestMatcher(
                                     population_eligibility_spec_uri,
                                     HttpRequest::Method::kGet, _, "")))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          expected_population_eligibility_spec.SerializeAsString())));

  auto eligibility_checkin_result = federated_protocol_->EligibilityEvalCheckin(
      mock_eet_received_callback_.AsStdFunction());

  ASSERT_OK(eligibility_checkin_result);
  // Given that the flag is on, we expect EligibilityEvalTask to be returned
  // even though there is no TensorFlow-based eligibility eval task configured,
  // because the population eligibility spec has eligibility policies
  EXPECT_THAT(
      *eligibility_checkin_result,
      VariantWith<FederatedProtocol::EligibilityEvalTask>(FieldsAre(
          _, _, Optional(EqualsProto(expected_population_eligibility_spec)),
          _)));
  ExpectRejectedRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

TEST_F(HttpFederatedProtocolTest,
       TestEligibilityEvalCheckinEnabledWithCompression) {
  std::string expected_plan = kPlan;
  absl::StatusOr<std::string> compressed_plan = CompressWithGzip(expected_plan);
  ASSERT_OK(compressed_plan);
  Resource plan_resource;
  plan_resource.mutable_inline_resource()->set_data(*compressed_plan);
  plan_resource.mutable_inline_resource()->set_compression_format(
      ResourceCompressionFormat::RESOURCE_COMPRESSION_FORMAT_GZIP);
  std::string expected_checkpoint = kInitCheckpoint;
  absl::StatusOr<std::string> compressed_checkpoint =
      CompressWithGzip(expected_checkpoint);
  Resource checkpoint_resource;
  checkpoint_resource.mutable_inline_resource()->set_data(
      *compressed_checkpoint);
  checkpoint_resource.mutable_inline_resource()->set_compression_format(
      ResourceCompressionFormat::RESOURCE_COMPRESSION_FORMAT_GZIP);
  std::string expected_execution_id = kEligibilityEvalExecutionId;
  EligibilityEvalTaskResponse eval_task_response =
      GetFakeEnabledEligibilityEvalTaskResponse(
          plan_resource, checkpoint_resource, expected_execution_id);
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://initial.uri/v1/eligibilityevaltasks/"
                  "TEST%2FPOPULATION:request?%24alt=proto",
                  HttpRequest::Method::kPost, _, _)))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), eval_task_response.SerializeAsString())));

  auto eligibility_checkin_result = federated_protocol_->EligibilityEvalCheckin(
      mock_eet_received_callback_.AsStdFunction());

  ASSERT_OK(eligibility_checkin_result);
  EXPECT_THAT(
      *eligibility_checkin_result,
      VariantWith<FederatedProtocol::EligibilityEvalTask>(FieldsAre(
          AllOf(Field(&FederatedProtocol::PlanAndCheckpointPayloads::plan,
                      absl::Cord(expected_plan)),
                Field(&FederatedProtocol::PlanAndCheckpointPayloads::checkpoint,
                      absl::Cord(expected_checkpoint))),
          expected_execution_id, Eq(std::nullopt), _)));
  ExpectRejectedRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

// Ensures that if the plan resource fails to be downloaded, the error is
// correctly returned from the EligibilityEvalCheckin(...) method.
TEST_F(HttpFederatedProtocolTest,
       TestEligibilityEvalCheckinEnabledPlanDataFetchFailed) {
  std::string plan_uri = "https://fake.uri/plan";
  Resource plan_resource;
  plan_resource.set_uri(plan_uri);
  std::string checkpoint_uri = "https://fake.uri/checkpoint";
  Resource checkpoint_resource;
  checkpoint_resource.set_uri(checkpoint_uri);
  std::string expected_execution_id = kEligibilityEvalExecutionId;
  EligibilityEvalTaskResponse eval_task_response =
      GetFakeEnabledEligibilityEvalTaskResponse(
          plan_resource, checkpoint_resource, expected_execution_id);
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://initial.uri/v1/eligibilityevaltasks/"
                  "TEST%2FPOPULATION:request?%24alt=proto",
                  HttpRequest::Method::kPost, _, _)))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), eval_task_response.SerializeAsString())));

  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  checkpoint_uri, HttpRequest::Method::kGet, _, "")))
      .WillOnce(Return(FakeHttpResponse(200, HeaderList(), "")));

  // Mock a failed plan fetch.
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  plan_uri, HttpRequest::Method::kGet, _, "")))
      .WillOnce(Return(FakeHttpResponse(404, HeaderList(), "")));

  auto eligibility_checkin_result = federated_protocol_->EligibilityEvalCheckin(
      mock_eet_received_callback_.AsStdFunction());

  // The 404 error for the resource request should be reflected in the return
  // value.
  EXPECT_THAT(eligibility_checkin_result.status(), IsCode(NOT_FOUND));
  EXPECT_THAT(eligibility_checkin_result.status().message(),
              HasSubstr("plan fetch failed"));
  // The original 404 HTTP response code should be included in the message as
  // well.
  EXPECT_THAT(eligibility_checkin_result.status().message(), HasSubstr("404"));
  // Since the error type is considered a permanent error, we should get a
  // permanent error retry window.
  ExpectPermanentErrorRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

// Ensures that if the checkpoint resource fails to be downloaded, the error is
// correctly returned from the EligibilityEvalCheckin(...) method.
TEST_F(HttpFederatedProtocolTest,
       TestEligibilityEvalCheckinEnabledCheckpointDataFetchFailed) {
  std::string plan_uri = "https://fake.uri/plan";
  Resource plan_resource;
  plan_resource.set_uri(plan_uri);
  std::string checkpoint_uri = "https://fake.uri/checkpoint";
  Resource checkpoint_resource;
  checkpoint_resource.set_uri(checkpoint_uri);
  std::string expected_execution_id = kEligibilityEvalExecutionId;
  EligibilityEvalTaskResponse eval_task_response =
      GetFakeEnabledEligibilityEvalTaskResponse(
          plan_resource, checkpoint_resource, expected_execution_id);
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://initial.uri/v1/eligibilityevaltasks/"
                  "TEST%2FPOPULATION:request?%24alt=proto",
                  HttpRequest::Method::kPost, _, _)))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), eval_task_response.SerializeAsString())));

  // Mock a failed checkpoint fetch.
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  checkpoint_uri, HttpRequest::Method::kGet, _, "")))
      .WillOnce(Return(FakeHttpResponse(503, HeaderList(), "")));

  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  plan_uri, HttpRequest::Method::kGet, _, "")))
      .WillOnce(Return(FakeHttpResponse(200, HeaderList(), "")));

  auto eligibility_checkin_result = federated_protocol_->EligibilityEvalCheckin(
      mock_eet_received_callback_.AsStdFunction());

  // The 503 error for the resource request should be reflected in the return
  // value.
  EXPECT_THAT(eligibility_checkin_result.status(), IsCode(UNAVAILABLE));
  EXPECT_THAT(eligibility_checkin_result.status().message(),
              HasSubstr("checkpoint fetch failed"));
  // The original 503 HTTP response code should be included in the message as
  // well.
  EXPECT_THAT(eligibility_checkin_result.status().message(), HasSubstr("503"));
  // RetryWindows were received from the server before the error was received,
  // and the error is considered 'transient', so we expect to get a rejected
  // RetryWindow.
  ExpectRejectedRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

TEST_F(HttpFederatedProtocolTest, TestReportEligibilityEvalTaskResult) {
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  std::string report_eet_request_uri =
      "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
      "eligibilityevaltasks/"
      "ELIGIBILITY%2FSESSION%23ID:reportresult?%24alt=proto";
  ReportEligibilityEvalTaskResultRequest report_eet_request;
  report_eet_request.set_status_code(
      static_cast<google::rpc::Code>(absl::StatusCode::kCancelled));
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  report_eet_request_uri, HttpRequest::Method::kPost, _,
                  ReportEligibilityEvalTaskResultRequestMatcher(
                      EqualsProto(report_eet_request)))))
      .WillOnce(Return(FakeHttpResponse(200, HeaderList(), "")));

  federated_protocol_->ReportEligibilityEvalError(absl::CancelledError());
}

// Tests that the protocol correctly sanitizes any invalid values it may have
// received from the server.
TEST_F(HttpFederatedProtocolTest,
       TestNegativeMinMaxRetryDelayValueSanitization) {
  RetryWindow retry_window;
  retry_window.mutable_delay_min()->set_seconds(-1);
  retry_window.mutable_delay_max()->set_seconds(-2);

  // The above retry window's negative min/max values should be clamped to 0.
  RetryWindow expected_retry_window;
  expected_retry_window.mutable_delay_min()->set_seconds(0);
  expected_retry_window.mutable_delay_max()->set_seconds(0);

  EligibilityEvalTaskResponse eval_task_response =
      GetFakeEnabledEligibilityEvalTaskResponse(
          Resource(), Resource(), kEligibilityEvalExecutionId,
          kTaskAssignmentTargetUri, std::nullopt, retry_window, retry_window);
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://initial.uri/v1/eligibilityevaltasks/"
                  "TEST%2FPOPULATION:request?%24alt=proto",
                  HttpRequest::Method::kPost, _, _)))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), eval_task_response.SerializeAsString())));

  ASSERT_OK(federated_protocol_->EligibilityEvalCheckin(
      mock_eet_received_callback_.AsStdFunction()));

  const google::internal::federatedml::v2::RetryWindow& actual_retry_window =
      federated_protocol_->GetLatestRetryWindow();
  // The above retry window's invalid max value should be clamped to the min
  // value (minus some errors introduced by the inaccuracy of double
  // multiplication).
  EXPECT_THAT(actual_retry_window.delay_min().seconds() +
                  actual_retry_window.delay_min().nanos() / 1000000000.0,
              DoubleEq(0));
  EXPECT_THAT(actual_retry_window.delay_max().seconds() +
                  actual_retry_window.delay_max().nanos() / 1000000000.0,
              DoubleEq(0));
}

// Tests that the protocol correctly sanitizes any invalid values it may have
// received from the server.
TEST_F(HttpFederatedProtocolTest, TestInvalidMaxRetryDelayValueSanitization) {
  RetryWindow retry_window;
  retry_window.mutable_delay_min()->set_seconds(1234);
  retry_window.mutable_delay_max()->set_seconds(1233);  // less than delay_min

  EligibilityEvalTaskResponse eval_task_response =
      GetFakeEnabledEligibilityEvalTaskResponse(
          Resource(), Resource(), kEligibilityEvalExecutionId,
          kTaskAssignmentTargetUri, std::nullopt, retry_window, retry_window);
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://initial.uri/v1/eligibilityevaltasks/"
                  "TEST%2FPOPULATION:request?%24alt=proto",
                  HttpRequest::Method::kPost, _, _)))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), eval_task_response.SerializeAsString())));

  ASSERT_OK(federated_protocol_->EligibilityEvalCheckin(
      mock_eet_received_callback_.AsStdFunction()));

  const google::internal::federatedml::v2::RetryWindow& actual_retry_window =
      federated_protocol_->GetLatestRetryWindow();
  // The above retry window's invalid max value should be clamped to the min
  // value (minus some errors introduced by the inaccuracy of double
  // multiplication). Note that DoubleEq enforces too precise of bounds, so we
  // use DoubleNear instead.
  EXPECT_THAT(actual_retry_window.delay_min().seconds() +
                  actual_retry_window.delay_min().nanos() / 1000000000.0,
              DoubleNear(1234.0, 0.015));
  EXPECT_THAT(actual_retry_window.delay_max().seconds() +
                  actual_retry_window.delay_max().nanos() / 1000000000.0,
              DoubleNear(1234.0, 0.015));
}

TEST_F(HttpFederatedProtocolDeathTest,
       TestCheckinAfterFailedEligibilityEvalCheckin) {
  // Make the HTTP client return a 503 Service Unavailable error when the
  // EligibilityEvalCheckin(...) code issues the protocol HTTP request.
  // This should result in the error being returned as the result.
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://initial.uri/v1/eligibilityevaltasks/"
                  "TEST%2FPOPULATION:request?%24alt=proto",
                  HttpRequest::Method::kPost, _, _)))
      .WillOnce(Return(FakeHttpResponse(503, HeaderList(), "")));

  auto eligibility_checkin_result = federated_protocol_->EligibilityEvalCheckin(
      mock_eet_received_callback_.AsStdFunction());

  EXPECT_THAT(eligibility_checkin_result.status(), IsCode(UNAVAILABLE));

  // A Checkin(...) request should now fail, because Checkin(...) should only
  // be a called after a successful EligibilityEvalCheckin(...) request.
  ASSERT_DEATH(
      {
        auto unused = federated_protocol_->Checkin(
            std::nullopt, mock_task_received_callback_.AsStdFunction(),
            std::nullopt);
      },
      _);
}

TEST_F(HttpFederatedProtocolDeathTest,
       TestCheckinAfterEligibilityEvalCheckinRejection) {
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://initial.uri/v1/eligibilityevaltasks/"
                  "TEST%2FPOPULATION:request?%24alt=proto",
                  HttpRequest::Method::kPost, _,
                  EligibilityEvalTaskRequestMatcher(
                      EqualsProto(GetExpectedEligibilityEvalTaskRequest())))))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          GetFakeRejectedEligibilityEvalTaskResponse().SerializeAsString())));

  ASSERT_OK(federated_protocol_->EligibilityEvalCheckin(
      mock_eet_received_callback_.AsStdFunction()));

  // A Checkin(...) request should now fail, because Checkin(...) should only
  // be a called after a successful EligibilityEvalCheckin(...) request, with a
  // non-rejection response.
  ASSERT_DEATH(
      {
        auto unused = federated_protocol_->Checkin(
            std::nullopt, mock_task_received_callback_.AsStdFunction(),
            std::nullopt);
      },
      _);
}

TEST_F(HttpFederatedProtocolDeathTest,
       TestCheckinWithEligibilityInfoAfterEligibilityEvalCheckinDisabled) {
  ASSERT_OK(
      RunSuccessfulEligibilityEvalCheckin(/*eligibility_eval_enabled=*/false));

  // A Checkin(...) request with a TaskEligibilityInfo argument should now fail,
  // because such info should only be passed a successful
  // EligibilityEvalCheckin(...) request with an eligibility eval task in the
  // response.
  ASSERT_DEATH(
      {
        auto unused = federated_protocol_->Checkin(
            TaskEligibilityInfo(), mock_task_received_callback_.AsStdFunction(),
            std::nullopt);
      },
      _);
}

TEST_F(HttpFederatedProtocolDeathTest, TestCheckinWithMissingEligibilityInfo) {
  ASSERT_OK(
      RunSuccessfulEligibilityEvalCheckin(/*eligibility_eval_enabled=*/true));

  // A Checkin(...) request with a missing TaskEligibilityInfo should now fail,
  // as the protocol requires us to provide one based on the plan includes in
  // the eligibility eval checkin response payload..
  ASSERT_DEATH(
      {
        auto unused = federated_protocol_->Checkin(
            std::nullopt, mock_task_received_callback_.AsStdFunction(),
            std::nullopt);
      },
      _);
}

TEST_F(HttpFederatedProtocolDeathTest,
       TestCheckinAfterEligibilityEvalResourceDataFetchFailed) {
  Resource plan_resource;
  plan_resource.set_uri("https://fake.uri/plan");
  Resource checkpoint_resource;
  checkpoint_resource.set_uri("https://fake.uri/checkpoint");
  EligibilityEvalTaskResponse eval_task_response =
      GetFakeEnabledEligibilityEvalTaskResponse(
          plan_resource, checkpoint_resource, kEligibilityEvalExecutionId);
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://initial.uri/v1/eligibilityevaltasks/"
                  "TEST%2FPOPULATION:request?%24alt=proto",
                  HttpRequest::Method::kPost, _, _)))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), eval_task_response.SerializeAsString())));

  // Mock a failed plan/resource fetch.
  EXPECT_CALL(mock_http_client_, PerformSingleRequest(SimpleHttpRequestMatcher(
                                     _, HttpRequest::Method::kGet, _, "")))
      .WillRepeatedly(Return(FakeHttpResponse(503, HeaderList(), "")));

  auto eligibility_checkin_result = federated_protocol_->EligibilityEvalCheckin(
      mock_eet_received_callback_.AsStdFunction());

  // A Checkin(...) request should now fail, because Checkin(...) should only
  // be a called after a successful EligibilityEvalCheckin(...) request, with a
  // non-rejection response.
  ASSERT_DEATH(
      {
        auto unused = federated_protocol_->Checkin(
            TaskEligibilityInfo(), mock_task_received_callback_.AsStdFunction(),
            std::nullopt);
      },
      _);
}

// Ensures that if the HTTP layer returns an error code that maps to a transient
// error, it is handled correctly
TEST_F(HttpFederatedProtocolTest, TestCheckinFailsTransientError) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  std::string report_eet_request_uri =
      "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
      "eligibilityevaltasks/"
      "ELIGIBILITY%2FSESSION%23ID:reportresult?%24alt=proto";
  ExpectSuccessfulReportEligibilityEvalTaskResultRequest(report_eet_request_uri,
                                                         absl::OkStatus());

  // Make the HTTP request return an 503 Service Unavailable error when the
  // Checkin(...) code tries to send its first request. This should result in
  // the error being returned as the result.
  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
          "taskassignments/ELIGIBILITY%2FSESSION%23ID:start?%24alt=proto",
          HttpRequest::Method::kPost, _, _)))
      .WillOnce(Return(FakeHttpResponse(503, HeaderList(), "")));

  auto checkin_result = federated_protocol_->Checkin(
      GetFakeTaskEligibilityInfo(),
      mock_task_received_callback_.AsStdFunction(), std::nullopt);

  EXPECT_THAT(checkin_result.status(), IsCode(UNAVAILABLE));
  // The original 503 HTTP response code should be included in the message as
  // well.
  EXPECT_THAT(checkin_result.status().message(), HasSubstr("503"));
  // RetryWindows were already received from the server during the eligibility
  // eval checkin, so we expect to get a 'rejected' retry window.
  ExpectRejectedRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

// Ensures that if the HTTP layer returns an error code that maps to a permanent
// error, it is handled correctly.
TEST_F(HttpFederatedProtocolTest, TestCheckinFailsPermanentErrorFromHttp) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  std::string report_eet_request_uri =
      "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
      "eligibilityevaltasks/"
      "ELIGIBILITY%2FSESSION%23ID:reportresult?%24alt=proto";
  ExpectSuccessfulReportEligibilityEvalTaskResultRequest(report_eet_request_uri,
                                                         absl::OkStatus());

  // Make the HTTP request return an 404 Not Found error when the Checkin(...)
  // code tries to send its first request. This should result in the error being
  // returned as the result.
  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
          "taskassignments/ELIGIBILITY%2FSESSION%23ID:start?%24alt=proto",
          HttpRequest::Method::kPost, _, _)))
      .WillOnce(Return(FakeHttpResponse(404, HeaderList(), "")));

  auto checkin_result = federated_protocol_->Checkin(
      GetFakeTaskEligibilityInfo(),
      mock_task_received_callback_.AsStdFunction(), std::nullopt);

  EXPECT_THAT(checkin_result.status(), IsCode(NOT_FOUND));
  // The original 503 HTTP response code should be included in the message as
  // well.
  EXPECT_THAT(checkin_result.status().message(), HasSubstr("404"));
  // Even though RetryWindows were already received from the server during the
  // eligibility eval checkin, we expect a RetryWindow generated based on the
  // *permanent* errors retry delay flag, since NOT_FOUND is marked as a
  // permanent error in the flags, and permanent errors should always result in
  // permanent error windows (regardless of whether retry windows were already
  // received).
  ExpectPermanentErrorRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

// Ensures that if the HTTP layer returns a successful response, but it contains
// an Operation proto with a permanent error, that it is handled correctly.
TEST_F(HttpFederatedProtocolTest, TestCheckinFailsPermanentErrorFromOperation) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  std::string report_eet_request_uri =
      "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
      "eligibilityevaltasks/"
      "ELIGIBILITY%2FSESSION%23ID:reportresult?%24alt=proto";
  ExpectSuccessfulReportEligibilityEvalTaskResultRequest(report_eet_request_uri,
                                                         absl::OkStatus());

  // Make the HTTP request return successfully, but make it contain an Operation
  // proto that itself contains a permanent error. This should result in the
  // error being returned as the result.
  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
          "taskassignments/ELIGIBILITY%2FSESSION%23ID:start?%24alt=proto",
          HttpRequest::Method::kPost, _, _)))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreateErrorOperation(kOperationName, absl::StatusCode::kNotFound,
                               "foo")
              .SerializeAsString())));

  auto checkin_result = federated_protocol_->Checkin(
      GetFakeTaskEligibilityInfo(),
      mock_task_received_callback_.AsStdFunction(), std::nullopt);

  EXPECT_THAT(checkin_result.status(), IsCode(NOT_FOUND));
  EXPECT_THAT(checkin_result.status().message(),
              HasSubstr("Operation my_operation contained error"));
  // The original error message should be included in the message as well.
  EXPECT_THAT(checkin_result.status().message(), HasSubstr("foo"));
  // Even though RetryWindows were already received from the server during the
  // eligibility eval checkin, we expect a RetryWindow generated based on the
  // *permanent* errors retry delay flag, since NOT_FOUND is marked as a
  // permanent error in the flags, and permanent errors should always result in
  // permanent error windows (regardless of whether retry windows were already
  // received).
  ExpectPermanentErrorRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

// Tests the case where we get interrupted while waiting for a response to the
// protocol request in Checkin.
TEST_F(HttpFederatedProtocolTest, TestCheckinInterrupted) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  std::string report_eet_request_uri =
      "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
      "eligibilityevaltasks/"
      "ELIGIBILITY%2FSESSION%23ID:reportresult?%24alt=proto";
  ExpectSuccessfulReportEligibilityEvalTaskResultRequest(report_eet_request_uri,
                                                         absl::OkStatus());

  absl::Notification request_issued;
  absl::Notification request_cancelled;

  // Make HttpClient::PerformRequests() block until the counter is decremented.
  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
          "taskassignments/ELIGIBILITY%2FSESSION%23ID:start?%24alt=proto",
          HttpRequest::Method::kPost, _, _)))
      .WillOnce([&request_issued, &request_cancelled](
                    MockableHttpClient::SimpleHttpRequest ignored) {
        request_issued.Notify();
        request_cancelled.WaitForNotification();
        return FakeHttpResponse(503, HeaderList(), "");
      });

  // Make should_abort return false until we know that the request was issued
  // (i.e. once InterruptibleRunner has actually started running the code it
  // was given), and then make it return true, triggering an abort sequence and
  // unblocking the PerformRequests()() call we caused to block above.
  EXPECT_CALL(mock_should_abort_, Call()).WillRepeatedly([&request_issued] {
    return request_issued.HasBeenNotified();
  });

  // When the HttpClient receives a HttpRequestHandle::Cancel call, we let the
  // request complete.
  mock_http_client_.SetCancellationListener([&request_cancelled]() {
    if (!request_cancelled.HasBeenNotified()) {
      request_cancelled.Notify();
    }
  });

  EXPECT_CALL(mock_log_manager_,
              LogDiag(ProdDiagCode::BACKGROUND_TRAINING_INTERRUPT_HTTP));

  auto checkin_result = federated_protocol_->Checkin(
      GetFakeTaskEligibilityInfo(),
      mock_task_received_callback_.AsStdFunction(), std::nullopt);
  EXPECT_THAT(checkin_result.status(), IsCode(CANCELLED));
  // RetryWindows were already received from the server during the eligibility
  // eval checkin, so we expect to get a 'rejected' retry window.
  ExpectRejectedRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

// Tests the case where we get interrupted during polling of the long running
// operation.
TEST_F(HttpFederatedProtocolTest,
       TestCheckinInterruptedDuringLongRunningOperation) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  std::string report_eet_request_uri =
      "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
      "eligibilityevaltasks/"
      "ELIGIBILITY%2FSESSION%23ID:reportresult?%24alt=proto";
  ExpectSuccessfulReportEligibilityEvalTaskResultRequest(report_eet_request_uri,
                                                         absl::OkStatus());

  absl::Notification request_issued;
  absl::Notification request_cancelled;

  Operation pending_operation = CreatePendingOperation("operations/foo#bar");
  // Make HttpClient::PerformRequests() block until the counter is decremented.
  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
          "taskassignments/ELIGIBILITY%2FSESSION%23ID:start?%24alt=proto",
          HttpRequest::Method::kPost, _, _)))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), pending_operation.SerializeAsString())));

  // Make should_abort return false until we know that the request was issued
  // (i.e. once InterruptibleRunner has actually started running the code it
  // was given), and then make it return true, triggering an abort sequence and
  // unblocking the PerformRequests()() call we caused to block above.
  EXPECT_CALL(mock_should_abort_, Call()).WillRepeatedly([&request_issued] {
    return request_issued.HasBeenNotified();
  });
  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          // Note that the '#' character is encoded as "%23".
          "https://taskassignment.uri/v1/operations/foo%23bar?%24alt=proto",
          HttpRequest::Method::kGet, _,
          GetOperationRequestMatcher(EqualsProto(GetOperationRequest())))))
      .WillRepeatedly([&request_issued, &request_cancelled, pending_operation](
                          MockableHttpClient::SimpleHttpRequest ignored) {
        if (!request_issued.HasBeenNotified()) {
          request_issued.Notify();
        }
        request_cancelled.WaitForNotification();
        return FakeHttpResponse(200, HeaderList(),
                                pending_operation.SerializeAsString());
      });

  // Once the client is cancelled, a CancelOperationRequest should still be sent
  // out before returning to the caller."
  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          // Note that the '#' character is encoded as "%23".
          "https://taskassignment.uri/v1/operations/"
          "foo%23bar:cancel?%24alt=proto",
          HttpRequest::Method::kPost, _,
          GetOperationRequestMatcher(EqualsProto(GetOperationRequest())))))
      .WillOnce(Return(FakeHttpResponse(200, HeaderList(), "")));

  // When the HttpClient receives a HttpRequestHandle::Cancel call, we let the
  // request complete.
  mock_http_client_.SetCancellationListener(
      [&request_cancelled]() { request_cancelled.Notify(); });

  EXPECT_CALL(mock_log_manager_,
              LogDiag(ProdDiagCode::BACKGROUND_TRAINING_INTERRUPT_HTTP));

  auto checkin_result = federated_protocol_->Checkin(
      GetFakeTaskEligibilityInfo(),
      mock_task_received_callback_.AsStdFunction(), std::nullopt);
  EXPECT_THAT(checkin_result.status(), IsCode(CANCELLED));
  // RetryWindows were already received from the server during the eligibility
  // eval checkin, so we expect to get a 'rejected' retry window.
  ExpectRejectedRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

// Tests the case where we get interrupted during polling of the long-running
// operation, and the issued cancellation request timed out.
TEST_F(HttpFederatedProtocolTest, TestCheckinInterruptedCancellationTimeout) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  std::string report_eet_request_uri =
      "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
      "eligibilityevaltasks/"
      "ELIGIBILITY%2FSESSION%23ID:reportresult?%24alt=proto";
  ExpectSuccessfulReportEligibilityEvalTaskResultRequest(report_eet_request_uri,
                                                         absl::OkStatus());

  absl::Notification request_issued;
  absl::Notification request_cancelled;

  Operation pending_operation = CreatePendingOperation("operations/foo#bar");
  // Make HttpClient::PerformRequests() block until the counter is decremented.
  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
          "taskassignments/ELIGIBILITY%2FSESSION%23ID:start?%24alt=proto",
          HttpRequest::Method::kPost, _, _)))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), pending_operation.SerializeAsString())));

  // Make should_abort return false until we know that the request was issued
  // (i.e. once InterruptibleRunner has actually started running the code it
  // was given), and then make it return true, triggering an abort sequence and
  // unblocking the PerformRequests()() call we caused to block above.
  EXPECT_CALL(mock_should_abort_, Call()).WillRepeatedly([&request_issued] {
    return request_issued.HasBeenNotified();
  });
  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          // Note that the '#' character is encoded as "%23".
          "https://taskassignment.uri/v1/operations/foo%23bar?%24alt=proto",
          HttpRequest::Method::kGet, _,
          GetOperationRequestMatcher(EqualsProto(GetOperationRequest())))))
      .WillRepeatedly([&request_issued, &request_cancelled, pending_operation](
                          MockableHttpClient::SimpleHttpRequest ignored) {
        if (!request_issued.HasBeenNotified()) {
          request_issued.Notify();
        }
        request_cancelled.WaitForNotification();
        return FakeHttpResponse(200, HeaderList(),
                                pending_operation.SerializeAsString());
      });

  // Once the client is cancelled, a CancelOperationRequest should still be sent
  // out before returning to the caller."
  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          // Note that the '#' character is encoded as "%23".
          "https://taskassignment.uri/v1/operations/"
          "foo%23bar:cancel?%24alt=proto",
          HttpRequest::Method::kPost, _,
          GetOperationRequestMatcher(EqualsProto(GetOperationRequest())))))
      .WillOnce([](MockableHttpClient::SimpleHttpRequest ignored) {
        // Sleep for 2 seconds before returning the response.
        absl::SleepFor(absl::Seconds(2));
        return FakeHttpResponse(200, HeaderList(), "");
      });

  // When the HttpClient receives a HttpRequestHandle::Cancel call, we let the
  // request complete.
  mock_http_client_.SetCancellationListener([&request_cancelled]() {
    if (!request_cancelled.HasBeenNotified()) {
      request_cancelled.Notify();
    }
  });

  // The Interruption log will be logged twice, one for Get operation, the other
  // for Cancel operation.
  EXPECT_CALL(mock_log_manager_,
              LogDiag(ProdDiagCode::BACKGROUND_TRAINING_INTERRUPT_HTTP))
      .Times(2);
  EXPECT_CALL(mock_log_manager_,
              LogDiag(ProdDiagCode::HTTP_CANCELLATION_OR_ABORT_REQUEST_FAILED));

  auto checkin_result = federated_protocol_->Checkin(
      GetFakeTaskEligibilityInfo(),
      mock_task_received_callback_.AsStdFunction(), std::nullopt);
  EXPECT_THAT(checkin_result.status(), IsCode(CANCELLED));
  // RetryWindows were already received from the server during the eligibility
  // eval checkin, so we expect to get a 'rejected' retry window.
  ExpectRejectedRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

// Tests whether 'rejection' responses to the main Checkin(...) request are
// handled correctly.
TEST_F(HttpFederatedProtocolTest, TestCheckinRejectionWithTaskEligibilityInfo) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  std::string report_eet_request_uri =
      "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
      "eligibilityevaltasks/"
      "ELIGIBILITY%2FSESSION%23ID:reportresult?%24alt=proto";
  ExpectSuccessfulReportEligibilityEvalTaskResultRequest(report_eet_request_uri,
                                                         absl::OkStatus());

  TaskEligibilityInfo expected_eligibility_info = GetFakeTaskEligibilityInfo();
  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
          "taskassignments/ELIGIBILITY%2FSESSION%23ID:start?%24alt=proto",
          HttpRequest::Method::kPost, _,
          StartTaskAssignmentRequestMatcher(
              EqualsProto(GetExpectedStartTaskAssignmentRequest(
                  expected_eligibility_info))))))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreateDoneOperation(kOperationName,
                              GetFakeRejectedTaskAssignmentResponse())
              .SerializeAsString())));

  // The 'task received' callback should not be invoked since no task was given
  // to the client.
  EXPECT_CALL(mock_task_received_callback_, Call(_)).Times(0);

  // Issue the regular checkin.
  auto checkin_result = federated_protocol_->Checkin(
      expected_eligibility_info, mock_task_received_callback_.AsStdFunction(),
      std::nullopt);

  ASSERT_OK(checkin_result.status());
  EXPECT_THAT(*checkin_result, VariantWith<FederatedProtocol::Rejection>(_));
  ExpectRejectedRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

// Tests whether we can issue a Checkin() request correctly without passing a
// TaskEligibilityInfo, in the case that the eligibility eval checkin didn't
// return any eligibility eval task to run.
TEST_F(HttpFederatedProtocolTest,
       TestCheckinRejectionWithoutTaskEligibilityInfo) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(
      RunSuccessfulEligibilityEvalCheckin(/*eligibility_eval_enabled=*/false));

  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
          "taskassignments/ELIGIBILITY%2FSESSION%23ID:start?%24alt=proto",
          HttpRequest::Method::kPost, _,
          StartTaskAssignmentRequestMatcher(EqualsProto(
              GetExpectedStartTaskAssignmentRequest(std::nullopt))))))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreateDoneOperation(kOperationName,
                              GetFakeRejectedTaskAssignmentResponse())
              .SerializeAsString())));

  // The 'task received' callback should not be invoked since no task was given
  // to the client.
  EXPECT_CALL(mock_task_received_callback_, Call(_)).Times(0);

  // Issue the regular checkin, without a TaskEligibilityInfo (since we didn't
  // receive an eligibility eval task to run during eligibility eval checkin).
  auto checkin_result = federated_protocol_->Checkin(
      std::nullopt, mock_task_received_callback_.AsStdFunction(), std::nullopt);

  ASSERT_OK(checkin_result.status());
  EXPECT_THAT(*checkin_result, VariantWith<FederatedProtocol::Rejection>(_));
  ExpectRejectedRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

// Tests whether a successful task assignment response is handled correctly.
TEST_F(HttpFederatedProtocolTest, TestCheckinTaskAssigned) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  std::string report_eet_request_uri =
      "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
      "eligibilityevaltasks/"
      "ELIGIBILITY%2FSESSION%23ID:reportresult?%24alt=proto";
  ExpectSuccessfulReportEligibilityEvalTaskResultRequest(report_eet_request_uri,
                                                         absl::OkStatus());

  TaskEligibilityInfo expected_eligibility_info = GetFakeTaskEligibilityInfo();
  // We return a fake response which requires fetching the plan via HTTP, but
  // which has the checkpoint data available inline.
  std::string expected_plan = kPlan;
  std::string plan_uri = "https://fake.uri/plan";
  Resource plan_resource;
  plan_resource.set_uri(plan_uri);
  std::string expected_checkpoint = kInitCheckpoint;
  Resource checkpoint_resource;
  checkpoint_resource.mutable_inline_resource()->set_data(expected_checkpoint);
  std::string expected_federated_select_uri_template =
      kFederatedSelectUriTemplate;
  std::string expected_aggregation_session_id = kAggregationSessionId;

  InSequence seq;
  // Note that in this particular test we check that the CheckinRequest is as
  // expected (in all prior tests we just use the '_' matcher, because the
  // request isn't really relevant to the test).
  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
          "taskassignments/ELIGIBILITY%2FSESSION%23ID:start?%24alt=proto",
          HttpRequest::Method::kPost, _,
          StartTaskAssignmentRequestMatcher(
              EqualsProto(GetExpectedStartTaskAssignmentRequest(
                  expected_eligibility_info))))))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreateDoneOperation(kOperationName,
                              GetFakeTaskAssignmentResponse(
                                  plan_resource, checkpoint_resource,
                                  expected_federated_select_uri_template,
                                  expected_aggregation_session_id,
                                  kMinimumClientsInServerVisibleAggregate))
              .SerializeAsString())));

  // The 'task received' callback should be called *before* the actual task
  // resources are fetched.
  EXPECT_CALL(
      mock_task_received_callback_,
      Call(FieldsAre(
          FieldsAre("", ""), expected_federated_select_uri_template,
          expected_aggregation_session_id,
          Optional(FieldsAre(_, Eq(kMinimumClientsInServerVisibleAggregate))),
          Eq(std::nullopt), kTaskName, _)));

  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  plan_uri, HttpRequest::Method::kGet, _, "")))
      .WillOnce(Return(FakeHttpResponse(200, HeaderList(), expected_plan)));

  // Issue the regular checkin.
  auto checkin_result = federated_protocol_->Checkin(
      expected_eligibility_info, mock_task_received_callback_.AsStdFunction(),
      std::nullopt);

  ASSERT_OK(checkin_result.status());
  EXPECT_THAT(
      *checkin_result,
      VariantWith<FederatedProtocol::TaskAssignment>(FieldsAre(
          FieldsAre(absl::Cord(expected_plan), absl::Cord(expected_checkpoint)),
          expected_federated_select_uri_template,
          expected_aggregation_session_id,
          Optional(FieldsAre(_, Eq(kMinimumClientsInServerVisibleAggregate))),
          Eq(std::nullopt), kTaskName, _)));
  // The Checkin call is expected to return the accepted retry window from the
  // response to the first eligibility eval request.
  ExpectAcceptedRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

TEST_F(HttpFederatedProtocolTest, TestCheckinTaskAssignedWithTaskIdentifier) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  auto checkin_result = RunSuccessfulCheckin();
  ASSERT_OK(checkin_result);
  EXPECT_THAT(*checkin_result,
              VariantWith<FederatedProtocol::TaskAssignment>(
                  FieldsAre(_, _, _, _, _, _, "task_default")));
}

TEST_F(HttpFederatedProtocolTest,
       TestMultiTaskAssignmentCalledAfterCheckinTaskAssigned) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin(
      /*eligibility_eval_enabled=*/true));
  ASSERT_OK(RunSuccessfulCheckin());
  // A PerformMultipleTaskAssignments(...) request should now fail, because
  // PerformMultipleTaskAssignments(...) should only be called before a
  // CheckIn(...) request.
  ASSERT_DEATH(
      {
        auto unused = federated_protocol_->PerformMultipleTaskAssignments(
            {kMultiTaskId_1, kMultiTaskId_2},
            mock_multiple_tasks_received_callback_.AsStdFunction(),
            std::nullopt);
      },
      _);
}

TEST_F(HttpFederatedProtocolTest,
       TestMultipleTaskAssignmentsWithConfidentialAggregation) {
  EXPECT_CALL(mock_flags_, enable_confidential_aggregation)
      .WillRepeatedly(Return(true));

  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin(
      /*eligibility_eval_enabled=*/true,
      /*enable_confidential_aggregation=*/true));
  std::string serialized_access_policy = "the access policy";
  Resource access_policy_resource;
  access_policy_resource.mutable_inline_resource()->set_data(
      serialized_access_policy);
  auto result = RunSuccessfulMultipleTaskAssignments(
      /*eligibility_eval_enabled*/ true,
      /*enable_confidential_aggregation=*/true,
      /*confidential_data_access_policy=*/access_policy_resource);
  ASSERT_OK(result);
  EXPECT_THAT(result->task_assignments, testing::SizeIs(2));
  absl::Cord expected_access_policy(serialized_access_policy);
  auto task_assignment_1 = result->task_assignments[kMultiTaskId_1];
  ASSERT_OK(task_assignment_1);
  EXPECT_EQ(task_assignment_1->confidential_agg_info.value().data_access_policy,
            expected_access_policy);

  auto task_assignment_2 = result->task_assignments[kMultiTaskId_2];
  ASSERT_OK(task_assignment_2);
  EXPECT_EQ(task_assignment_2->confidential_agg_info.value().data_access_policy,
            expected_access_policy);
}

TEST_F(
    HttpFederatedProtocolTest,
    TestMultipleTaskAssignmentsWithConfidentialAggregationAndTaskIdentifier) {
  EXPECT_CALL(mock_flags_, enable_confidential_aggregation)
      .WillRepeatedly(Return(true));

  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin(
      /*eligibility_eval_enabled=*/true,
      /*enable_confidential_aggregation=*/true));
  std::string serialized_access_policy = "the access policy";
  Resource access_policy_resource;
  access_policy_resource.mutable_inline_resource()->set_data(
      serialized_access_policy);
  auto result = RunSuccessfulMultipleTaskAssignments(
      /*eligibility_eval_enabled*/ true,
      /*enable_confidential_aggregation=*/true,
      /*confidential_data_access_policy=*/access_policy_resource);
  ASSERT_OK(result);
  EXPECT_THAT(result->task_assignments, testing::SizeIs(2));
  absl::Cord expected_access_policy(serialized_access_policy);
  auto task_assignment_1 = result->task_assignments[kMultiTaskId_1];
  ASSERT_OK(task_assignment_1);
  EXPECT_EQ(task_assignment_1->confidential_agg_info.value().data_access_policy,
            expected_access_policy);
  EXPECT_EQ(task_assignment_1->task_identifier, "task_0");

  auto task_assignment_2 = result->task_assignments[kMultiTaskId_2];
  ASSERT_OK(task_assignment_2);
  EXPECT_EQ(task_assignment_2->confidential_agg_info.value().data_access_policy,
            expected_access_policy);
  EXPECT_EQ(task_assignment_2->task_identifier, "task_1");
}

// Ensures that polling the Operation returned by a StartTaskAssignmentRequest
// works as expected. This serves mostly as a high-level check. Further
// polling-specific behavior is tested in more detail in
// ProtocolRequestHelperTest.
TEST_F(HttpFederatedProtocolTest,
       TestCheckinTaskAssignedAfterOperationPolling) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  std::string report_eet_request_uri =
      "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
      "eligibilityevaltasks/"
      "ELIGIBILITY%2FSESSION%23ID:reportresult?%24alt=proto";
  ExpectSuccessfulReportEligibilityEvalTaskResultRequest(report_eet_request_uri,
                                                         absl::OkStatus());

  // Make the initial StartTaskAssignmentRequest return a pending Operation
  // result. Note that we use a '#' character in the operation name to allow us
  // to verify that it is properly URL-encoded.
  Operation pending_operation_response =
      CreatePendingOperation("operations/foo#bar");
  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
          "taskassignments/ELIGIBILITY%2FSESSION%23ID:start?%24alt=proto",
          HttpRequest::Method::kPost, _, _)))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), pending_operation_response.SerializeAsString())));

  // Then, after letting the operation get polled twice more, eventually return
  // a fake response.
  std::string expected_plan = kPlan;
  Resource plan_resource;
  plan_resource.mutable_inline_resource()->set_data(expected_plan);
  std::string expected_checkpoint = kInitCheckpoint;
  Resource checkpoint_resource;
  checkpoint_resource.mutable_inline_resource()->set_data(expected_checkpoint);
  std::string expected_federated_select_uri_template =
      kFederatedSelectUriTemplate;
  std::string expected_aggregation_session_id = kAggregationSessionId;

  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          // Note that the '#' character is encoded as "%23".
          "https://taskassignment.uri/v1/operations/foo%23bar?%24alt=proto",
          HttpRequest::Method::kGet, _,
          GetOperationRequestMatcher(EqualsProto(GetOperationRequest())))))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), pending_operation_response.SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), pending_operation_response.SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreateDoneOperation(kOperationName,
                              GetFakeTaskAssignmentResponse(
                                  plan_resource, checkpoint_resource,
                                  expected_federated_select_uri_template,
                                  expected_aggregation_session_id, 0))
              .SerializeAsString())));

  // The 'task received' callback should be called, even if the task resource
  // data was available inline.
  EXPECT_CALL(
      mock_task_received_callback_,
      Call(FieldsAre(FieldsAre("", ""), expected_federated_select_uri_template,
                     expected_aggregation_session_id, Eq(std::nullopt),
                     Eq(std::nullopt), kTaskName, _)));

  // Issue the regular checkin.
  auto checkin_result = federated_protocol_->Checkin(
      GetFakeTaskEligibilityInfo(),
      mock_task_received_callback_.AsStdFunction(), std::nullopt);

  ASSERT_OK(checkin_result.status());
  EXPECT_THAT(
      *checkin_result,
      VariantWith<FederatedProtocol::TaskAssignment>(FieldsAre(
          FieldsAre(absl::Cord(expected_plan), absl::Cord(expected_checkpoint)),
          expected_federated_select_uri_template,
          expected_aggregation_session_id, Eq(std::nullopt), Eq(std::nullopt),
          kTaskName, _)));
  // The Checkin call is expected to return the accepted retry window from the
  // response to the first eligibility eval request.
  ExpectAcceptedRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

// Ensures that if the plan resource fails to be downloaded, the error is
// correctly returned from the Checkin(...) method.
TEST_F(HttpFederatedProtocolTest, TestCheckinTaskAssignedPlanDataFetchFailed) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  std::string report_eet_request_uri =
      "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
      "eligibilityevaltasks/"
      "ELIGIBILITY%2FSESSION%23ID:reportresult?%24alt=proto";
  ExpectSuccessfulReportEligibilityEvalTaskResultRequest(report_eet_request_uri,
                                                         absl::OkStatus());

  std::string plan_uri = "https://fake.uri/plan";
  Resource plan_resource;
  plan_resource.set_uri(plan_uri);
  std::string checkpoint_uri = "https://fake.uri/checkpoint";
  Resource checkpoint_resource;
  checkpoint_resource.set_uri(checkpoint_uri);
  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
          "taskassignments/ELIGIBILITY%2FSESSION%23ID:start?%24alt=proto",
          HttpRequest::Method::kPost, _, _)))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreateDoneOperation(
              kOperationName,
              GetFakeTaskAssignmentResponse(plan_resource, checkpoint_resource,
                                            kFederatedSelectUriTemplate,
                                            kAggregationSessionId, 0))
              .SerializeAsString())));

  // Mock a failed plan fetch.
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  plan_uri, HttpRequest::Method::kGet, _, "")))
      .WillOnce(Return(FakeHttpResponse(404, HeaderList(), "")));

  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  checkpoint_uri, HttpRequest::Method::kGet, _, "")))
      .WillOnce(Return(FakeHttpResponse(200, HeaderList(), "")));

  // Issue the regular checkin.
  auto checkin_result = federated_protocol_->Checkin(
      GetFakeTaskEligibilityInfo(),
      mock_task_received_callback_.AsStdFunction(), std::nullopt);

  // The 404 error for the resource request should be reflected in the return
  // value.
  EXPECT_THAT(checkin_result.status(), IsCode(NOT_FOUND));
  EXPECT_THAT(checkin_result.status().message(),
              HasSubstr("plan fetch failed"));
  EXPECT_THAT(checkin_result.status().message(), HasSubstr("404"));
  // The Checkin call is expected to return the permanent error retry window,
  // since 404 maps to a permanent error.
  ExpectPermanentErrorRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

// Ensures that if the checkpoint resource fails to be downloaded, the error is
// correctly returned from the Checkin(...) method.
TEST_F(HttpFederatedProtocolTest,
       TestCheckinTaskAssignedCheckpointDataFetchFailed) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  std::string report_eet_request_uri =
      "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
      "eligibilityevaltasks/"
      "ELIGIBILITY%2FSESSION%23ID:reportresult?%24alt=proto";
  ExpectSuccessfulReportEligibilityEvalTaskResultRequest(report_eet_request_uri,
                                                         absl::OkStatus());

  std::string plan_uri = "https://fake.uri/plan";
  Resource plan_resource;
  plan_resource.set_uri(plan_uri);
  std::string checkpoint_uri = "https://fake.uri/checkpoint";
  Resource checkpoint_resource;
  checkpoint_resource.set_uri(checkpoint_uri);

  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
          "taskassignments/ELIGIBILITY%2FSESSION%23ID:start?%24alt=proto",
          HttpRequest::Method::kPost, _, _)))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreateDoneOperation(
              kOperationName,
              GetFakeTaskAssignmentResponse(plan_resource, checkpoint_resource,
                                            kFederatedSelectUriTemplate,
                                            kAggregationSessionId, 0))
              .SerializeAsString())));

  // Mock a failed checkpoint fetch.
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  checkpoint_uri, HttpRequest::Method::kGet, _, "")))
      .WillOnce(Return(FakeHttpResponse(503, HeaderList(), "")));

  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  plan_uri, HttpRequest::Method::kGet, _, "")))
      .WillOnce(Return(FakeHttpResponse(200, HeaderList(), "")));

  // Issue the regular checkin.
  auto checkin_result = federated_protocol_->Checkin(
      GetFakeTaskEligibilityInfo(),
      mock_task_received_callback_.AsStdFunction(), std::nullopt);

  // The 503 error for the resource request should be reflected in the return
  // value.
  EXPECT_THAT(checkin_result.status(), IsCode(UNAVAILABLE));
  EXPECT_THAT(checkin_result.status().message(),
              HasSubstr("checkpoint fetch failed"));
  EXPECT_THAT(checkin_result.status().message(), HasSubstr("503"));
  // The Checkin call is expected to return the rejected retry window from the
  // response to the first eligibility eval request.
  ExpectRejectedRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

TEST_F(HttpFederatedProtocolTest,
       TestPerformMultipleTaskAssignmentsNoTaskAvailable) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin(
      /*eligibility_eval_enabled=*/true));
  std::string report_eet_request_uri =
      "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
      "eligibilityevaltasks/"
      "ELIGIBILITY%2FSESSION%23ID:reportresult?%24alt=proto";
  ExpectSuccessfulReportEligibilityEvalTaskResultRequest(report_eet_request_uri,
                                                         absl::OkStatus());

  std::vector<std::string> task_names{kMultiTaskId_1, kMultiTaskId_2};

  PerformMultipleTaskAssignmentsRequest request;
  request.mutable_client_version()->set_version_code(kClientVersion);
  request.mutable_resource_capabilities()->add_supported_compression_formats(
      ResourceCompressionFormat::RESOURCE_COMPRESSION_FORMAT_GZIP);
  for (const auto& task_name : task_names) {
    request.add_task_names(task_name);
  }
  PerformMultipleTaskAssignmentsResponse response;

  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
                  "taskassignments/"
                  "ELIGIBILITY%2FSESSION%23ID:performmultiple?%24alt=proto",
                  HttpRequest::Method::kPost, _, request.SerializeAsString())))
      .WillOnce(Return(
          FakeHttpResponse(200, HeaderList(), response.SerializeAsString())));

  auto multiTaskAssignmentResult =
      federated_protocol_->PerformMultipleTaskAssignments(
          task_names, mock_multiple_tasks_received_callback_.AsStdFunction(),
          std::nullopt);
  EXPECT_OK(multiTaskAssignmentResult);
  // The Checkin call is expected to return the rejected retry window from
  // the response to the first eligibility eval request.
  ExpectRejectedRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

TEST_F(HttpFederatedProtocolTest, TestPerformMultipleTaskAssignmentsFailed) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin(
      /*eligibility_eval_enabled=*/true));
  std::string report_eet_request_uri =
      "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
      "eligibilityevaltasks/"
      "ELIGIBILITY%2FSESSION%23ID:reportresult?%24alt=proto";
  ExpectSuccessfulReportEligibilityEvalTaskResultRequest(report_eet_request_uri,
                                                         absl::OkStatus());

  std::vector<std::string> task_names{kMultiTaskId_1, kMultiTaskId_2};

  PerformMultipleTaskAssignmentsRequest request;
  request.mutable_client_version()->set_version_code(kClientVersion);
  request.mutable_resource_capabilities()->add_supported_compression_formats(
      ResourceCompressionFormat::RESOURCE_COMPRESSION_FORMAT_GZIP);
  for (const auto& task_name : task_names) {
    request.add_task_names(task_name);
  }

  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
                  "taskassignments/"
                  "ELIGIBILITY%2FSESSION%23ID:performmultiple?%24alt=proto",
                  HttpRequest::Method::kPost, _, request.SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(503, HeaderList(), "")));

  auto multi_task_assignment_result =
      federated_protocol_->PerformMultipleTaskAssignments(
          task_names, mock_multiple_tasks_received_callback_.AsStdFunction(),
          std::nullopt);
  EXPECT_THAT(multi_task_assignment_result,
              IsCode(absl::StatusCode::kUnavailable));
  EXPECT_THAT(multi_task_assignment_result.status().message(),
              HasSubstr("protocol request failed"));
  // The original 500 HTTP response code should be included in the message as
  // well.
  EXPECT_THAT(multi_task_assignment_result.status().message(),
              HasSubstr("503"));
  // The Checkin call is expected to return the rejected retry window from
  // the response to the first eligibility eval request.
  ExpectRejectedRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

TEST_F(HttpFederatedProtocolTest,
       TestPerformMultipleTaskAssignmentsFailedPermanentError) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin(
      /*eligibility_eval_enabled=*/true));
  std::string report_eet_request_uri =
      "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
      "eligibilityevaltasks/"
      "ELIGIBILITY%2FSESSION%23ID:reportresult?%24alt=proto";
  ExpectSuccessfulReportEligibilityEvalTaskResultRequest(report_eet_request_uri,
                                                         absl::OkStatus());

  std::vector<std::string> task_names{kMultiTaskId_1, kMultiTaskId_2};

  PerformMultipleTaskAssignmentsRequest request;
  request.mutable_client_version()->set_version_code(kClientVersion);
  request.mutable_resource_capabilities()->add_supported_compression_formats(
      ResourceCompressionFormat::RESOURCE_COMPRESSION_FORMAT_GZIP);
  for (const auto& task_name : task_names) {
    request.add_task_names(task_name);
  }

  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
                  "taskassignments/"
                  "ELIGIBILITY%2FSESSION%23ID:performmultiple?%24alt=proto",
                  HttpRequest::Method::kPost, _, request.SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(404, HeaderList(), "")));

  auto multi_task_assignment_result =
      federated_protocol_->PerformMultipleTaskAssignments(
          task_names, mock_multiple_tasks_received_callback_.AsStdFunction(),
          std::nullopt);
  EXPECT_THAT(multi_task_assignment_result,
              IsCode(absl::StatusCode::kNotFound));
  EXPECT_THAT(multi_task_assignment_result.status().message(),
              HasSubstr("protocol request failed"));
  // The original 404 HTTP response code should be included in the message as
  // well.
  EXPECT_THAT(multi_task_assignment_result.status().message(),
              HasSubstr("404"));
  // The Checkin call is expected to return the permanent error retry window.
  ExpectPermanentErrorRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

TEST_F(HttpFederatedProtocolTest, TestPerformMultipleTaskAssignmentsAccepted) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin(
      /*eligibility_eval_enabled=*/true));
  std::string report_eet_request_uri =
      "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
      "eligibilityevaltasks/"
      "ELIGIBILITY%2FSESSION%23ID:reportresult?%24alt=proto";
  ExpectSuccessfulReportEligibilityEvalTaskResultRequest(report_eet_request_uri,
                                                         absl::OkStatus());

  std::vector<std::string> task_names{kMultiTaskId_1, kMultiTaskId_2};

  PerformMultipleTaskAssignmentsRequest request;
  request.mutable_client_version()->set_version_code(kClientVersion);
  request.mutable_resource_capabilities()->add_supported_compression_formats(
      ResourceCompressionFormat::RESOURCE_COMPRESSION_FORMAT_GZIP);
  for (const auto& task_name : task_names) {
    request.add_task_names(task_name);
  }

  PerformMultipleTaskAssignmentsResponse response;
  Resource plan_1;
  std::string expected_plan_1 = "plan1";
  *plan_1.mutable_inline_resource()->mutable_data() = expected_plan_1;
  Resource checkpoint_1;
  std::string expected_checkpoint_1 = "checkpoint1";
  *checkpoint_1.mutable_inline_resource()->mutable_data() =
      expected_checkpoint_1;
  *response.add_task_assignments() = CreateTaskAssignment(
      plan_1, checkpoint_1, kFederatedSelectUriTemplate,
      kMultiTaskClientSessionId_1, kMultiTaskAggregationSessionId_1,
      kMultiTaskId_1, kAggregationTargetUri,
      kMinimumClientsInServerVisibleAggregate);
  Resource plan_2;
  std::string plan_uri = "https://fake.uri/plan";
  plan_2.set_uri(plan_uri);
  std::string checkpoint_uri = "https://fake.uri/checkpoint";
  Resource checkpoint_2;
  checkpoint_2.set_uri(checkpoint_uri);
  *response.add_task_assignments() = CreateTaskAssignment(
      plan_2, checkpoint_2, kFederatedSelectUriTemplate,
      kMultiTaskClientSessionId_2, kMultiTaskAggregationSessionId_2,
      kMultiTaskId_2, kAggregationTargetUri,
      kMinimumClientsInServerVisibleAggregate);
  std::string expected_plan_2 = "expected_plan_2";
  std::string expected_checkpoint_2 = "expected_checkpoint_2";

  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://taskassignment.uri/v1/populations/"
                  "TEST%2FPOPULATION/"
                  "taskassignments/"
                  "ELIGIBILITY%2FSESSION%23ID:performmultiple?%24alt=proto",
                  HttpRequest::Method::kPost, _, request.SerializeAsString())))
      .WillOnce(Return(
          FakeHttpResponse(200, HeaderList(), response.SerializeAsString())));
  EXPECT_CALL(mock_multiple_tasks_received_callback_, Call(2));
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  checkpoint_uri, HttpRequest::Method::kGet, _, "")))
      .WillOnce(
          Return(FakeHttpResponse(200, HeaderList(), expected_checkpoint_2)));
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  plan_uri, HttpRequest::Method::kGet, _, "")))
      .WillOnce(Return(FakeHttpResponse(200, HeaderList(), expected_plan_2)));

  auto multiple_task_assignment_result =
      federated_protocol_->PerformMultipleTaskAssignments(
          task_names, mock_multiple_tasks_received_callback_.AsStdFunction(),
          std::nullopt);
  ASSERT_OK(multiple_task_assignment_result);
  EXPECT_THAT(multiple_task_assignment_result->task_assignments,
              UnorderedElementsAre(
                  Pair(kMultiTaskId_1,
                       IsOkAndHolds(FieldsAre(
                           FieldsAre(absl::Cord(expected_plan_1),
                                     absl::Cord(expected_checkpoint_1)),
                           kFederatedSelectUriTemplate,
                           kMultiTaskAggregationSessionId_1,
                           Optional(FieldsAre(
                               _, Eq(kMinimumClientsInServerVisibleAggregate))),
                           Eq(std::nullopt), kMultiTaskId_1, _))),
                  Pair(kMultiTaskId_2,
                       IsOkAndHolds(FieldsAre(
                           FieldsAre(absl::Cord(expected_plan_2),
                                     absl::Cord(expected_checkpoint_2)),
                           kFederatedSelectUriTemplate,
                           kMultiTaskAggregationSessionId_2,
                           Optional(FieldsAre(
                               _, Eq(kMinimumClientsInServerVisibleAggregate))),
                           Eq(std::nullopt), kMultiTaskId_2, _)))));
  ExpectRejectedRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

TEST_F(HttpFederatedProtocolTest,
       TestMultipleTaskAssignmentsCreateTaskIdentifier) {
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin(
      /*eligibility_eval_enabled=*/true));
  auto multiple_task_assignment_result = RunSuccessfulMultipleTaskAssignments();
  ASSERT_OK(multiple_task_assignment_result);
  auto task_assignments = multiple_task_assignment_result->task_assignments;
  EXPECT_EQ(task_assignments.size(), 2);
  EXPECT_EQ(task_assignments[kMultiTaskId_1]->task_identifier, "task_0");
  EXPECT_EQ(task_assignments[kMultiTaskId_2]->task_identifier, "task_1");
}

TEST_F(HttpFederatedProtocolTest,
       TestPerformMultipleTaskAssignmentsPayloadRetrievalFailed) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin(
      /*eligibility_eval_enabled=*/true));
  std::string report_eet_request_uri =
      "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
      "eligibilityevaltasks/"
      "ELIGIBILITY%2FSESSION%23ID:reportresult?%24alt=proto";
  ExpectSuccessfulReportEligibilityEvalTaskResultRequest(report_eet_request_uri,
                                                         absl::OkStatus());

  std::vector<std::string> task_names{kMultiTaskId_1, kMultiTaskId_2};

  PerformMultipleTaskAssignmentsRequest request;
  request.mutable_client_version()->set_version_code(kClientVersion);
  request.mutable_resource_capabilities()->add_supported_compression_formats(
      ResourceCompressionFormat::RESOURCE_COMPRESSION_FORMAT_GZIP);
  for (const auto& task_name : task_names) {
    request.add_task_names(task_name);
  }

  PerformMultipleTaskAssignmentsResponse response;
  Resource plan_1;
  std::string expected_plan_1 = "plan1";
  *plan_1.mutable_inline_resource()->mutable_data() = expected_plan_1;
  Resource checkpoint_1;
  std::string expected_checkpoint_1 = "checkpoint1";
  *checkpoint_1.mutable_inline_resource()->mutable_data() =
      expected_checkpoint_1;
  *response.add_task_assignments() = CreateTaskAssignment(
      plan_1, checkpoint_1, kFederatedSelectUriTemplate,
      kMultiTaskClientSessionId_1, kMultiTaskAggregationSessionId_1,
      kMultiTaskId_1, kAggregationTargetUri,
      kMinimumClientsInServerVisibleAggregate);
  Resource plan_2;
  std::string plan_uri = "https://fake.uri/plan";
  plan_2.set_uri(plan_uri);
  std::string checkpoint_uri = "https://fake.uri/checkpoint";
  Resource checkpoint_2;
  checkpoint_2.set_uri(checkpoint_uri);
  *response.add_task_assignments() = CreateTaskAssignment(
      plan_2, checkpoint_2, kFederatedSelectUriTemplate,
      kMultiTaskClientSessionId_2, kMultiTaskAggregationSessionId_2,
      kMultiTaskId_2, kAggregationTargetUri,
      kMinimumClientsInServerVisibleAggregate);

  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://taskassignment.uri/v1/populations/"
                  "TEST%2FPOPULATION/"
                  "taskassignments/"
                  "ELIGIBILITY%2FSESSION%23ID:performmultiple?%24alt=proto",
                  HttpRequest::Method::kPost, _, request.SerializeAsString())))
      .WillOnce(Return(
          FakeHttpResponse(200, HeaderList(), response.SerializeAsString())));
  EXPECT_CALL(mock_multiple_tasks_received_callback_, Call(2));
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  checkpoint_uri, HttpRequest::Method::kGet, _, "")))
      .WillOnce(Return(FakeHttpResponse(400, HeaderList(), "")));
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  plan_uri, HttpRequest::Method::kGet, _, "")))
      .WillOnce(Return(FakeHttpResponse(200, HeaderList(), "unused")));

  auto multiple_task_assignment_result =
      federated_protocol_->PerformMultipleTaskAssignments(
          task_names, mock_multiple_tasks_received_callback_.AsStdFunction(),
          std::nullopt);
  // We expect the overall result is ok because the
  // PerformMultipleTaskAssignments RPC succeeded. For the task assignments
  // inside the PerformMultipleTaskAssignmentsResponse, we expect one successful
  // result, one failure result.
  ASSERT_OK(multiple_task_assignment_result);
  EXPECT_THAT(
      multiple_task_assignment_result->task_assignments,
      UnorderedElementsAre(
          Pair(
              kMultiTaskId_1,
              IsOkAndHolds(FieldsAre(
                  FieldsAre(absl::Cord(expected_plan_1),
                            absl::Cord(expected_checkpoint_1)),
                  kFederatedSelectUriTemplate, kMultiTaskAggregationSessionId_1,
                  Optional(FieldsAre(
                      _, Eq(kMinimumClientsInServerVisibleAggregate))),
                  Eq(std::nullopt), kMultiTaskId_1, _))),
          Pair(kMultiTaskId_2, IsCode(absl::StatusCode::kInvalidArgument))));
  ExpectRejectedRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

TEST_F(HttpFederatedProtocolTest,
       TestPerformMultipleTaskAssignmentsPartialFailure) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin(
      /*eligibility_eval_enabled=*/true));
  std::string report_eet_request_uri =
      "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
      "eligibilityevaltasks/"
      "ELIGIBILITY%2FSESSION%23ID:reportresult?%24alt=proto";
  ExpectSuccessfulReportEligibilityEvalTaskResultRequest(report_eet_request_uri,
                                                         absl::OkStatus());

  std::vector<std::string> task_names{kMultiTaskId_1, kMultiTaskId_2};

  PerformMultipleTaskAssignmentsRequest request;
  request.mutable_client_version()->set_version_code(kClientVersion);
  request.mutable_resource_capabilities()->add_supported_compression_formats(
      ResourceCompressionFormat::RESOURCE_COMPRESSION_FORMAT_GZIP);
  for (const auto& task_name : task_names) {
    request.add_task_names(task_name);
  }

  PerformMultipleTaskAssignmentsResponse response;
  Resource plan_1;
  std::string expected_plan_1 = "plan1";
  *plan_1.mutable_inline_resource()->mutable_data() = expected_plan_1;
  Resource checkpoint_1;
  std::string expected_checkpoint_1 = "checkpoint1";
  *checkpoint_1.mutable_inline_resource()->mutable_data() =
      expected_checkpoint_1;
  *response.add_task_assignments() = CreateTaskAssignment(
      plan_1, checkpoint_1, kFederatedSelectUriTemplate,
      kMultiTaskClientSessionId_1, kMultiTaskAggregationSessionId_1,
      kMultiTaskId_1, kAggregationTargetUri,
      kMinimumClientsInServerVisibleAggregate);
  // Add invalid task assignment which has empty target uri prefix in the
  // forwarding info.
  auto* invalid_task_assignment = response.add_task_assignments();
  invalid_task_assignment->set_aggregation_id(kMultiTaskAggregationSessionId_2);
  invalid_task_assignment->set_task_name(kMultiTaskId_2);

  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://taskassignment.uri/v1/populations/"
                  "TEST%2FPOPULATION/"
                  "taskassignments/"
                  "ELIGIBILITY%2FSESSION%23ID:performmultiple?%24alt=proto",
                  HttpRequest::Method::kPost, _, request.SerializeAsString())))
      .WillOnce(Return(
          FakeHttpResponse(200, HeaderList(), response.SerializeAsString())));
  EXPECT_CALL(mock_multiple_tasks_received_callback_, Call(1));

  auto multiple_task_assignment_result =
      federated_protocol_->PerformMultipleTaskAssignments(
          task_names, mock_multiple_tasks_received_callback_.AsStdFunction(),
          std::nullopt);
  // We expect the overall result is ok because the
  // PerformMultipleTaskAssignments RPC succeeded. For the task assignments
  // inside the PerformMultipleTaskAssignmentsResponse, we expect one successful
  // result, one failure result.
  ASSERT_OK(multiple_task_assignment_result);
  EXPECT_THAT(
      multiple_task_assignment_result->task_assignments,
      UnorderedElementsAre(
          Pair(
              kMultiTaskId_1,
              IsOkAndHolds(FieldsAre(
                  FieldsAre(absl::Cord(expected_plan_1),
                            absl::Cord(expected_checkpoint_1)),
                  kFederatedSelectUriTemplate, kMultiTaskAggregationSessionId_1,
                  Optional(FieldsAre(
                      _, Eq(kMinimumClientsInServerVisibleAggregate))),
                  Eq(std::nullopt), kMultiTaskId_1, _))),
          Pair(kMultiTaskId_2, IsCode(absl::StatusCode::kInvalidArgument))));
  ExpectRejectedRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

TEST_F(HttpFederatedProtocolTest, TestReportCompletedViaSimpleAggSuccess) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  // Issue a regular checkin
  ASSERT_OK(RunSuccessfulCheckin());

  // Create a fake checkpoint with 32 'X'.
  std::string checkpoint_str(32, 'X');
  ComputationResults results;
  results.emplace("tensorflow_checkpoint", checkpoint_str);
  absl::Duration plan_duration = absl::Minutes(5);

  ExpectSuccessfulReportTaskResultRequest(
      "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
      "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
      kAggregationSessionId, kTaskName, plan_duration);
  ExpectSuccessfulStartAggregationDataUploadRequest(
      "https://aggregation.uri/v1/aggregations/AGGREGATION_SESSION_ID/"
      "clients/AUTHORIZATION_TOKEN:startdataupload?%24alt=proto",
      kResourceName, kByteStreamTargetUri, kSecondStageAggregationTargetUri);
  ExpectSuccessfulByteStreamUploadRequest(
      "https://bytestream.uri/upload/v1/media/"
      "CHECKPOINT_RESOURCE?upload_protocol=raw",
      checkpoint_str);
  ExpectSuccessfulSubmitAggregationResultRequest(
      "https://aggregation.second.uri/v1/aggregations/"
      "AGGREGATION_SESSION_ID/clients/CLIENT_TOKEN:submit?%24alt=proto");

  EXPECT_OK(federated_protocol_->ReportCompleted(
      std::move(results), plan_duration, std::nullopt, payload_metadata_));
}

// Validates that reporting a lightweight client report wire format result
// should work.
TEST_F(HttpFederatedProtocolTest,
       TestReportCompletedViaSimpleAggSuccessWithFCWireFormat) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  // Issue a regular checkin.
  ASSERT_OK(RunSuccessfulCheckin());
  // Enables enable_lightweight_client_report_wire_format flag.
  EXPECT_CALL(mock_flags_, enable_lightweight_client_report_wire_format())
      .WillRepeatedly(Return(true));
  // Create a fake checkpoint with 32 'X'.
  std::string checkpoint_str(32, 'X');
  absl::Cord checkpoint_cord(checkpoint_str);
  ComputationResults results;
  results.emplace("fc_checkpoint", checkpoint_cord);
  absl::Duration plan_duration = absl::Minutes(5);

  ExpectSuccessfulReportTaskResultRequest(
      "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
      "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
      kAggregationSessionId, kTaskName, plan_duration);
  ExpectSuccessfulStartAggregationDataUploadRequest(
      "https://aggregation.uri/v1/aggregations/AGGREGATION_SESSION_ID/"
      "clients/AUTHORIZATION_TOKEN:startdataupload?%24alt=proto",
      kResourceName, kByteStreamTargetUri, kSecondStageAggregationTargetUri);
  ExpectSuccessfulByteStreamUploadRequest(
      "https://bytestream.uri/upload/v1/media/"
      "CHECKPOINT_RESOURCE?upload_protocol=raw",
      checkpoint_str);
  ExpectSuccessfulSubmitAggregationResultRequest(
      "https://aggregation.second.uri/v1/aggregations/"
      "AGGREGATION_SESSION_ID/clients/CLIENT_TOKEN:submit?%24alt=proto");

  EXPECT_OK(federated_protocol_->ReportCompleted(
      std::move(results), plan_duration, std::nullopt, payload_metadata_));
}

// Validates that reporting a lightweight client report wire format result
// should fail if the feature is disabled.
TEST_F(HttpFederatedProtocolTest,
       TestReportCompletedWithLightweightWireFormatSupportDisabled) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  // Issue a regular checkin.
  ASSERT_OK(RunSuccessfulCheckin());
  // Disables enable_lightweight_client_report_wire_format flag.
  EXPECT_CALL(mock_flags_, enable_lightweight_client_report_wire_format())
      .WillRepeatedly(Return(false));
  // Create a fake lightweight result format with 32 'X'.
  std::string checkpoint_str(32, 'X');
  absl::Cord checkpoint_cord(checkpoint_str);
  ComputationResults results;
  results.emplace("fc_checkpoint", checkpoint_cord);
  absl::Duration plan_duration = absl::Minutes(5);

  // Should fail because the flag is disabled.
  EXPECT_THAT(
      federated_protocol_->ReportCompleted(std::move(results), plan_duration,
                                           std::nullopt, payload_metadata_),
      IsCode(absl::StatusCode::kInternal));
}

// Validates that reporting a tf checkpoint should work, even if the
// enable_lightweight_client_report_wire_format flag is enabled (e.g. because
// the task type that was run did not actually support producing a lightweight
// report, despite the flag being enabled).
TEST_F(HttpFederatedProtocolTest,
       TestReportWithTfCheckpointAndFcWireFormatFlagEnabled) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  // Issue a regular checkin.
  ASSERT_OK(RunSuccessfulCheckin());
  // Enables enable_lightweight_client_report_wire_format flag.
  EXPECT_CALL(mock_flags_, enable_lightweight_client_report_wire_format())
      .WillRepeatedly(Return(true));
  // Create a fake tf checkpoint with 32 'X'.
  std::string checkpoint_str(32, 'X');
  ComputationResults results;
  results.emplace("tensorflow_checkpoint", checkpoint_str);
  absl::Duration plan_duration = absl::Minutes(5);

  ExpectSuccessfulReportTaskResultRequest(
      "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
      "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
      kAggregationSessionId, kTaskName, plan_duration);
  ExpectSuccessfulStartAggregationDataUploadRequest(
      "https://aggregation.uri/v1/aggregations/AGGREGATION_SESSION_ID/"
      "clients/AUTHORIZATION_TOKEN:startdataupload?%24alt=proto",
      kResourceName, kByteStreamTargetUri, kSecondStageAggregationTargetUri);
  ExpectSuccessfulByteStreamUploadRequest(
      "https://bytestream.uri/upload/v1/media/"
      "CHECKPOINT_RESOURCE?upload_protocol=raw",
      checkpoint_str);
  ExpectSuccessfulSubmitAggregationResultRequest(
      "https://aggregation.second.uri/v1/aggregations/"
      "AGGREGATION_SESSION_ID/clients/CLIENT_TOKEN:submit?%24alt=proto");

  EXPECT_OK(federated_protocol_->ReportCompleted(
      std::move(results), plan_duration, std::nullopt, payload_metadata_));
}

// TODO(team): Remove this test once client_token is always populated in
// StartAggregationDataUploadResponse.
TEST_F(HttpFederatedProtocolTest,
       TestReportCompletedViaSimpleAggWithoutClientToken) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  // Issue a regular checkin
  ASSERT_OK(RunSuccessfulCheckin());

  // Create a fake checkpoint with 32 'X'.
  std::string checkpoint_str(32, 'X');
  ComputationResults results;
  results.emplace("tensorflow_checkpoint", checkpoint_str);
  absl::Duration plan_duration = absl::Minutes(5);

  ExpectSuccessfulReportTaskResultRequest(
      "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
      "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
      kAggregationSessionId, kTaskName, plan_duration);

  StartAggregationDataUploadResponse start_aggregation_data_upload_response =
      GetFakeStartAggregationDataUploadResponse(
          kResourceName, kByteStreamTargetUri,
          kSecondStageAggregationTargetUri);
  // Omit the client token from the response.
  start_aggregation_data_upload_response.clear_client_token();
  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://aggregation.uri/v1/aggregations/AGGREGATION_SESSION_ID/"
          "clients/AUTHORIZATION_TOKEN:startdataupload?%24alt=proto",
          HttpRequest::Method::kPost, _, _)))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreateDoneOperation(kOperationName,
                              start_aggregation_data_upload_response)
              .SerializeAsString())));

  ExpectSuccessfulByteStreamUploadRequest(
      "https://bytestream.uri/upload/v1/media/"
      "CHECKPOINT_RESOURCE?upload_protocol=raw",
      checkpoint_str);
  // SubmitAggregationResult should reuse the authorization token.
  ExpectSuccessfulSubmitAggregationResultRequest(
      "https://aggregation.second.uri/v1/aggregations/"
      "AGGREGATION_SESSION_ID/clients/AUTHORIZATION_TOKEN:submit?%24alt=proto");

  EXPECT_OK(federated_protocol_->ReportCompleted(
      std::move(results), plan_duration, std::nullopt, payload_metadata_));
}

TEST_F(HttpFederatedProtocolTest,
       TestReportCompletedViaConfidentialAggSuccess) {
  EXPECT_CALL(mock_flags_, enable_confidential_aggregation)
      .WillRepeatedly(Return(true));
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin(
      /*eligibility_eval_enabled=*/true,
      /*enable_confidential_aggregation*/ true));
  std::string serialized_access_policy = "the access policy";
  ASSERT_OK(RunSuccessfulCheckin(
      /*report_eligibility_eval_result*/ true,
      /*confidential_data_access_policy=*/serialized_access_policy));

  // Create a fake checkpoint with 32 'X'.
  std::string checkpoint_str(32, 'X');
  ComputationResults results;
  results.emplace("tensorflow_checkpoint", checkpoint_str);
  absl::Duration plan_duration = absl::Minutes(5);

  // Generate a new public key, which we'll pass to the client in the
  // ConfidentialEncryptionConfig. We'll use the decryptor from which the public
  // key was generated to validate the encrypted payload at the end of the test.
  fcp::confidential_compute::MessageDecryptor decryptor;
  auto encoded_public_key =
      decryptor
          .GetPublicKey(
              [](absl::string_view payload) { return "fakesignature"; }, 0)
          .value();
  absl::StatusOr<OkpCwt> parsed_public_key = OkpCwt::Decode(encoded_public_key);
  ASSERT_OK(parsed_public_key);
  ASSERT_TRUE(parsed_public_key->public_key.has_value());

  // Note: we don't specify any attestation evidence nor attestation
  // endorsements in the encryption config, since we can't generate valid
  // attestations in a test anyway.
  ConfidentialEncryptionConfig encryption_config;
  encryption_config.set_public_key(encoded_public_key);
  // Empty SignedEndorsements since the task does not use endorsements.
  confidentialcompute::SignedEndorsements signed_endorsements;

  // Ensure that the server's attestation evidence is considered valid.
  EXPECT_CALL(
      *mock_attestation_verifier_,
      Verify(Eq(serialized_access_policy), _, EqualsProto(encryption_config)))
      .WillRepeatedly(
          [=](const absl::Cord& access_policy,
              const confidentialcompute::SignedEndorsements&
                  signed_endorsements,
              const ConfidentialEncryptionConfig& encryption_config) {
            return attestation::AlwaysPassingAttestationVerifier().Verify(
                access_policy, signed_endorsements, encryption_config);
          });

  ExpectSuccessfulReportTaskResultRequest(
      "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
      "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
      kAggregationSessionId, kTaskName, plan_duration);
  ExpectSuccessfulStartAggregationDataUploadRequest(
      "https://aggregation.uri/v1/confidentialaggregations/"
      "AGGREGATION_SESSION_ID/"
      "clients/AUTHORIZATION_TOKEN:startdataupload?%24alt=proto",
      kResourceName, kByteStreamTargetUri, kSecondStageAggregationTargetUri,
      false, encryption_config);

  // Capture the raw uploaded data so we can subsequently validate that it was
  // properly encrypted with the public key that was provided to the client.
  std::string uploaded_data;
  EXPECT_CALL(mock_http_client_, PerformSingleRequest(SimpleHttpRequestMatcher(
                                     "https://bytestream.uri/upload/v1/media/"
                                     "CHECKPOINT_RESOURCE?upload_protocol=raw",
                                     HttpRequest::Method::kPost, _, _)))
      .WillOnce([&uploaded_data](MockHttpClient::SimpleHttpRequest request) {
        uploaded_data = request.body;
        return CreateEmptySuccessHttpResponse();
      });

  ExpectSuccessfulSubmitAggregationResultRequest(
      "https://aggregation.second.uri/v1/confidentialaggregations/"
      "AGGREGATION_SESSION_ID/clients/CLIENT_TOKEN:submit?%24alt=proto",
      /*confidential_aggregation=*/true);

  confidentialcompute::PayloadMetadata payload_metadata;
  payload_metadata.mutable_event_time_range()
      ->mutable_start_event_time()
      ->set_year(2025);
  payload_metadata.mutable_event_time_range()
      ->mutable_start_event_time()
      ->set_month(1);
  payload_metadata.mutable_event_time_range()
      ->mutable_start_event_time()
      ->set_day(1);
  payload_metadata.mutable_event_time_range()
      ->mutable_end_event_time()
      ->set_year(2025);
  payload_metadata.mutable_event_time_range()
      ->mutable_end_event_time()
      ->set_month(1);
  payload_metadata.mutable_event_time_range()
      ->mutable_end_event_time()
      ->set_day(7);
  EXPECT_OK(federated_protocol_->ReportCompleted(
      std::move(results), plan_duration, std::nullopt, payload_metadata));

  // Validate that the payload can be parsed and that the ciphertext can be
  // decrypted using the decryptor that generated the public encryption key.
  absl::StatusOr<confidential_compute::ClientPayloadHeader> payload_header;
  absl::string_view ciphertext;
  {
    absl::string_view uploaded_data_view(uploaded_data);
    payload_header =
        fcp::confidential_compute::DecodeAndConsumeClientPayloadHeader(
            uploaded_data_view);
    ASSERT_OK(payload_header);
    // The uploaded_data_view now contains just the ciphertext.
    ciphertext = uploaded_data_view;
  }

  // Validate the payload header values.
  EXPECT_TRUE(payload_header->is_gzip_compressed);
  ::fcp::confidentialcompute::BlobHeader blob_header;
  ASSERT_TRUE(
      blob_header.ParseFromString(payload_header->serialized_blob_header));
  EXPECT_EQ(blob_header.access_policy_sha256(),
            ComputeSHA256(serialized_access_policy));
  EXPECT_EQ(blob_header.access_policy_node_id(), 0);
  EXPECT_THAT(blob_header.blob_id(), Not(IsEmpty()));
  EXPECT_EQ(blob_header.key_id(), parsed_public_key->public_key->key_id);
  EXPECT_THAT(blob_header.payload_metadata(), EqualsProto(payload_metadata));

  // Ensure that the ciphertext can be decrypted.
  auto decrypted_uploaded_data =
      decryptor.Decrypt(ciphertext, payload_header->serialized_blob_header,
                        payload_header->encrypted_symmetric_key,
                        payload_header->serialized_blob_header,
                        payload_header->encapsulated_public_key);
  ASSERT_OK(decrypted_uploaded_data);

  // The ciphertext contains compressed data, so we must decompress it before
  // comparing it with the expected checkpoint.
  auto decompressed_uploaded_data =
      UncompressWithGzip(*decrypted_uploaded_data);
  ASSERT_OK(decompressed_uploaded_data);
  EXPECT_EQ(*decompressed_uploaded_data, checkpoint_str);
}
TEST_F(HttpFederatedProtocolTest,
       TestReportCompletedViaConfidentialAggWithSignedEndorsementsSuccess) {
  EXPECT_CALL(mock_flags_, enable_confidential_aggregation)
      .WillRepeatedly(Return(true));
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin(
      /*eligibility_eval_enabled=*/true,
      /*enable_confidential_aggregation*/ true));
  std::string serialized_access_policy = "the access policy";
  confidentialcompute::SignedEndorsements signed_endorsements =
      GetFakeSignedEndorsements();
  std::string serialized_signed_endorsements =
      signed_endorsements.SerializeAsString();
  ASSERT_OK(RunSuccessfulCheckin(
      /*report_eligibility_eval_result*/ true,
      /*confidential_data_access_policy=*/serialized_access_policy,
      /*set_relative_uri=*/false,
      /*signed_endorsements=*/serialized_signed_endorsements));

  // Create a fake checkpoint with 32 'X'.
  std::string checkpoint_str(32, 'X');
  ComputationResults results;
  results.emplace("tensorflow_checkpoint", checkpoint_str);
  absl::Duration plan_duration = absl::Minutes(5);

  // Generate a new public key, which we'll pass to the client in the
  // ConfidentialEncryptionConfig. We'll use the decryptor from which the public
  // key was generated to validate the encrypted payload at the end of the test.
  fcp::confidential_compute::MessageDecryptor decryptor;
  auto encoded_public_key =
      decryptor
          .GetPublicKey(
              [](absl::string_view payload) { return "fakesignature"; }, 0)
          .value();
  absl::StatusOr<OkpCwt> parsed_public_key = OkpCwt::Decode(encoded_public_key);
  ASSERT_OK(parsed_public_key);
  ASSERT_TRUE(parsed_public_key->public_key.has_value());

  // Note: we don't specify any attestation evidence nor attestation
  // endorsements in the encryption config, since we can't generate valid
  // attestations in a test anyway.
  ConfidentialEncryptionConfig encryption_config;
  encryption_config.set_public_key(encoded_public_key);

  // Ensure that the server's attestation evidence is considered valid, and that
  // the signed_endorsements have been correctly plumbed through and parsed.
  EXPECT_CALL(*mock_attestation_verifier_,
              Verify(Eq(serialized_access_policy),
                     EqualsProto(GetFakeSignedEndorsements()),
                     EqualsProto(encryption_config)))
      .WillRepeatedly(
          [=](const absl::Cord& access_policy,
              const confidentialcompute::SignedEndorsements&
                  signed_endorsements,
              const ConfidentialEncryptionConfig& encryption_config) {
            return attestation::AlwaysPassingAttestationVerifier().Verify(
                access_policy, signed_endorsements, encryption_config);
          });

  ExpectSuccessfulReportTaskResultRequest(
      "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
      "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
      kAggregationSessionId, kTaskName, plan_duration);
  ExpectSuccessfulStartAggregationDataUploadRequest(
      "https://aggregation.uri/v1/confidentialaggregations/"
      "AGGREGATION_SESSION_ID/"
      "clients/AUTHORIZATION_TOKEN:startdataupload?%24alt=proto",
      kResourceName, kByteStreamTargetUri, kSecondStageAggregationTargetUri,
      false, encryption_config);

  // Capture the raw uploaded data so we can subsequently validate that it was
  // properly encrypted with the public key that was provided to the client.
  std::string uploaded_data;
  EXPECT_CALL(mock_http_client_, PerformSingleRequest(SimpleHttpRequestMatcher(
                                     "https://bytestream.uri/upload/v1/media/"
                                     "CHECKPOINT_RESOURCE?upload_protocol=raw",
                                     HttpRequest::Method::kPost, _, _)))
      .WillOnce([&uploaded_data](MockHttpClient::SimpleHttpRequest request) {
        uploaded_data = request.body;
        return CreateEmptySuccessHttpResponse();
      });

  ExpectSuccessfulSubmitAggregationResultRequest(
      "https://aggregation.second.uri/v1/confidentialaggregations/"
      "AGGREGATION_SESSION_ID/clients/CLIENT_TOKEN:submit?%24alt=proto",
      /*confidential_aggregation=*/true);

  EXPECT_OK(federated_protocol_->ReportCompleted(
      std::move(results), plan_duration, std::nullopt, payload_metadata_));

  // Validate that the payload can be parsed and that the ciphertext can be
  // decrypted using the decryptor that generated the public encryption key.
  absl::StatusOr<confidential_compute::ClientPayloadHeader> payload_header;
  absl::string_view ciphertext;
  {
    absl::string_view uploaded_data_view(uploaded_data);
    payload_header =
        fcp::confidential_compute::DecodeAndConsumeClientPayloadHeader(
            uploaded_data_view);
    ASSERT_OK(payload_header);
    // The uploaded_data_view now contains just the ciphertext.
    ciphertext = uploaded_data_view;
  }

  // Validate the payload header values.
  EXPECT_TRUE(payload_header->is_gzip_compressed);
  ::fcp::confidentialcompute::BlobHeader blob_header;
  ASSERT_TRUE(
      blob_header.ParseFromString(payload_header->serialized_blob_header));
  EXPECT_EQ(blob_header.access_policy_sha256(),
            ComputeSHA256(serialized_access_policy));
  EXPECT_EQ(blob_header.access_policy_node_id(), 0);
  EXPECT_THAT(blob_header.blob_id(), Not(IsEmpty()));
  EXPECT_EQ(blob_header.key_id(), parsed_public_key->public_key->key_id);

  // Ensure that the ciphertext can be decrypted.
  auto decrypted_uploaded_data =
      decryptor.Decrypt(ciphertext, payload_header->serialized_blob_header,
                        payload_header->encrypted_symmetric_key,
                        payload_header->serialized_blob_header,
                        payload_header->encapsulated_public_key);
  ASSERT_OK(decrypted_uploaded_data);

  // The ciphertext contains compressed data, so we must decompress it before
  // comparing it with the expected checkpoint.
  auto decompressed_uploaded_data =
      UncompressWithGzip(*decrypted_uploaded_data);
  ASSERT_OK(decompressed_uploaded_data);
  EXPECT_EQ(*decompressed_uploaded_data, checkpoint_str);
}

TEST_F(HttpFederatedProtocolTest,
       TestReportCompletedViaConfidentialAggAttestationValidationFailure) {
  EXPECT_CALL(mock_flags_, enable_confidential_aggregation)
      .WillRepeatedly(Return(true));
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin(
      /*eligibility_eval_enabled=*/true,
      /*enable_confidential_aggregation*/ true));
  std::string serialized_access_policy = "the access policy";
  ASSERT_OK(RunSuccessfulCheckin(
      /*report_eligibility_eval_result*/ true,
      /*confidential_data_access_policy=*/serialized_access_policy));

  // Create a fake checkpoint with 32 'X'.
  std::string checkpoint_str(32, 'X');
  ComputationResults results;
  results.emplace("tensorflow_checkpoint", checkpoint_str);
  absl::Duration plan_duration = absl::Minutes(5);

  // We use an empty encryption config since we're just testing the attestation
  // verification failure case, which doesn't need any real values in the
  // config.
  ConfidentialEncryptionConfig encryption_config;
  // Empty SignedEndorsements since the task does not use endorsements.
  confidentialcompute::SignedEndorsements signed_endorsements;

  // Ensure that the server's attestation evidence is considered invalid.
  EXPECT_CALL(
      *mock_attestation_verifier_,
      Verify(Eq(serialized_access_policy), _, EqualsProto(encryption_config)))
      .WillRepeatedly(
          [=](const absl::Cord& access_policy,
              const confidentialcompute::SignedEndorsements&
                  signed_endorsements,
              const ConfidentialEncryptionConfig& encryption_config) {
            return attestation::AlwaysFailingAttestationVerifier().Verify(
                access_policy, signed_endorsements, encryption_config);
          });

  ExpectSuccessfulReportTaskResultRequest(
      "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
      "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
      kAggregationSessionId, kTaskName, plan_duration);
  ExpectSuccessfulStartAggregationDataUploadRequest(
      "https://aggregation.uri/v1/confidentialaggregations/"
      "AGGREGATION_SESSION_ID/"
      "clients/AUTHORIZATION_TOKEN:startdataupload?%24alt=proto",
      kResourceName, kByteStreamTargetUri, kSecondStageAggregationTargetUri,
      false, encryption_config);

  // Attestation validation will fail after the
  // StartConfidentialAggregationResponse, leading to the session being aborted
  // and an error being returned by `ReportCompleted`.
  //
  // No further protocol requests are expected after this one.
  ExpectSuccessfulAbortAggregationRequest("https://aggregation.second.uri",
                                          /*confidential_aggregation=*/true);

  absl::Status report_result = federated_protocol_->ReportCompleted(
      std::move(results), plan_duration, std::nullopt, payload_metadata_);
  EXPECT_THAT(report_result, IsCode(FAILED_PRECONDITION));
  EXPECT_THAT(report_result.message(),
              HasSubstr("attestation verification failed"));
}

TEST_F(HttpFederatedProtocolTest,
       TestReportCompletedViaConfidentialAggWithBlobHeaderInHttpHeaderSuccess) {
  EXPECT_CALL(mock_flags_, enable_confidential_aggregation)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_flags_, enable_blob_header_in_http_headers)
      .WillRepeatedly(Return(true));
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin(
      /*eligibility_eval_enabled=*/true,
      /*enable_confidential_aggregation*/ true));
  std::string serialized_access_policy = "the access policy";
  ASSERT_OK(RunSuccessfulCheckin(
      /*report_eligibility_eval_result*/ true,
      /*confidential_data_access_policy=*/serialized_access_policy));

  // Create a fake checkpoint with 32 'X'.
  std::string checkpoint_str(32, 'X');
  ComputationResults results;
  results.emplace("tensorflow_checkpoint", checkpoint_str);
  absl::Duration plan_duration = absl::Minutes(5);

  // Generate a new public key, which we'll pass to the client in the
  // ConfidentialEncryptionConfig. We'll use the decryptor from which the public
  // key was generated to validate the encrypted payload at the end of the test.
  fcp::confidential_compute::MessageDecryptor decryptor;
  auto encoded_public_key =
      decryptor
          .GetPublicKey(
              [](absl::string_view payload) { return "fakesignature"; }, 0)
          .value();
  absl::StatusOr<OkpCwt> parsed_public_key = OkpCwt::Decode(encoded_public_key);
  ASSERT_OK(parsed_public_key);
  ASSERT_TRUE(parsed_public_key->public_key.has_value());

  // Note: we don't specify any attestation evidence nor attestation
  // endorsements in the encryption config, since we can't generate valid
  // attestations in a test anyway.
  ConfidentialEncryptionConfig encryption_config;
  encryption_config.set_public_key(encoded_public_key);
  // Empty SignedEndorsements since the task does not use endorsements.
  confidentialcompute::SignedEndorsements signed_endorsements;

  // Ensure that the server's attestation evidence is considered valid.
  EXPECT_CALL(
      *mock_attestation_verifier_,
      Verify(Eq(serialized_access_policy), _, EqualsProto(encryption_config)))
      .WillRepeatedly(
          [=](const absl::Cord& access_policy,
              const confidentialcompute::SignedEndorsements&
                  signed_endorsements,
              const ConfidentialEncryptionConfig& encryption_config) {
            return attestation::AlwaysPassingAttestationVerifier().Verify(
                access_policy, signed_endorsements, encryption_config);
          });

  ExpectSuccessfulReportTaskResultRequest(
      "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
      "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
      kAggregationSessionId, kTaskName, plan_duration);
  ExpectSuccessfulStartAggregationDataUploadRequest(
      "https://aggregation.uri/v1/confidentialaggregations/"
      "AGGREGATION_SESSION_ID/"
      "clients/AUTHORIZATION_TOKEN:startdataupload?%24alt=proto",
      kResourceName, kByteStreamTargetUri, kSecondStageAggregationTargetUri,
      false, encryption_config);

  // Capture the raw uploaded data so we can subsequently validate that it was
  // properly encrypted with the public key that was provided to the client.
  std::string uploaded_data;
  HeaderList headers;
  EXPECT_CALL(mock_http_client_, PerformSingleRequest(SimpleHttpRequestMatcher(
                                     "https://bytestream.uri/upload/v1/media/"
                                     "CHECKPOINT_RESOURCE?upload_protocol=raw",
                                     HttpRequest::Method::kPost, _, _)))
      .WillOnce([&uploaded_data,
                 &headers](MockHttpClient::SimpleHttpRequest request) {
        uploaded_data = request.body;
        headers = request.headers;
        return CreateEmptySuccessHttpResponse();
      });

  ExpectSuccessfulSubmitAggregationResultRequest(
      "https://aggregation.second.uri/v1/confidentialaggregations/"
      "AGGREGATION_SESSION_ID/clients/CLIENT_TOKEN:submit?%24alt=proto",
      /*confidential_aggregation=*/true);

  confidentialcompute::PayloadMetadata payload_metadata;
  payload_metadata.mutable_event_time_range()
      ->mutable_start_event_time()
      ->set_year(2025);
  payload_metadata.mutable_event_time_range()
      ->mutable_start_event_time()
      ->set_month(1);
  payload_metadata.mutable_event_time_range()
      ->mutable_start_event_time()
      ->set_day(1);
  payload_metadata.mutable_event_time_range()
      ->mutable_end_event_time()
      ->set_year(2025);
  payload_metadata.mutable_event_time_range()
      ->mutable_end_event_time()
      ->set_month(1);
  payload_metadata.mutable_event_time_range()
      ->mutable_end_event_time()
      ->set_day(7);
  EXPECT_OK(federated_protocol_->ReportCompleted(
      std::move(results), plan_duration, std::nullopt, payload_metadata));

  // Validate that the payload can be parsed and that the ciphertext can be
  // decrypted using the decryptor that generated the public encryption key.
  absl::StatusOr<confidential_compute::ClientPayloadHeader> payload_header;
  absl::string_view ciphertext;
  {
    absl::string_view uploaded_data_view(uploaded_data);
    payload_header =
        fcp::confidential_compute::DecodeAndConsumeClientPayloadHeader(
            uploaded_data_view);
    ASSERT_OK(payload_header);
    // The uploaded_data_view now contains just the ciphertext.
    ciphertext = uploaded_data_view;
    EXPECT_THAT(payload_header->serialized_blob_header, IsEmpty());
  }

  // Validate the blob header values.
  bool found_blob_header = false;
  for (const auto& header : headers) {
    if (header.first == "x-goog-blob-header") {
      found_blob_header = true;
      std::string unescaped;
      ASSERT_TRUE(absl::WebSafeBase64Unescape(header.second, &unescaped));
      ::fcp::confidentialcompute::BlobHeader blob_header;
      ASSERT_TRUE(blob_header.ParseFromString(unescaped));
      EXPECT_EQ(blob_header.access_policy_sha256(),
                ComputeSHA256(serialized_access_policy));
      EXPECT_EQ(blob_header.access_policy_node_id(), 0);
      EXPECT_THAT(blob_header.blob_id(), Not(IsEmpty()));
      EXPECT_EQ(blob_header.key_id(), parsed_public_key->public_key->key_id);
      EXPECT_THAT(blob_header.payload_metadata(),
                  EqualsProto(payload_metadata));
    }
  }
  ASSERT_TRUE(found_blob_header);
}

// TODO: b/307312707 -  Add a test for confidential aggregation with multiple
// task assignment.

TEST_F(HttpFederatedProtocolTest, TestReportCompletedViaSecureAgg) {
  absl::Duration plan_duration = absl::Minutes(5);
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  // Issue a regular checkin
  ASSERT_OK(RunSuccessfulCheckin());

  StartSecureAggregationResponse start_secure_aggregation_response;
  start_secure_aggregation_response.set_client_token(kClientToken);
  auto masked_result_resource =
      start_secure_aggregation_response.mutable_masked_result_resource();
  masked_result_resource->set_resource_name("masked_resource");
  masked_result_resource->mutable_data_upload_forwarding_info()
      ->set_target_uri_prefix("https://bytestream.uri/");

  auto nonmasked_result_resource =
      start_secure_aggregation_response.mutable_nonmasked_result_resource();
  nonmasked_result_resource->set_resource_name("nonmasked_resource");
  nonmasked_result_resource->mutable_data_upload_forwarding_info()
      ->set_target_uri_prefix("https://bytestream.uri/");

  start_secure_aggregation_response.mutable_secagg_protocol_forwarding_info()
      ->set_target_uri_prefix("https://secure.aggregations.uri/");
  auto protocol_execution_info =
      start_secure_aggregation_response.mutable_protocol_execution_info();
  protocol_execution_info->set_minimum_surviving_clients_for_reconstruction(
      450);
  protocol_execution_info->set_expected_number_of_clients(500);

  auto secure_aggregands =
      start_secure_aggregation_response.mutable_secure_aggregands();
  SecureAggregandExecutionInfo secure_aggregand_execution_info;
  secure_aggregand_execution_info.set_modulus(9999);
  (*secure_aggregands)["secagg_tensor"] = secure_aggregand_execution_info;

  ExpectSuccessfulReportTaskResultRequest(
      "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
      "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
      kAggregationSessionId, kTaskName, plan_duration);
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://aggregation.uri/v1/secureaggregations/"
                  "AGGREGATION_SESSION_ID/clients/"
                  "AUTHORIZATION_TOKEN:start?%24alt=proto",
                  HttpRequest::Method::kPost, _,
                  StartSecureAggregationRequest().SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreatePendingOperation("operations/foo#bar").SerializeAsString())));
  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://aggregation.uri/v1/operations/foo%23bar?%24alt=proto",
          HttpRequest::Method::kGet, _, "")))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreateDoneOperation(kOperationName, start_secure_aggregation_response)
              .SerializeAsString())));

  // Create a fake checkpoint with 32 'X'.
  std::string checkpoint_str(32, 'X');
  ComputationResults results;
  results.emplace("tensorflow_checkpoint", checkpoint_str);
  results.emplace("secagg_tensor", QuantizedTensor());

  EXPECT_CALL(*mock_secagg_runner_factory_,
              CreateSecAggRunner(_, _, _, _, _, 500, 450))
      .WillOnce(WithArg<0>([&](auto send_to_server_impl) {
        auto mock_secagg_runner =
            std::make_unique<StrictMock<MockSecAggRunner>>();
        EXPECT_CALL(*mock_secagg_runner,
                    Run(UnorderedElementsAre(Pair(
                        "secagg_tensor", VariantWith<QuantizedTensor>(FieldsAre(
                                             IsEmpty(), 0, IsEmpty()))))))
            .WillOnce([=,
                       send_to_server_impl = std::move(send_to_server_impl)] {
              // SecAggSendToServerBase::Send should use the client token. This
              // needs to be tested here since `send_to_server_impl` should not
              // be used outside of Run.
              EXPECT_CALL(
                  mock_http_client_,
                  PerformSingleRequest(SimpleHttpRequestMatcher(
                      "https://secure.aggregations.uri/v1/secureaggregations/"
                      "AGGREGATION_SESSION_ID/clients/"
                      "CLIENT_TOKEN:abort?%24alt=proto",
                      _, _, _)))
                  .WillOnce(Return(CreateEmptySuccessHttpResponse()));
              secagg::ClientToServerWrapperMessage abort_message;
              abort_message.mutable_abort();
              send_to_server_impl->Send(&abort_message);

              return absl::OkStatus();
            });
        return mock_secagg_runner;
      }));

  EXPECT_OK(federated_protocol_->ReportCompleted(
      std::move(results), plan_duration, std::nullopt, payload_metadata_));
}

// TODO(team): Remove this test once client_token is always populated in
// StartSecureAggregationResponse.
TEST_F(HttpFederatedProtocolTest,
       TestReportCompletedViaSecureAggWithoutClientToken) {
  absl::Duration plan_duration = absl::Minutes(5);
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  // Issue a regular checkin
  ASSERT_OK(RunSuccessfulCheckin());

  StartSecureAggregationResponse start_secure_aggregation_response;
  // Don't set client_token.
  auto masked_result_resource =
      start_secure_aggregation_response.mutable_masked_result_resource();
  masked_result_resource->set_resource_name("masked_resource");
  masked_result_resource->mutable_data_upload_forwarding_info()
      ->set_target_uri_prefix("https://bytestream.uri/");

  auto nonmasked_result_resource =
      start_secure_aggregation_response.mutable_nonmasked_result_resource();
  nonmasked_result_resource->set_resource_name("nonmasked_resource");
  nonmasked_result_resource->mutable_data_upload_forwarding_info()
      ->set_target_uri_prefix("https://bytestream.uri/");

  start_secure_aggregation_response.mutable_secagg_protocol_forwarding_info()
      ->set_target_uri_prefix("https://secure.aggregations.uri/");
  auto protocol_execution_info =
      start_secure_aggregation_response.mutable_protocol_execution_info();
  protocol_execution_info->set_minimum_surviving_clients_for_reconstruction(
      450);
  protocol_execution_info->set_expected_number_of_clients(500);

  auto secure_aggregands =
      start_secure_aggregation_response.mutable_secure_aggregands();
  SecureAggregandExecutionInfo secure_aggregand_execution_info;
  secure_aggregand_execution_info.set_modulus(9999);
  (*secure_aggregands)["secagg_tensor"] = secure_aggregand_execution_info;

  ExpectSuccessfulReportTaskResultRequest(
      "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
      "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
      kAggregationSessionId, kTaskName, plan_duration);
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://aggregation.uri/v1/secureaggregations/"
                  "AGGREGATION_SESSION_ID/clients/"
                  "AUTHORIZATION_TOKEN:start?%24alt=proto",
                  HttpRequest::Method::kPost, _,
                  StartSecureAggregationRequest().SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreateDoneOperation(kOperationName, start_secure_aggregation_response)
              .SerializeAsString())));

  // Create a fake checkpoint with 32 'X'.
  std::string checkpoint_str(32, 'X');
  ComputationResults results;
  results.emplace("tensorflow_checkpoint", checkpoint_str);
  results.emplace("secagg_tensor", QuantizedTensor());

  EXPECT_CALL(*mock_secagg_runner_factory_,
              CreateSecAggRunner(_, _, _, _, _, _, _))
      .WillOnce(WithArg<0>([&](auto send_to_server_impl) {
        auto mock_secagg_runner =
            std::make_unique<StrictMock<MockSecAggRunner>>();
        EXPECT_CALL(*mock_secagg_runner, Run(_))
            .WillOnce([=,
                       send_to_server_impl = std::move(send_to_server_impl)] {
              // SecAggSendToServerBase::Send should reuse the authorization
              // token. This needs to be tested here since `send_to_server_impl`
              // should not be used outside of Run.
              EXPECT_CALL(
                  mock_http_client_,
                  PerformSingleRequest(SimpleHttpRequestMatcher(
                      "https://secure.aggregations.uri/v1/secureaggregations/"
                      "AGGREGATION_SESSION_ID/clients/"
                      "AUTHORIZATION_TOKEN:abort?%24alt=proto",
                      _, _, _)))
                  .WillOnce(Return(CreateEmptySuccessHttpResponse()));
              secagg::ClientToServerWrapperMessage abort_message;
              abort_message.mutable_abort();
              send_to_server_impl->Send(&abort_message);

              return absl::OkStatus();
            });
        return mock_secagg_runner;
      }));

  EXPECT_OK(federated_protocol_->ReportCompleted(
      std::move(results), plan_duration, std::nullopt, payload_metadata_));
}

TEST_F(HttpFederatedProtocolTest,
       TestReportCompletedViaSecureAggReportTaskResultFailed) {
  absl::Duration plan_duration = absl::Minutes(5);
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  // Issue a regular checkin
  ASSERT_OK(RunSuccessfulCheckin());

  StartSecureAggregationResponse start_secure_aggregation_response;
  start_secure_aggregation_response.set_client_token(kClientToken);
  auto masked_result_resource =
      start_secure_aggregation_response.mutable_masked_result_resource();
  masked_result_resource->set_resource_name("masked_resource");
  masked_result_resource->mutable_data_upload_forwarding_info()
      ->set_target_uri_prefix("https://bytestream.uri/");

  auto nonmasked_result_resource =
      start_secure_aggregation_response.mutable_nonmasked_result_resource();
  nonmasked_result_resource->set_resource_name("nonmasked_resource");
  nonmasked_result_resource->mutable_data_upload_forwarding_info()
      ->set_target_uri_prefix("https://bytestream.uri/");

  start_secure_aggregation_response.mutable_secagg_protocol_forwarding_info()
      ->set_target_uri_prefix("https://secure.aggregations.uri/");
  auto protocol_execution_info =
      start_secure_aggregation_response.mutable_protocol_execution_info();
  protocol_execution_info->set_minimum_surviving_clients_for_reconstruction(
      450);
  protocol_execution_info->set_expected_number_of_clients(500);

  auto secure_aggregands =
      start_secure_aggregation_response.mutable_secure_aggregands();
  SecureAggregandExecutionInfo secure_aggregand_execution_info;
  secure_aggregand_execution_info.set_modulus(9999);
  (*secure_aggregands)["secagg_tensor"] = secure_aggregand_execution_info;

  // Mock a failed ReportTaskResult request.
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
                  "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
                  HttpRequest::Method::kPost, _,
                  ReportTaskResultRequestMatcher(
                      EqualsProto(GetExpectedReportTaskResultRequest(
                          kAggregationSessionId, kTaskName,
                          google::rpc::Code::OK, plan_duration))))))
      .WillOnce(Return(FakeHttpResponse(503, HeaderList())));
  EXPECT_CALL(mock_log_manager_,
              LogDiag(ProdDiagCode::HTTP_REPORT_TASK_RESULT_REQUEST_FAILED));
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://aggregation.uri/v1/secureaggregations/"
                  "AGGREGATION_SESSION_ID/clients/"
                  "AUTHORIZATION_TOKEN:start?%24alt=proto",
                  HttpRequest::Method::kPost, _,
                  StartSecureAggregationRequest().SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreatePendingOperation("operations/foo#bar").SerializeAsString())));
  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://aggregation.uri/v1/operations/foo%23bar?%24alt=proto",
          HttpRequest::Method::kGet, _, "")))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreateDoneOperation(kOperationName, start_secure_aggregation_response)
              .SerializeAsString())));

  // Create a fake checkpoint with 32 'X'.
  std::string checkpoint_str(32, 'X');
  ComputationResults results;
  results.emplace("tensorflow_checkpoint", checkpoint_str);
  results.emplace("secagg_tensor", QuantizedTensor());

  MockSecAggRunner* mock_secagg_runner = new StrictMock<MockSecAggRunner>();
  EXPECT_CALL(*mock_secagg_runner_factory_,
              CreateSecAggRunner(_, _, _, _, _, 500, 450))
      .WillOnce(Return(ByMove(absl::WrapUnique(mock_secagg_runner))));
  EXPECT_CALL(*mock_secagg_runner,
              Run(UnorderedElementsAre(
                  Pair("secagg_tensor", VariantWith<QuantizedTensor>(FieldsAre(
                                            IsEmpty(), 0, IsEmpty()))))))
      .WillOnce(Return(absl::OkStatus()));

  EXPECT_OK(federated_protocol_->ReportCompleted(
      std::move(results), plan_duration, std::nullopt, payload_metadata_));
}

TEST_F(HttpFederatedProtocolTest, TestReportCompletedStartSecAggFailed) {
  absl::Duration plan_duration = absl::Minutes(5);
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  // Issue a regular checkin.
  ASSERT_OK(RunSuccessfulCheckin());
  ExpectSuccessfulReportTaskResultRequest(
      "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
      "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
      kAggregationSessionId, kTaskName, plan_duration);
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://aggregation.uri/v1/secureaggregations/"
                  "AGGREGATION_SESSION_ID/clients/"
                  "AUTHORIZATION_TOKEN:start?%24alt=proto",
                  HttpRequest::Method::kPost, _,
                  StartSecureAggregationRequest().SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreateErrorOperation(kOperationName, absl::StatusCode::kInternal,
                               "Request failed.")
              .SerializeAsString())));

  // Create a fake checkpoint with 32 'X'.
  std::string checkpoint_str(32, 'X');
  ComputationResults results;
  results.emplace("tensorflow_checkpoint", checkpoint_str);
  results.emplace("secagg_tensor", QuantizedTensor());

  EXPECT_THAT(
      federated_protocol_->ReportCompleted(std::move(results), plan_duration,
                                           std::nullopt, payload_metadata_),
      IsCode(absl::StatusCode::kInternal));
}

TEST_F(HttpFederatedProtocolTest,
       TestReportCompletedStartSecAggFailedImmediately) {
  absl::Duration plan_duration = absl::Minutes(5);
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  // Issue a regular checkin.
  ASSERT_OK(RunSuccessfulCheckin());
  ExpectSuccessfulReportTaskResultRequest(
      "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
      "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
      kAggregationSessionId, kTaskName, plan_duration);
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://aggregation.uri/v1/secureaggregations/"
                  "AGGREGATION_SESSION_ID/clients/"
                  "AUTHORIZATION_TOKEN:start?%24alt=proto",
                  HttpRequest::Method::kPost, _,
                  StartSecureAggregationRequest().SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(403, HeaderList(), "")));

  // Create a fake checkpoint with 32 'X'.
  std::string checkpoint_str(32, 'X');
  ComputationResults results;
  results.emplace("tensorflow_checkpoint", checkpoint_str);
  results.emplace("secagg_tensor", QuantizedTensor());

  EXPECT_THAT(
      federated_protocol_->ReportCompleted(std::move(results), plan_duration,
                                           std::nullopt, payload_metadata_),
      IsCode(absl::StatusCode::kPermissionDenied));
}

TEST_F(HttpFederatedProtocolTest, TestReportCompletedReportTaskResultFailed) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  // Issue a regular checkin.
  ASSERT_OK(RunSuccessfulCheckin());

  // Create a fake checkpoint with 32 'X'.
  std::string checkpoint_str(32, 'X');
  ComputationResults results;
  results.emplace("tensorflow_checkpoint", checkpoint_str);
  absl::Duration plan_duration = absl::Minutes(5);

  // Mock a failed ReportTaskResult request.
  ReportTaskResultResponse report_task_result_response;
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
                  "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
                  HttpRequest::Method::kPost, _,
                  ReportTaskResultRequestMatcher(
                      EqualsProto(GetExpectedReportTaskResultRequest(
                          kAggregationSessionId, kTaskName,
                          google::rpc::Code::OK, plan_duration))))))
      .WillOnce(Return(FakeHttpResponse(503, HeaderList())));
  EXPECT_CALL(mock_log_manager_,
              LogDiag(ProdDiagCode::HTTP_REPORT_TASK_RESULT_REQUEST_FAILED));

  ExpectSuccessfulStartAggregationDataUploadRequest(
      "https://aggregation.uri/v1/aggregations/AGGREGATION_SESSION_ID/"
      "clients/AUTHORIZATION_TOKEN:startdataupload?%24alt=proto",
      kResourceName, kByteStreamTargetUri, kSecondStageAggregationTargetUri);
  ExpectSuccessfulByteStreamUploadRequest(
      "https://bytestream.uri/upload/v1/media/"
      "CHECKPOINT_RESOURCE?upload_protocol=raw",
      checkpoint_str);
  ExpectSuccessfulSubmitAggregationResultRequest(
      "https://aggregation.second.uri/v1/aggregations/"
      "AGGREGATION_SESSION_ID/clients/CLIENT_TOKEN:submit?%24alt=proto");

  // Despite the ReportTaskResult request failed, we still consider the overall
  // ReportCompleted succeeded because the rest of the steps succeeds, and the
  // ReportTaskResult is a just a metric reporting on a best effort basis.
  EXPECT_OK(federated_protocol_->ReportCompleted(
      std::move(results), plan_duration, std::nullopt, payload_metadata_));
}

TEST_F(HttpFederatedProtocolTest,
       TestReportCompletedStartAggregationFailedImmediately) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  // Issue a regular checkin.
  ASSERT_OK(RunSuccessfulCheckin());

  std::string checkpoint_str;
  const size_t kTFCheckpointSize = 32;
  checkpoint_str.resize(kTFCheckpointSize, 'X');
  ComputationResults results;
  results.emplace("tensorflow_checkpoint", checkpoint_str);
  absl::Duration plan_duration = absl::Minutes(5);

  ExpectSuccessfulReportTaskResultRequest(
      "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
      "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
      kAggregationSessionId, kTaskName, plan_duration);
  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://aggregation.uri/v1/aggregations/AGGREGATION_SESSION_ID/"
          "clients/AUTHORIZATION_TOKEN:startdataupload?%24alt=proto",
          HttpRequest::Method::kPost, _,
          StartAggregationDataUploadRequest().SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(503, HeaderList())));
  absl::Status report_result = federated_protocol_->ReportCompleted(
      std::move(results), plan_duration, std::nullopt, payload_metadata_);
  ASSERT_THAT(report_result, IsCode(absl::StatusCode::kUnavailable));
  EXPECT_THAT(report_result.message(),
              HasSubstr("StartAggregationDataUpload request failed"));
  EXPECT_THAT(report_result.message(), HasSubstr("503"));
}

TEST_F(HttpFederatedProtocolTest,
       TestReportCompletedStartAggregationFailedDuringPolling) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  // Issue a regular checkin.
  ASSERT_OK(RunSuccessfulCheckin());

  std::string checkpoint_str;
  const size_t kTFCheckpointSize = 32;
  checkpoint_str.resize(kTFCheckpointSize, 'X');
  ComputationResults results;
  results.emplace("tensorflow_checkpoint", checkpoint_str);
  absl::Duration plan_duration = absl::Minutes(5);

  ExpectSuccessfulReportTaskResultRequest(
      "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
      "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
      kAggregationSessionId, kTaskName, plan_duration);
  Operation pending_operation_response =
      CreatePendingOperation("operations/foo#bar");
  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://aggregation.uri/v1/aggregations/AGGREGATION_SESSION_ID/"
          "clients/AUTHORIZATION_TOKEN:startdataupload?%24alt=proto",
          HttpRequest::Method::kPost, _,
          StartAggregationDataUploadRequest().SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), pending_operation_response.SerializeAsString())));
  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          // Note that the '#' character is encoded as "%23".
          "https://aggregation.uri/v1/operations/foo%23bar?%24alt=proto",
          HttpRequest::Method::kGet, _,
          GetOperationRequestMatcher(EqualsProto(GetOperationRequest())))))
      .WillOnce(Return(FakeHttpResponse(401, HeaderList())));
  absl::Status report_result = federated_protocol_->ReportCompleted(
      std::move(results), plan_duration, std::nullopt, payload_metadata_);
  ASSERT_THAT(report_result, IsCode(absl::StatusCode::kUnauthenticated));
  EXPECT_THAT(report_result.message(),
              HasSubstr("StartAggregationDataUpload request failed"));
  EXPECT_THAT(report_result.message(), HasSubstr("401"));
}

TEST_F(HttpFederatedProtocolTest, TestReportCompletedUploadFailed) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  // Issue a regular checkin.
  ASSERT_OK(RunSuccessfulCheckin());

  std::string checkpoint_str;
  const size_t kTFCheckpointSize = 32;
  checkpoint_str.resize(kTFCheckpointSize, 'X');
  ComputationResults results;
  results.emplace("tensorflow_checkpoint", checkpoint_str);
  absl::Duration plan_duration = absl::Minutes(5);

  ExpectSuccessfulReportTaskResultRequest(
      "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
      "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
      kAggregationSessionId, kTaskName, plan_duration);
  ExpectSuccessfulStartAggregationDataUploadRequest(
      "https://aggregation.uri/v1/aggregations/AGGREGATION_SESSION_ID/"
      "clients/AUTHORIZATION_TOKEN:startdataupload?%24alt=proto",
      kResourceName, kByteStreamTargetUri, kSecondStageAggregationTargetUri);
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  StrEq("https://bytestream.uri/upload/v1/media/"
                        "CHECKPOINT_RESOURCE?upload_protocol=raw"),
                  HttpRequest::Method::kPost, _, std::string(checkpoint_str))))
      .WillOnce(Return(FakeHttpResponse(501, HeaderList())));
  ExpectSuccessfulAbortAggregationRequest("https://aggregation.second.uri");
  absl::Status report_result = federated_protocol_->ReportCompleted(
      std::move(results), plan_duration, std::nullopt, payload_metadata_);
  ASSERT_THAT(report_result, IsCode(absl::StatusCode::kUnimplemented));
  EXPECT_THAT(report_result.message(), HasSubstr("Data upload failed"));
  EXPECT_THAT(report_result.message(), HasSubstr("501"));
}

TEST_F(HttpFederatedProtocolTest, TestReportCompletedUploadAbortedByServer) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  // Issue a regular checkin.
  ASSERT_OK(RunSuccessfulCheckin());

  std::string checkpoint_str;
  const size_t kTFCheckpointSize = 32;
  checkpoint_str.resize(kTFCheckpointSize, 'X');
  ComputationResults results;
  results.emplace("tensorflow_checkpoint", checkpoint_str);
  absl::Duration plan_duration = absl::Minutes(5);

  ExpectSuccessfulReportTaskResultRequest(
      "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
      "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
      kAggregationSessionId, kTaskName, plan_duration);
  ExpectSuccessfulStartAggregationDataUploadRequest(
      "https://aggregation.uri/v1/aggregations/AGGREGATION_SESSION_ID/"
      "clients/AUTHORIZATION_TOKEN:startdataupload?%24alt=proto",
      kResourceName, kByteStreamTargetUri, kSecondStageAggregationTargetUri);
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  StrEq("https://bytestream.uri/upload/v1/media/"
                        "CHECKPOINT_RESOURCE?upload_protocol=raw"),
                  HttpRequest::Method::kPost, _, std::string(checkpoint_str))))
      .WillOnce(Return(FakeHttpResponse(
          409, HeaderList(),
          CreateErrorOperation(kOperationName, absl::StatusCode::kAborted,
                               "The client update is no longer needed.")
              .SerializeAsString())));
  absl::Status report_result = federated_protocol_->ReportCompleted(
      std::move(results), plan_duration, std::nullopt, payload_metadata_);
  ASSERT_THAT(report_result, IsCode(absl::StatusCode::kAborted));
  EXPECT_THAT(report_result.message(), HasSubstr("Data upload failed"));
  EXPECT_THAT(report_result.message(), HasSubstr("409"));
}

TEST_F(HttpFederatedProtocolTest, TestReportCompletedUploadInterrupted) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  // Issue a regular checkin.
  ASSERT_OK(RunSuccessfulCheckin());

  std::string checkpoint_str;
  const size_t kTFCheckpointSize = 32;
  checkpoint_str.resize(kTFCheckpointSize, 'X');
  ComputationResults results;
  results.emplace("tensorflow_checkpoint", checkpoint_str);
  absl::Duration plan_duration = absl::Minutes(5);

  ExpectSuccessfulReportTaskResultRequest(
      "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
      "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
      kAggregationSessionId, kTaskName, plan_duration);
  ExpectSuccessfulStartAggregationDataUploadRequest(
      "https://aggregation.uri/v1/aggregations/AGGREGATION_SESSION_ID/"
      "clients/AUTHORIZATION_TOKEN:startdataupload?%24alt=proto",
      kResourceName, kByteStreamTargetUri, kSecondStageAggregationTargetUri);
  absl::Notification request_issued;
  absl::Notification request_cancelled;

  // Make HttpClient::PerformRequests() block until the counter is decremented.
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  StrEq("https://bytestream.uri/upload/v1/media/"
                        "CHECKPOINT_RESOURCE?upload_protocol=raw"),
                  HttpRequest::Method::kPost, _, std::string(checkpoint_str))))
      .WillOnce([&request_issued, &request_cancelled](
                    MockableHttpClient::SimpleHttpRequest ignored) {
        request_issued.Notify();
        request_cancelled.WaitForNotification();
        return FakeHttpResponse(503, HeaderList(), "");
      });
  // Make should_abort return false until we know that the request was issued
  // (i.e. once InterruptibleRunner has actually started running the code it
  // was given), and then make it return true, triggering an abort sequence and
  // unblocking the PerformRequests()() call we caused to block above.
  EXPECT_CALL(mock_should_abort_, Call()).WillRepeatedly([&request_issued] {
    return request_issued.HasBeenNotified();
  });

  // When the HttpClient receives a HttpRequestHandle::Cancel call, we let the
  // request complete.
  mock_http_client_.SetCancellationListener([&request_cancelled]() {
    if (!request_cancelled.HasBeenNotified()) {
      request_cancelled.Notify();
    }
  });

  EXPECT_CALL(mock_log_manager_,
              LogDiag(ProdDiagCode::BACKGROUND_TRAINING_INTERRUPT_HTTP));
  ExpectSuccessfulAbortAggregationRequest("https://aggregation.second.uri");
  absl::Status report_result = federated_protocol_->ReportCompleted(
      std::move(results), plan_duration, std::nullopt, payload_metadata_);
  ASSERT_THAT(report_result, IsCode(absl::StatusCode::kCancelled));
  EXPECT_THAT(report_result.message(), HasSubstr("Data upload failed"));
}

TEST_F(HttpFederatedProtocolTest,
       TestReportCompletedSubmitAggregationResultFailed) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  // Issue a regular checkin.
  ASSERT_OK(RunSuccessfulCheckin());

  std::string checkpoint_str;
  const size_t kTFCheckpointSize = 32;
  checkpoint_str.resize(kTFCheckpointSize, 'X');
  ComputationResults results;
  results.emplace("tensorflow_checkpoint", checkpoint_str);
  absl::Duration plan_duration = absl::Minutes(5);

  ExpectSuccessfulReportTaskResultRequest(
      "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
      "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
      kAggregationSessionId, kTaskName, plan_duration);
  ExpectSuccessfulStartAggregationDataUploadRequest(
      "https://aggregation.uri/v1/aggregations/AGGREGATION_SESSION_ID/"
      "clients/AUTHORIZATION_TOKEN:startdataupload?%24alt=proto",
      kResourceName, kByteStreamTargetUri, kSecondStageAggregationTargetUri);
  ExpectSuccessfulByteStreamUploadRequest(
      "https://bytestream.uri/upload/v1/media/"
      "CHECKPOINT_RESOURCE?upload_protocol=raw",
      checkpoint_str);

  SubmitAggregationResultRequest submit_aggregation_result_request;
  submit_aggregation_result_request.set_resource_name(kResourceName);
  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          "https://aggregation.second.uri/v1/aggregations/"
          "AGGREGATION_SESSION_ID/clients/CLIENT_TOKEN:submit?%24alt=proto",
          HttpRequest::Method::kPost, _,
          submit_aggregation_result_request.SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(409, HeaderList())));
  absl::Status report_result = federated_protocol_->ReportCompleted(
      std::move(results), plan_duration, std::nullopt, payload_metadata_);

  ASSERT_THAT(report_result, IsCode(absl::StatusCode::kAborted));
  EXPECT_THAT(report_result.message(),
              HasSubstr("SubmitAggregationResult failed"));
  EXPECT_THAT(report_result.message(), HasSubstr("409"));
}

TEST_F(HttpFederatedProtocolTest, TestReportNotCompletedSuccess) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  // Issue a regular checkin.
  ASSERT_OK(RunSuccessfulCheckin());
  absl::Duration plan_duration = absl::Minutes(5);
  ReportTaskResultResponse response;
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
                  "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
                  HttpRequest::Method::kPost, _,
                  ReportTaskResultRequestMatcher(
                      EqualsProto(GetExpectedReportTaskResultRequest(
                          kAggregationSessionId, kTaskName,
                          ::google::rpc::Code::INTERNAL, plan_duration))))))
      .WillOnce(Return(
          FakeHttpResponse(200, HeaderList(), response.SerializeAsString())));

  ASSERT_OK(federated_protocol_->ReportNotCompleted(
      engine::PhaseOutcome::ERROR, plan_duration, std::nullopt));

  ExpectAcceptedRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

TEST_F(HttpFederatedProtocolTest, TestReportNotCompletedError) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  // Issue a regular checkin.
  ASSERT_OK(RunSuccessfulCheckin());
  ReportTaskResultResponse response;
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
                  "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
                  HttpRequest::Method::kPost, _, _)))
      .WillOnce(Return(FakeHttpResponse(503, HeaderList())));

  absl::Status status = federated_protocol_->ReportNotCompleted(
      engine::PhaseOutcome::ERROR, absl::Minutes(5), std::nullopt);
  EXPECT_THAT(status, IsCode(UNAVAILABLE));
  EXPECT_THAT(
      status.message(),
      AllOf(HasSubstr("ReportTaskResult request failed:"), HasSubstr("503")));
  ExpectAcceptedRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

TEST_F(HttpFederatedProtocolTest, TestReportNotCompletedPermanentError) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin());
  // Issue a regular checkin.
  ASSERT_OK(RunSuccessfulCheckin());
  ReportTaskResultResponse response;
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
                  "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
                  HttpRequest::Method::kPost, _, _)))
      .WillOnce(Return(FakeHttpResponse(404, HeaderList())));

  absl::Status status = federated_protocol_->ReportNotCompleted(
      engine::PhaseOutcome::ERROR, absl::Minutes(5), std::nullopt);
  EXPECT_THAT(status, IsCode(NOT_FOUND));
  EXPECT_THAT(
      status.message(),
      AllOf(HasSubstr("ReportTaskResult request failed:"), HasSubstr("404")));
  ExpectAcceptedRetryWindow(federated_protocol_->GetLatestRetryWindow());
}

TEST_F(HttpFederatedProtocolTest, TestFullProtocol) {
  // Issue an eligibility eval checkin first.
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin(
      /*eligibility_eval_enabled=*/true));
  // Issue a regular checkin
  ASSERT_OK(RunSuccessfulMultipleTaskAssignments());

  // Upload the result from the first task.
  std::string checkpoint_str_1(32, 'X');
  absl::Duration plan_duration_1 = absl::Minutes(5);
  ASSERT_OK(RunSuccessfulUploadViaSimpleAgg(
      kMultiTaskClientSessionId_1, "task_0", kMultiTaskAggregationSessionId_1,
      kMultiTaskId_1, plan_duration_1, checkpoint_str_1));

  // Upload the result from the second task.
  std::string checkpoint_str_2(32, 'Y');
  absl::Duration plan_duration_2 = absl::Minutes(6);
  ASSERT_OK(RunSuccessfulUploadViaSimpleAgg(
      kMultiTaskClientSessionId_2, "task_1", kMultiTaskAggregationSessionId_2,
      kMultiTaskId_2, plan_duration_2, checkpoint_str_2));

  // Run regular checkin, note we won't report eligibility eval result again
  // since we have done that during PerformMultipleTaskAssignments.
  ASSERT_OK(RunSuccessfulCheckin(/*report_eligibility_eval_result=*/false));

  // Upload the result from the task returned by the regular checkin.
  std::string checkpoint_str_3(32, 'Z');
  absl::Duration plan_duration_3 = absl::Minutes(7);
  ASSERT_OK(RunSuccessfulUploadViaSimpleAgg(
      kClientSessionId, std::nullopt, kAggregationSessionId, kTaskName,
      plan_duration_3, checkpoint_str_3, /*use_per_task_upload=*/false));
}

TEST_F(HttpFederatedProtocolTest,
       TestClientDecodedResourcesEnabledDeclaresSupport) {
  EligibilityEvalTaskRequest expected_eligibility_request;
  expected_eligibility_request.mutable_client_version()->set_version_code(
      kClientVersion);
  expected_eligibility_request.mutable_attestation_measurement()->set_value(
      kAttestationMeasurement);
  // Make sure gzip support is declared in the eligibility eval checkin request.
  expected_eligibility_request.mutable_resource_capabilities()
      ->add_supported_compression_formats(
          ResourceCompressionFormat::RESOURCE_COMPRESSION_FORMAT_GZIP);
  expected_eligibility_request.mutable_eligibility_eval_task_capabilities()
      ->set_supports_multiple_task_assignment(true);
  expected_eligibility_request.mutable_eligibility_eval_task_capabilities()
      ->set_supports_native_eets(true);

  // Issue an eligibility eval checkin so we can validate the field is set.
  Resource eligibility_plan_resource;
  eligibility_plan_resource.mutable_inline_resource()->set_data(kPlan);
  Resource checkpoint_resource;
  checkpoint_resource.mutable_inline_resource()->set_data(kInitCheckpoint);

  EligibilityEvalTaskResponse eval_task_response =
      GetFakeEnabledEligibilityEvalTaskResponse(eligibility_plan_resource,
                                                checkpoint_resource,
                                                kEligibilityEvalExecutionId);
  const std::string eligibility_request_uri =
      "https://initial.uri/v1/eligibilityevaltasks/"
      "TEST%2FPOPULATION:request?%24alt=proto";
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  eligibility_request_uri, HttpRequest::Method::kPost, _,
                  EligibilityEvalTaskRequestMatcher(
                      EqualsProto(expected_eligibility_request)))))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(), eval_task_response.SerializeAsString())));

  ASSERT_OK(federated_protocol_->EligibilityEvalCheckin(
      mock_eet_received_callback_.AsStdFunction()));

  // Now issue a regular checkin and make sure the field is set there too.
  const std::string plan_uri = "https://fake.uri/plan";
  Resource plan_resource;
  plan_resource.set_uri(plan_uri);
  StartTaskAssignmentResponse task_assignment_response =
      GetFakeTaskAssignmentResponse(plan_resource, checkpoint_resource,
                                    kFederatedSelectUriTemplate,
                                    kAggregationSessionId, 0);
  const std::string request_uri =
      "https://taskassignment.uri/v1/populations/TEST%2FPOPULATION/"
      "taskassignments/ELIGIBILITY%2FSESSION%23ID:start?%24alt=proto";
  TaskEligibilityInfo expected_eligibility_info = GetFakeTaskEligibilityInfo();
  StartTaskAssignmentRequest expected_request;
  expected_request.mutable_client_version()->set_version_code(kClientVersion);
  *expected_request.mutable_task_eligibility_info() = expected_eligibility_info;
  // Make sure gzip support is declared in the regular checkin request.
  expected_request.mutable_resource_capabilities()
      ->add_supported_compression_formats(
          ResourceCompressionFormat::RESOURCE_COMPRESSION_FORMAT_GZIP);

  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          request_uri, HttpRequest::Method::kPost, _,
          StartTaskAssignmentRequestMatcher(EqualsProto(expected_request)))))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreateDoneOperation(kOperationName, task_assignment_response)
              .SerializeAsString())));

  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  plan_uri, HttpRequest::Method::kGet, _, "")))
      .WillOnce(Return(FakeHttpResponse(200, HeaderList(), kPlan)));

  std::string report_eet_request_uri =
      "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
      "eligibilityevaltasks/"
      "ELIGIBILITY%2FSESSION%23ID:reportresult?%24alt=proto";
  ExpectSuccessfulReportEligibilityEvalTaskResultRequest(report_eet_request_uri,
                                                         absl::OkStatus());

  ASSERT_OK(federated_protocol_->Checkin(
      expected_eligibility_info, mock_task_received_callback_.AsStdFunction(),
      std::nullopt));
}
TEST_F(HttpFederatedProtocolTest, TestRelativePathForwardingSimpleAgg) {
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin(true, false,
                                                /*set_relative_uri=*/true));

  ASSERT_OK(
      RunSuccessfulCheckin(true, std::nullopt, /*set_relative_uri=*/true));

  std::string checkpoint_str;
  const size_t kTFCheckpointSize = 32;
  checkpoint_str.resize(kTFCheckpointSize, 'X');
  ComputationResults results;
  results.emplace("tensorflow_checkpoint", checkpoint_str);
  absl::Duration plan_duration = absl::Minutes(5);

  ExpectSuccessfulReportTaskResultRequest(
      "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
      "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
      kAggregationSessionId, kTaskName, plan_duration);
  ExpectSuccessfulStartAggregationDataUploadRequest(
      "https://initial.uri/v1/aggregations/AGGREGATION_SESSION_ID/"
      "clients/AUTHORIZATION_TOKEN:startdataupload?%24alt=proto",
      kResourceName, /*byte_stream_uri_prefix=*/"/",
      /*second_stage_aggregation_uri_prefix=*/"/", true);
  ExpectSuccessfulByteStreamUploadRequest(
      "https://initial.uri/upload/v1/media/"
      "CHECKPOINT_RESOURCE?upload_protocol=raw",
      checkpoint_str);
  ExpectSuccessfulSubmitAggregationResultRequest(
      "https://initial.uri/v1/aggregations/"
      "AGGREGATION_SESSION_ID/clients/CLIENT_TOKEN:submit?%24alt=proto");

  EXPECT_OK(federated_protocol_->ReportCompleted(
      std::move(results), plan_duration, std::nullopt, payload_metadata_));
}

TEST_F(HttpFederatedProtocolTest,
       TestRelativePathForwardingSimpleAggMixedRelativeAndAbsolute) {
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin(true, false,
                                                /*set_relative_uri=*/true));
  ASSERT_OK(
      RunSuccessfulCheckin(true, std::nullopt, /*set_relative_uri=*/true));

  std::string checkpoint_str;
  const size_t kTFCheckpointSize = 32;
  checkpoint_str.resize(kTFCheckpointSize, 'X');
  ComputationResults results;
  results.emplace("tensorflow_checkpoint", checkpoint_str);
  absl::Duration plan_duration = absl::Minutes(5);

  ExpectSuccessfulReportTaskResultRequest(
      "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
      "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
      kAggregationSessionId, kTaskName, plan_duration);

  // Use absolute paths for the ByteStream and second stage aggregation URI
  // prefixes.
  ExpectSuccessfulStartAggregationDataUploadRequest(
      "https://initial.uri/v1/aggregations/AGGREGATION_SESSION_ID/"
      "clients/AUTHORIZATION_TOKEN:startdataupload?%24alt=proto",
      kResourceName, kByteStreamTargetUri, kSecondStageAggregationTargetUri,
      true);
  ExpectSuccessfulByteStreamUploadRequest(
      "https://bytestream.uri/upload/v1/media/"
      "CHECKPOINT_RESOURCE?upload_protocol=raw",
      checkpoint_str);
  ExpectSuccessfulSubmitAggregationResultRequest(
      "https://aggregation.second.uri/v1/aggregations/"
      "AGGREGATION_SESSION_ID/clients/CLIENT_TOKEN:submit?%24alt=proto");

  EXPECT_OK(federated_protocol_->ReportCompleted(
      std::move(results), plan_duration, std::nullopt, payload_metadata_));
}

TEST_F(HttpFederatedProtocolTest,
       TestRelativePathForwardingRelativeDataUploadForwardingInfo) {
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin(true, false,
                                                /*set_relative_uri=*/true));

  ASSERT_OK(
      RunSuccessfulCheckin(true, std::nullopt, /*set_relative_uri=*/true));

  std::string checkpoint_str;
  const size_t kTFCheckpointSize = 32;
  checkpoint_str.resize(kTFCheckpointSize, 'X');
  ComputationResults results;
  results.emplace("tensorflow_checkpoint", checkpoint_str);
  absl::Duration plan_duration = absl::Minutes(5);

  ExpectSuccessfulReportTaskResultRequest(
      "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
      "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
      kAggregationSessionId, kTaskName, plan_duration);
  ExpectSuccessfulStartAggregationDataUploadRequest(
      "https://initial.uri/v1/aggregations/AGGREGATION_SESSION_ID/"
      "clients/AUTHORIZATION_TOKEN:startdataupload?%24alt=proto",
      kResourceName, /*byte_stream_uri_prefix=*/"/",
      kSecondStageAggregationTargetUri, true);

  // Although we process the second stage aggregation target URI prior to
  // resolving the relative byte stream URI prefix above, we ensure the relative
  // path is resolved using the previous stage's resolved protocol and hostname.
  ExpectSuccessfulByteStreamUploadRequest(
      "https://initial.uri/upload/v1/media/"
      "CHECKPOINT_RESOURCE?upload_protocol=raw",
      checkpoint_str);
  ExpectSuccessfulSubmitAggregationResultRequest(
      "https://aggregation.second.uri/v1/aggregations/"
      "AGGREGATION_SESSION_ID/clients/CLIENT_TOKEN:submit?%24alt=proto");

  EXPECT_OK(federated_protocol_->ReportCompleted(
      std::move(results), plan_duration, std::nullopt, payload_metadata_));
}

TEST_F(HttpFederatedProtocolTest,
       TestRelativePathForwardingConfidentialAggregations) {
  EXPECT_CALL(mock_flags_, enable_confidential_aggregation)
      .WillRepeatedly(Return(true));

  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin(
      true, /*enable_confidential_aggregation=*/true,
      /*set_relative_uri=*/true));

  std::string serialized_access_policy = "the access policy";
  ASSERT_OK(RunSuccessfulCheckin(true, serialized_access_policy,
                                 /*set_relative_uri=*/true));

  // Create a fake checkpoint with 32 'X'.
  std::string checkpoint_str(32, 'X');
  ComputationResults results;
  results.emplace("tensorflow_checkpoint", checkpoint_str);
  absl::Duration plan_duration = absl::Minutes(5);

  fcp::confidential_compute::MessageDecryptor decryptor;
  auto encoded_public_key =
      decryptor
          .GetPublicKey(
              [](absl::string_view payload) { return "fakesignature"; }, 0)
          .value();
  absl::StatusOr<OkpCwt> parsed_public_key = OkpCwt::Decode(encoded_public_key);
  ASSERT_OK(parsed_public_key);
  ASSERT_TRUE(parsed_public_key->public_key.has_value());

  ConfidentialEncryptionConfig encryption_config;
  encryption_config.set_public_key(encoded_public_key);
  // Empty SignedEndorsements since the task does not use endorsements.
  confidentialcompute::SignedEndorsements signed_endorsements;

  // Ensure that the server's attestation evidence is considered valid.
  EXPECT_CALL(
      *mock_attestation_verifier_,
      Verify(Eq(serialized_access_policy), _, EqualsProto(encryption_config)))
      .WillRepeatedly(
          [=](const absl::Cord& access_policy,
              const confidentialcompute::SignedEndorsements&
                  signed_endorsements,
              const ConfidentialEncryptionConfig& encryption_config) {
            return attestation::AlwaysPassingAttestationVerifier().Verify(
                access_policy, signed_endorsements, encryption_config);
          });

  ExpectSuccessfulReportTaskResultRequest(
      "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
      "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
      kAggregationSessionId, kTaskName, plan_duration);
  ExpectSuccessfulStartAggregationDataUploadRequest(
      "https://initial.uri/v1/confidentialaggregations/AGGREGATION_SESSION_ID/"
      "clients/AUTHORIZATION_TOKEN:startdataupload?%24alt=proto",
      kResourceName, /*byte_stream_uri_prefix=*/"/",
      /*second_stage_aggregation_uri_prefix=*/"/", true, encryption_config);

  std::string uploaded_data;
  EXPECT_CALL(mock_http_client_, PerformSingleRequest(SimpleHttpRequestMatcher(
                                     "https://initial.uri/upload/v1/media/"
                                     "CHECKPOINT_RESOURCE?upload_protocol=raw",
                                     HttpRequest::Method::kPost, _, _)))
      .WillOnce([&uploaded_data](MockHttpClient::SimpleHttpRequest request) {
        uploaded_data = request.body;
        return CreateEmptySuccessHttpResponse();
      });

  ExpectSuccessfulSubmitAggregationResultRequest(
      "https://initial.uri/v1/confidentialaggregations/"
      "AGGREGATION_SESSION_ID/clients/CLIENT_TOKEN:submit?%24alt=proto",
      /*confidential_aggregation=*/true);

  EXPECT_OK(federated_protocol_->ReportCompleted(
      std::move(results), plan_duration, std::nullopt, payload_metadata_));
}

TEST_F(HttpFederatedProtocolTest,
       TestRelativePathForwardingCAMixedRelativeAndAbsolute) {
  EXPECT_CALL(mock_flags_, enable_confidential_aggregation)
      .WillRepeatedly(Return(true));

  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin(
      true, /*enable_confidential_aggregation=*/true,
      /*set_relative_uri=*/true));

  std::string serialized_access_policy = "the access policy";
  ASSERT_OK(RunSuccessfulCheckin(true, serialized_access_policy,
                                 /*set_relative_uri=*/true));

  // Create a fake checkpoint with 32 'X'.
  std::string checkpoint_str(32, 'X');
  ComputationResults results;
  results.emplace("tensorflow_checkpoint", checkpoint_str);
  absl::Duration plan_duration = absl::Minutes(5);

  fcp::confidential_compute::MessageDecryptor decryptor;
  auto encoded_public_key =
      decryptor
          .GetPublicKey(
              [](absl::string_view payload) { return "fakesignature"; }, 0)
          .value();
  absl::StatusOr<OkpCwt> parsed_public_key = OkpCwt::Decode(encoded_public_key);
  ASSERT_OK(parsed_public_key);
  ASSERT_TRUE(parsed_public_key->public_key.has_value());

  ConfidentialEncryptionConfig encryption_config;
  encryption_config.set_public_key(encoded_public_key);
  // Empty SignedEndorsements since the task does not use endorsements.
  confidentialcompute::SignedEndorsements signed_endorsements;

  // Ensure that the server's attestation evidence is considered valid.
  EXPECT_CALL(
      *mock_attestation_verifier_,
      Verify(Eq(serialized_access_policy), _, EqualsProto(encryption_config)))
      .WillRepeatedly(
          [=](const absl::Cord& access_policy,
              const confidentialcompute::SignedEndorsements&
                  signed_endorsements,
              const ConfidentialEncryptionConfig& encryption_config) {
            return attestation::AlwaysPassingAttestationVerifier().Verify(
                access_policy, signed_endorsements, encryption_config);
          });

  ExpectSuccessfulReportTaskResultRequest(
      "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
      "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
      kAggregationSessionId, kTaskName, plan_duration);

  // Use absolute paths for the ByteStream and aggregation URI
  // prefixes.
  ExpectSuccessfulStartAggregationDataUploadRequest(
      "https://initial.uri/v1/confidentialaggregations/AGGREGATION_SESSION_ID/"
      "clients/AUTHORIZATION_TOKEN:startdataupload?%24alt=proto",
      kResourceName, kByteStreamTargetUri, kAggregationTargetUri, true,
      encryption_config);

  std::string uploaded_data;
  EXPECT_CALL(mock_http_client_, PerformSingleRequest(SimpleHttpRequestMatcher(
                                     "https://bytestream.uri/upload/v1/media/"
                                     "CHECKPOINT_RESOURCE?upload_protocol=raw",
                                     HttpRequest::Method::kPost, _, _)))
      .WillOnce([&uploaded_data](MockHttpClient::SimpleHttpRequest request) {
        uploaded_data = request.body;
        return CreateEmptySuccessHttpResponse();
      });

  ExpectSuccessfulSubmitAggregationResultRequest(
      "https://aggregation.uri/v1/confidentialaggregations/"
      "AGGREGATION_SESSION_ID/clients/CLIENT_TOKEN:submit?%24alt=proto",
      /*confidential_aggregation=*/true);

  EXPECT_OK(federated_protocol_->ReportCompleted(
      std::move(results), plan_duration, std::nullopt, payload_metadata_));
}

TEST_F(HttpFederatedProtocolTest,
       TestRelativePathForwardingCARelativeDataUploadForwardingInfo) {
  EXPECT_CALL(mock_flags_, enable_confidential_aggregation)
      .WillRepeatedly(Return(true));

  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin(
      true, /*enable_confidential_aggregation=*/true,
      /*set_relative_uri=*/true));

  std::string serialized_access_policy = "the access policy";
  ASSERT_OK(RunSuccessfulCheckin(true, serialized_access_policy,
                                 /*set_relative_uri=*/true));

  // Create a fake checkpoint with 32 'X'.
  std::string checkpoint_str(32, 'X');
  ComputationResults results;
  results.emplace("tensorflow_checkpoint", checkpoint_str);
  absl::Duration plan_duration = absl::Minutes(5);

  fcp::confidential_compute::MessageDecryptor decryptor;
  auto encoded_public_key =
      decryptor
          .GetPublicKey(
              [](absl::string_view payload) { return "fakesignature"; }, 0)
          .value();
  absl::StatusOr<OkpCwt> parsed_public_key = OkpCwt::Decode(encoded_public_key);
  ASSERT_OK(parsed_public_key);
  ASSERT_TRUE(parsed_public_key->public_key.has_value());

  ConfidentialEncryptionConfig encryption_config;
  encryption_config.set_public_key(encoded_public_key);
  // Empty SignedEndorsements since the task does not use endorsements.
  confidentialcompute::SignedEndorsements signed_endorsements;

  // Ensure that the server's attestation evidence is considered valid.
  EXPECT_CALL(
      *mock_attestation_verifier_,
      Verify(Eq(serialized_access_policy), _, EqualsProto(encryption_config)))
      .WillRepeatedly(
          [=](const absl::Cord& access_policy,
              const confidentialcompute::SignedEndorsements&
                  signed_endorsements,

              const ConfidentialEncryptionConfig& encryption_config) {
            return attestation::AlwaysPassingAttestationVerifier().Verify(
                access_policy, signed_endorsements, encryption_config);
          });

  ExpectSuccessfulReportTaskResultRequest(
      "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
      "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
      kAggregationSessionId, kTaskName, plan_duration);
  ExpectSuccessfulStartAggregationDataUploadRequest(
      "https://initial.uri/v1/confidentialaggregations/AGGREGATION_SESSION_ID/"
      "clients/AUTHORIZATION_TOKEN:startdataupload?%24alt=proto",
      kResourceName, /*byte_stream_uri_prefix=*/"/",
      kSecondStageAggregationTargetUri, true, encryption_config);

  std::string uploaded_data;
  EXPECT_CALL(mock_http_client_, PerformSingleRequest(SimpleHttpRequestMatcher(
                                     "https://initial.uri/upload/v1/media/"
                                     "CHECKPOINT_RESOURCE?upload_protocol=raw",
                                     HttpRequest::Method::kPost, _, _)))
      .WillOnce([&uploaded_data](MockHttpClient::SimpleHttpRequest request) {
        uploaded_data = request.body;
        return CreateEmptySuccessHttpResponse();
      });

  ExpectSuccessfulSubmitAggregationResultRequest(
      "https://aggregation.second.uri/v1/confidentialaggregations/"
      "AGGREGATION_SESSION_ID/clients/CLIENT_TOKEN:submit?%24alt=proto",
      /*confidential_aggregation=*/true);

  EXPECT_OK(federated_protocol_->ReportCompleted(
      std::move(results), plan_duration, std::nullopt, payload_metadata_));
}

TEST_F(HttpFederatedProtocolTest, TestRelativePathForwardingSecAgg) {
  absl::Duration plan_duration = absl::Minutes(5);

  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin(true, false,
                                                /*set_relative_uri=*/true));

  ASSERT_OK(
      RunSuccessfulCheckin(true, std::nullopt, /*set_relative_uri=*/true));

  StartSecureAggregationResponse start_secure_aggregation_response;
  start_secure_aggregation_response.set_client_token(kClientToken);
  auto masked_result_resource =
      start_secure_aggregation_response.mutable_masked_result_resource();
  masked_result_resource->set_resource_name("masked_resource");
  masked_result_resource->mutable_data_upload_forwarding_info()
      ->set_target_uri_prefix("/");

  auto nonmasked_result_resource =
      start_secure_aggregation_response.mutable_nonmasked_result_resource();
  nonmasked_result_resource->set_resource_name("nonmasked_resource");
  nonmasked_result_resource->mutable_data_upload_forwarding_info()
      ->set_target_uri_prefix("/");

  start_secure_aggregation_response.mutable_secagg_protocol_forwarding_info()
      ->set_target_uri_prefix("/");
  auto protocol_execution_info =
      start_secure_aggregation_response.mutable_protocol_execution_info();
  protocol_execution_info->set_minimum_surviving_clients_for_reconstruction(
      450);
  protocol_execution_info->set_expected_number_of_clients(500);

  auto secure_aggregands =
      start_secure_aggregation_response.mutable_secure_aggregands();
  SecureAggregandExecutionInfo secure_aggregand_execution_info;
  secure_aggregand_execution_info.set_modulus(9999);
  (*secure_aggregands)["secagg_tensor"] = secure_aggregand_execution_info;

  ExpectSuccessfulReportTaskResultRequest(
      "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
      "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
      kAggregationSessionId, kTaskName, plan_duration);
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://initial.uri/v1/secureaggregations/"
                  "AGGREGATION_SESSION_ID/clients/"
                  "AUTHORIZATION_TOKEN:start?%24alt=proto",
                  HttpRequest::Method::kPost, _,
                  StartSecureAggregationRequest().SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreatePendingOperation("operations/foo#bar").SerializeAsString())));
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://initial.uri/v1/operations/foo%23bar?%24alt=proto",
                  HttpRequest::Method::kGet, _, "")))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreateDoneOperation(kOperationName, start_secure_aggregation_response)
              .SerializeAsString())));

  // Create a fake checkpoint with 32 'X'.
  std::string checkpoint_str(32, 'X');
  ComputationResults results;
  results.emplace("tensorflow_checkpoint", checkpoint_str);
  results.emplace("secagg_tensor", QuantizedTensor());

  EXPECT_CALL(*mock_secagg_runner_factory_,
              CreateSecAggRunner(_, _, _, _, _, 500, 450))
      .WillOnce(WithArg<0>([&](auto send_to_server_impl) {
        auto mock_secagg_runner =
            std::make_unique<StrictMock<MockSecAggRunner>>();
        EXPECT_CALL(*mock_secagg_runner,
                    Run(UnorderedElementsAre(Pair(
                        "secagg_tensor", VariantWith<QuantizedTensor>(FieldsAre(
                                             IsEmpty(), 0, IsEmpty()))))))
            .WillOnce(
                [=, send_to_server_impl = std::move(send_to_server_impl)] {
                  // SecAggSendToServerBase::Send should use the client token.
                  // This needs to be tested here since `send_to_server_impl`
                  // should not be used outside of Run.
                  EXPECT_CALL(mock_http_client_,
                              PerformSingleRequest(SimpleHttpRequestMatcher(
                                  "https://initial.uri/v1/secureaggregations/"
                                  "AGGREGATION_SESSION_ID/clients/"
                                  "CLIENT_TOKEN:abort?%24alt=proto",
                                  _, _, _)))
                      .WillOnce(Return(CreateEmptySuccessHttpResponse()));
                  secagg::ClientToServerWrapperMessage abort_message;
                  abort_message.mutable_abort();
                  send_to_server_impl->Send(&abort_message);

                  return absl::OkStatus();
                });
        return mock_secagg_runner;
      }));

  EXPECT_OK(federated_protocol_->ReportCompleted(
      std::move(results), plan_duration, std::nullopt, payload_metadata_));
}

TEST_F(HttpFederatedProtocolTest,
       TestRelativePathForwardingSecAggRelativeSecAggMixedRelativeandAbsolute) {
  absl::Duration plan_duration = absl::Minutes(5);

  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin(true, false,
                                                /*set_relative_uri=*/true));

  ASSERT_OK(
      RunSuccessfulCheckin(true, std::nullopt, /*set_relative_uri=*/true));

  StartSecureAggregationResponse start_secure_aggregation_response;
  start_secure_aggregation_response.set_client_token(kClientToken);
  auto masked_result_resource =
      start_secure_aggregation_response.mutable_masked_result_resource();
  masked_result_resource->set_resource_name("masked_resource");
  masked_result_resource->mutable_data_upload_forwarding_info()
      ->set_target_uri_prefix("/");

  auto nonmasked_result_resource =
      start_secure_aggregation_response.mutable_nonmasked_result_resource();
  nonmasked_result_resource->set_resource_name("nonmasked_resource");
  nonmasked_result_resource->mutable_data_upload_forwarding_info()
      ->set_target_uri_prefix("/");

  start_secure_aggregation_response.mutable_secagg_protocol_forwarding_info()
      ->set_target_uri_prefix("https://aggregation.second.uri/");
  auto protocol_execution_info =
      start_secure_aggregation_response.mutable_protocol_execution_info();
  protocol_execution_info->set_minimum_surviving_clients_for_reconstruction(
      450);
  protocol_execution_info->set_expected_number_of_clients(500);

  auto secure_aggregands =
      start_secure_aggregation_response.mutable_secure_aggregands();
  SecureAggregandExecutionInfo secure_aggregand_execution_info;
  secure_aggregand_execution_info.set_modulus(9999);
  (*secure_aggregands)["secagg_tensor"] = secure_aggregand_execution_info;

  ExpectSuccessfulReportTaskResultRequest(
      "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
      "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
      kAggregationSessionId, kTaskName, plan_duration);
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://initial.uri/v1/secureaggregations/"
                  "AGGREGATION_SESSION_ID/clients/"
                  "AUTHORIZATION_TOKEN:start?%24alt=proto",
                  HttpRequest::Method::kPost, _,
                  StartSecureAggregationRequest().SerializeAsString())))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreatePendingOperation("operations/foo#bar").SerializeAsString())));
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://initial.uri/v1/operations/foo%23bar?%24alt=proto",
                  HttpRequest::Method::kGet, _, "")))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList(),
          CreateDoneOperation(kOperationName, start_secure_aggregation_response)
              .SerializeAsString())));

  // Create a fake checkpoint with 32 'X'.
  std::string checkpoint_str(32, 'X');
  ComputationResults results;
  results.emplace("tensorflow_checkpoint", checkpoint_str);
  results.emplace("secagg_tensor", QuantizedTensor());

  EXPECT_CALL(*mock_secagg_runner_factory_,
              CreateSecAggRunner(_, _, _, _, _, 500, 450))
      .WillOnce(WithArg<0>([&](auto send_to_server_impl) {
        auto mock_secagg_runner =
            std::make_unique<StrictMock<MockSecAggRunner>>();
        EXPECT_CALL(*mock_secagg_runner,
                    Run(UnorderedElementsAre(Pair(
                        "secagg_tensor", VariantWith<QuantizedTensor>(FieldsAre(
                                             IsEmpty(), 0, IsEmpty()))))))
            .WillOnce([=,
                       send_to_server_impl = std::move(send_to_server_impl)] {
              // SecAggSendToServerBase::Send should use the client token.
              // This needs to be tested here since `send_to_server_impl`
              // should not be used outside of Run.
              EXPECT_CALL(
                  mock_http_client_,
                  PerformSingleRequest(SimpleHttpRequestMatcher(
                      "https://aggregation.second.uri/v1/secureaggregations/"
                      "AGGREGATION_SESSION_ID/clients/"
                      "CLIENT_TOKEN:abort?%24alt=proto",
                      _, _, _)))
                  .WillOnce(Return(CreateEmptySuccessHttpResponse()));
              secagg::ClientToServerWrapperMessage abort_message;
              abort_message.mutable_abort();
              send_to_server_impl->Send(&abort_message);

              return absl::OkStatus();
            });
        return mock_secagg_runner;
      }));

  EXPECT_OK(federated_protocol_->ReportCompleted(
      std::move(results), plan_duration, std::nullopt, payload_metadata_));
}

TEST_F(HttpFederatedProtocolTest, TestRelativePathForwardingNoTrailingSlash) {
  ASSERT_OK(RunSuccessfulEligibilityEvalCheckin(true, false,
                                                /*set_relative_uri=*/true));

  ASSERT_OK(
      RunSuccessfulCheckin(true, std::nullopt, /*set_relative_uri=*/true));

  std::string checkpoint_str;
  const size_t kTFCheckpointSize = 32;
  checkpoint_str.resize(kTFCheckpointSize, 'X');
  ComputationResults results;
  results.emplace("tensorflow_checkpoint", checkpoint_str);
  absl::Duration plan_duration = absl::Minutes(5);

  ExpectSuccessfulReportTaskResultRequest(
      "https://initial.uri/v1/populations/TEST%2FPOPULATION/"
      "taskassignments/CLIENT_SESSION_ID:reportresult?%24alt=proto",
      kAggregationSessionId, kTaskName, plan_duration);

  // Note these URIs do not contain a trailing slash and this should be fine.
  ExpectSuccessfulStartAggregationDataUploadRequest(
      "https://initial.uri/v1/aggregations/AGGREGATION_SESSION_ID/"
      "clients/AUTHORIZATION_TOKEN:startdataupload?%24alt=proto",
      kResourceName, "https://bytestream.uri", "https://aggregation.second.uri",
      true);
  ExpectSuccessfulByteStreamUploadRequest(
      "https://bytestream.uri/upload/v1/media/"
      "CHECKPOINT_RESOURCE?upload_protocol=raw",
      checkpoint_str);
  ExpectSuccessfulSubmitAggregationResultRequest(
      "https://aggregation.second.uri/v1/aggregations/"
      "AGGREGATION_SESSION_ID/clients/CLIENT_TOKEN:submit?%24alt=proto");

  EXPECT_OK(federated_protocol_->ReportCompleted(
      std::move(results), plan_duration, std::nullopt, payload_metadata_));
}

}  // anonymous namespace
}  // namespace fcp::client::http
