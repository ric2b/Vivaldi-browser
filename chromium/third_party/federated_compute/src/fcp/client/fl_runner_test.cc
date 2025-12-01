/*
 * Copyright 2020 Google LLC
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
#include "fcp/client/fl_runner.h"

#include <fcntl.h>

#include <atomic>
#include <cstdint>
#include <filesystem>  // NOLINT(build/c++17)
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/duration.pb.h"
#include "google/type/datetime.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "fcp/base/digest.h"
#include "fcp/base/function_registry.h"
#include "fcp/base/simulated_clock.h"
#include "fcp/client/cache/temp_files.h"
#include "fcp/client/client_runner.h"
#include "fcp/client/engine/common.h"
#include "fcp/client/engine/engine.pb.h"
#include "fcp/client/event_time_range.pb.h"
#include "fcp/client/example_query_result.pb.h"
#include "fcp/client/federated_protocol.h"
#include "fcp/client/fl_runner.pb.h"
#include "fcp/client/interruptible_runner.h"
#include "fcp/client/runner_common.h"
#include "fcp/client/selector_context.pb.h"
#include "fcp/client/stats.h"
#include "fcp/client/task_result_info.pb.h"
#include "fcp/client/tensorflow/tensorflow_runner_factory.h"
#include "fcp/client/test_helpers.h"
#include "fcp/protos/confidentialcompute/payload_metadata.pb.h"
#include "fcp/protos/data_type.pb.h"
#include "fcp/protos/federated_api.pb.h"
#include "fcp/protos/opstats.pb.h"
#include "fcp/protos/plan.pb.h"
#include "fcp/protos/population_eligibility_spec.pb.h"
#include "fcp/testing/testing.h"
#include "google/protobuf/map.h"
#include "google/protobuf/repeated_ptr_field.h"
#include "tensorflow/core/example/example.pb.h"
#include "tensorflow/core/framework/tensor_shape.pb.h"
#include "tensorflow/core/protobuf/struct.pb.h"

namespace fcp {
namespace client {
namespace {

using ::fcp::client::ExampleQueryResult;
using ::fcp::confidentialcompute::PayloadMetadata;
using ::google::internal::federated::plan::AggregationConfig;
using ::google::internal::federated::plan::ClientOnlyPlan;
using ::google::internal::federated::plan::Dataset;
using ::google::internal::federated::plan::DataType;
using ::google::internal::federated::plan::ExampleQuerySpec;
using ::google::internal::federated::plan::ExampleSelector;
using ::google::internal::federated::plan::Metric;
using ::google::internal::federated::plan::PopulationEligibilitySpec;
using ::google::internal::federatedml::v2::RetryWindow;
using ::google::internal::federatedml::v2::TaskEligibilityInfo;
using ::google::internal::federatedml::v2::TaskWeight;
using ::google::protobuf::Any;
using ::testing::_;
using ::testing::AtMost;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::FieldsAre;
using ::testing::Ge;
using ::testing::Gt;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::VariantWith;

constexpr NetworkStats kFederatedSelectNetworkStats = {
    .bytes_downloaded = 300000,
    .bytes_uploaded = 400000,
    .network_duration = absl::Milliseconds(1000)};
// Create aliases for some of MockFederatedProtocol's static constants, so we
// can be less verbose.
const NetworkStats kPostEligibilityCheckinPlanUriReceivedNetworkStats =
    ::fcp::client::MockFederatedProtocol::
        kEligibilityCheckinPlanUriReceivedNetworkStats;
const NetworkStats kPostEligibilityCheckinNetworkStats =
    kPostEligibilityCheckinPlanUriReceivedNetworkStats +
    ::fcp::client::MockFederatedProtocol::
        kEligibilityCheckinArtifactRetrievalNetworkStats;
const NetworkStats kPostCheckinPlanUriReceivedNetworkStats =
    kPostEligibilityCheckinNetworkStats +
    ::fcp::client::MockFederatedProtocol::kCheckinPlanUriReceivedNetworkStats;
const NetworkStats kPostCheckinNetworkStats =
    kPostCheckinPlanUriReceivedNetworkStats +
    ::fcp::client::MockFederatedProtocol::kCheckinArtifactRetrievalNetworkStats;
const NetworkStats kPostReportCompletedNetworkStats =
    kPostCheckinNetworkStats +
    ::fcp::client::MockFederatedProtocol::kReportCompletedNetworkStats;
const NetworkStats kPostReportNotCompletedNetworkStats =
    kPostCheckinNetworkStats +
    ::fcp::client::MockFederatedProtocol::kReportNotCompletedNetworkStats;

constexpr char kSessionName[] = "test_session";
constexpr char kPopulationName[] = "test_population";
constexpr char kEligibilityEvalExecutionId[] =
    "test_population/eligibility_eval_execution_id";
constexpr char kAggregationSessionId[] = "aggregation-session-id";
constexpr char kTaskName[] = "test_task";
constexpr char kFederatedSelectUriTemplate[] = "https://federated.select";
constexpr char kAnotherFederatedSelectUriTemplate[] =
    "https://federated.select.alternative";
constexpr absl::string_view kSwor24HourTaskName = "swor;version:1;period:24hr";
constexpr absl::string_view kRequires5ExamplesTaskName =
    "data_availability;name:d1_5_examples;version:1;uri:app:/"
    "test_eligibility_eval_collection;min_example_count:5";
constexpr absl::string_view kMultipleTaskAggregationSessionId1 =
    "aggregation-session-id-1";
constexpr absl::string_view kMultipleTaskAggregationSessionId2 =
    "aggregation-session-id-2";
constexpr FederatedProtocol::SecAggInfo kSecAggInfoForMixedSecAgg{
    .expected_number_of_clients = 4,
    .minimum_clients_in_server_visible_aggregate = 3};
constexpr FederatedProtocol::SecAggInfo kSecAggInfoForPureSecAggTask{
    .expected_number_of_clients = 3,
    .minimum_clients_in_server_visible_aggregate = 2};
constexpr int64_t kMinSepPolicyCurrentIndex = 1;
constexpr absl::string_view kTaskIdentifier1 = "task_0";
constexpr absl::string_view kTaskIdentifier2 = "task_1";
constexpr absl::string_view kInitialCheckpoint = "init_checkpoint";
constexpr absl::string_view kEligibilityEvalCollectionUri =
    "app:/test_eligibility_eval_collection";

static testing::Matcher<const NetworkStats&> EqualsNetworkStats(
    const NetworkStats& other) {
  return AllOf(Field("bytes_downloaded", &NetworkStats::bytes_downloaded,
                     other.bytes_downloaded),
               Field("bytes_uploaded", &NetworkStats::bytes_uploaded,
                     other.bytes_uploaded),
               Field("network_duration", &NetworkStats::network_duration,
                     other.network_duration));
}

MATCHER(IsTaskResultSuccess, "") { return arg.result(); }

MATCHER(IsTaskResultFailure, "") { return !arg.result(); }

StrictMock<MockTensorflowRunner>* GetMockTensorflowRunner() {
  static StrictMock<MockTensorflowRunner>* mock_tensorflow_runner =
      new StrictMock<MockTensorflowRunner>();
  testing::Mock::AllowLeak(mock_tensorflow_runner);
  return mock_tensorflow_runner;
}

// Register a TestingTensorflowRunner which delegates all calls to the mock.
const auto kUnused = fcp::RegisterOrDie(
    GetGlobalTensorflowRunnerFactoryRegistry(),
    TensorflowRunnerImplementation::kTensorflowRunnerImpl, []() {
      return std::make_unique<TestingTensorflowRunner>(
          GetMockTensorflowRunner());
    });

RetryInfo CreateRetryInfoFromRetryWindow(const RetryWindow& retry_window) {
  RetryInfo retry_info;
  *retry_info.mutable_retry_token() = retry_window.retry_token();
  *retry_info.mutable_minimum_delay() = retry_window.delay_min();
  return retry_info;
}

void WriteContentToFile(absl::string_view file_path,
                        absl::string_view content) {
  std::string file_path_str(file_path);
  std::string content_str(content);
  std::ofstream out(file_path_str.c_str(), std::ofstream::binary);
  out.write(content_str.data(), content_str.size());
  out.close();
}

class FlRunnerTestBase : public ::testing::Test {
 public:
  explicit FlRunnerTestBase();

 protected:
  void TearDown() override;
  // Sets up the FederatedProtocol mock so that if EligibilityEvalCheckin() is
  // called, it returns a EligibilityEvalDisabled result. This means that the
  // subsequent Checkin(...) request should not be given a TaskEligibilityInfo
  // value.
  void MockEligibilityEvalDisabled();

  void MockSuccessfulEligibilityPlanExecution(
      const TaskEligibilityInfo& task_eligibility_info);

  void MockSuccessfulPlanExecution(
      bool has_checkpoint, bool has_secagg_output,
      testing::Matcher<const ClientOnlyPlan&> plan_matcher = _);

  void ExpectEligibilityCheckinCompletedLogEvent();

  // Checks that the right log events have been logged for a session that
  // includes an eligibility eval checkin and plan execution.
  void ExpectEligibilityEvalLogEvents();

  void ExpectCheckinCompletedLogEvents();

  // Checks that the right log events have been logged for a session that
  // results in an accepted checkin, and a successful training run.
  void ExpectCheckinTrainingLogEvents(bool federated_select_enabled = false,
                                      bool has_min_sep_policy = false);

  // Checks that the right log events have been logged for a session that
  // results in an accepted checkin, a successful training run and a successful
  // report.
  void ExpectCheckinTrainingReportLogEvents(
      bool federated_select_enabled = false, bool has_min_sep_policy = false);

  fcp::client::FilesImpl files_impl_;
  StrictMock<MockSimpleTaskEnvironment> mock_task_env_;
  NiceMock<MockLogManager> mock_log_manager_;
  NiceMock<MockEventPublisher> mock_event_publisher_;
  NiceMock<MockOpStatsLogger> mock_opstats_logger_;
  NiceMock<MockOpStatsDb> mock_opstats_db_;
  StrictMock<MockPhaseLogger> mock_phase_logger_;
  NiceMock<MockFederatedSelectManager> mock_fedselect_manager_;
  NiceMock<MockFederatedSelectExampleIteratorFactory>*
      mock_federated_select_iterator_factory_;
  MockFlags mock_flags_;
  MockFederatedProtocol mock_federated_protocol_;
  StrictMock<MockTensorflowRunner>* mock_tensorflow_runner_;
  // Training conditions are satisfied by default.
  std::atomic<bool> training_conditions_satisfied_ = true;
  SelectorContext latest_eligibility_selector_context_;
  SelectorContext latest_selector_context_;
  RetryWindow latest_opstats_retry_window_;
  NetworkStats logged_network_stats_;
  SimulatedClock clock_;

  ComputationArtifacts single_task_assignment_artifacts_;
  ClientOnlyPlan single_task_assignment_client_only_plan_;

  fcp::client::InterruptibleRunner::TimingConfig timing_config_ = {
      .polling_period = absl::Milliseconds(10),
      .graceful_shutdown_period = absl::Milliseconds(1000),
      .extended_shutdown_period = absl::Milliseconds(2000),
  };
};

FlRunnerTestBase::FlRunnerTestBase() {
  EXPECT_CALL(mock_flags_, condition_polling_period_millis())
      .WillRepeatedly(Return(1000));
  EXPECT_CALL(mock_flags_, tf_execution_teardown_grace_period_millis())
      .WillRepeatedly(Return(1000));
  EXPECT_CALL(mock_flags_, tf_execution_teardown_extended_period_millis())
      .WillRepeatedly(Return(2000));
  EXPECT_CALL(mock_flags_, log_tensorflow_error_messages())
      .WillRepeatedly(Return(true));

  // By default all training conditions are satisfied, but this can be
  // changed mid-test by modifying the training_conditions_satisfied_
  // boolean field.
  EXPECT_CALL(mock_task_env_, TrainingConditionsSatisfied())
      .WillRepeatedly(
          Invoke([this]() { return training_conditions_satisfied_.load(); }));

  mock_federated_select_iterator_factory_ =
      new NiceMock<MockFederatedSelectExampleIteratorFactory>();
  ON_CALL(mock_fedselect_manager_, CreateExampleIteratorFactoryForUriTemplate(
                                       kFederatedSelectUriTemplate))
      .WillByDefault(Return(
          ByMove(absl::WrapUnique(mock_federated_select_iterator_factory_))));

  ON_CALL(mock_fedselect_manager_, GetNetworkStats())
      .WillByDefault(Return(NetworkStats()));

  // Whenever opstats is given new network stats we store them in a variable
  // for inspection at the end of the test.
  EXPECT_CALL(mock_phase_logger_, UpdateRetryWindowAndNetworkStats(_, _))
      .WillRepeatedly(DoAll(SaveArg<0>(&latest_opstats_retry_window_),
                            SaveArg<1>(&logged_network_stats_)));
  mock_tensorflow_runner_ = GetMockTensorflowRunner();

  // Create a non-empty tensorflow spec to make it a tensorflow plan.
  *single_task_assignment_client_only_plan_.mutable_phase()
       ->mutable_tensorflow_spec()
       ->mutable_dataset_token_tensor_name() = "dataset_token";
  single_task_assignment_artifacts_.plan =
      single_task_assignment_client_only_plan_;
  single_task_assignment_artifacts_.checkpoint = kInitialCheckpoint;
}

void FlRunnerTestBase::TearDown() {
  // At the end of all tests, the opstats info must match what the protocol
  // and the fedselect manager report, regardless of the outcome of the run.
  EXPECT_THAT(latest_opstats_retry_window_,
              EqualsProto(mock_federated_protocol_.GetLatestRetryWindow()));
  EXPECT_THAT(logged_network_stats_,
              EqualsNetworkStats(mock_federated_protocol_.GetNetworkStats()));
  ::testing::Test::TearDown();
}

void FlRunnerTestBase::MockEligibilityEvalDisabled() {
  EXPECT_CALL(mock_phase_logger_, LogEligibilityEvalCheckinStarted());
  EXPECT_CALL(mock_federated_protocol_, MockEligibilityEvalCheckin())
      .WillOnce(Return(FederatedProtocol::EligibilityEvalDisabled()));
  EXPECT_CALL(mock_phase_logger_,
              LogEligibilityEvalNotConfigured(
                  EqualsNetworkStats(
                      kPostEligibilityCheckinPlanUriReceivedNetworkStats),
                  _));
}

void FlRunnerTestBase::MockSuccessfulPlanExecution(
    bool has_checkpoint, bool has_secagg_output,
    testing::Matcher<const ClientOnlyPlan&> plan_matcher) {
  engine::PlanResult plan_result(engine::PlanOutcome::kSuccess,
                                 absl::OkStatus());
  plan_result.example_stats.example_count = 5;
  plan_result.example_stats.example_size_bytes = 10;
  if (has_secagg_output) {
    absl::flat_hash_map<std::string, QuantizedTensor> output_tensors;
    QuantizedTensor output_tensor;
    output_tensor.bitwidth = 32;
    output_tensor.values = {1, 2, 3, 4, 5};
    output_tensors["output_tensor"] = std::move(output_tensor);
    plan_result.secagg_tensor_map = std::move(output_tensors);
  }
  PlanResultAndCheckpointFile plan_result_and_checkpoint_file(
      std::move(plan_result));
  if (has_checkpoint) {
    auto checkpoint_file = files_impl_.CreateTempFile("output", ".ckpt");
    ASSERT_OK(checkpoint_file);
    WriteContentToFile(*checkpoint_file, "output_checkpoint");
    plan_result_and_checkpoint_file.checkpoint_filename =
        std::move(*checkpoint_file);
  }
  EXPECT_CALL(
      *mock_tensorflow_runner_,
      RunPlanWithTensorflowSpec(_, _, _, _, _, _, plan_matcher, _, _, _))
      .WillOnce(Return(std::move(plan_result_and_checkpoint_file)))
      .RetiresOnSaturation();
}

void FlRunnerTestBase::MockSuccessfulEligibilityPlanExecution(
    const TaskEligibilityInfo& task_eligibility_info) {
  engine::PlanResult plan_result(engine::PlanOutcome::kSuccess,
                                 absl::OkStatus());
  plan_result.example_stats.example_count = 5;
  plan_result.example_stats.example_size_bytes = 10;
  plan_result.task_eligibility_info = task_eligibility_info;
  EXPECT_CALL(
      *mock_tensorflow_runner_,
      RunEligibilityEvalPlanWithTensorflowSpec(_, _, _, _, _, _, _, _, _, _))
      .WillOnce(Return(std::move(plan_result)));
}

void FlRunnerTestBase::ExpectEligibilityCheckinCompletedLogEvent() {
  // If the flag is off, then the "before plan download" stats should not be
  // calculated, and instead simply equal the "before checkin" stats.
  NetworkStats expected_network_stats =
      kPostEligibilityCheckinNetworkStats -
      kPostEligibilityCheckinPlanUriReceivedNetworkStats;

  EXPECT_CALL(mock_phase_logger_,
              LogEligibilityEvalCheckinCompleted(
                  EqualsNetworkStats(expected_network_stats), _, _));
}

void FlRunnerTestBase::ExpectEligibilityEvalLogEvents() {
  EXPECT_CALL(mock_phase_logger_, LogEligibilityEvalCheckinStarted());
  EXPECT_CALL(mock_phase_logger_,
              SetModelIdentifier(kEligibilityEvalExecutionId));
  EXPECT_CALL(mock_phase_logger_,
              LogEligibilityEvalCheckinPlanUriReceived(_, _));

  ExpectEligibilityCheckinCompletedLogEvent();
  EXPECT_CALL(mock_phase_logger_, LogEligibilityEvalComputationStarted());
  EXPECT_CALL(mock_phase_logger_, LogEligibilityEvalComputationCompleted(
                                      FieldsAre(Eq(0), Eq(0)), _, _));
}

void FlRunnerTestBase::ExpectCheckinCompletedLogEvents() {
  EXPECT_CALL(mock_phase_logger_,
              LogCheckinPlanUriReceived(
                  kTaskName,
                  EqualsNetworkStats(kPostCheckinPlanUriReceivedNetworkStats -
                                     kPostEligibilityCheckinNetworkStats),
                  _));
  // If the flag is off, then the "before plan download" stats should not be
  // calculated, and instead simply equal the "before checkin" stats.
  NetworkStats expected_network_stats =
      kPostCheckinNetworkStats - kPostCheckinPlanUriReceivedNetworkStats;
  EXPECT_CALL(
      mock_phase_logger_,
      LogCheckinCompleted(kTaskName, EqualsNetworkStats(expected_network_stats),
                          _, _, _));
}

void FlRunnerTestBase::ExpectCheckinTrainingLogEvents(
    bool federated_select_enabled, bool has_min_sep_policy) {
  EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
  EXPECT_CALL(mock_phase_logger_, LogCheckinStarted());
  EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(kTaskName));
  ExpectCheckinCompletedLogEvents();
  EXPECT_CALL(mock_phase_logger_, LogComputationStarted(kTaskName));
  std::optional<int64_t> min_sep_policy_index =
      has_min_sep_policy ? std::make_optional(kMinSepPolicyCurrentIndex)
                         : std::nullopt;
  EXPECT_CALL(mock_phase_logger_,
              LogComputationCompleted(
                  FieldsAre(Gt(0), Gt(0)),
                  EqualsNetworkStats(federated_select_enabled
                                         ? kFederatedSelectNetworkStats
                                         : NetworkStats()),
                  _, _, min_sep_policy_index));
}

void FlRunnerTestBase::ExpectCheckinTrainingReportLogEvents(
    bool federated_select_enabled, bool has_min_sep_policy) {
  ExpectCheckinTrainingLogEvents(federated_select_enabled, has_min_sep_policy);
  EXPECT_CALL(mock_phase_logger_, LogResultUploadStarted())
      .WillOnce(Return(absl::OkStatus()));

  EXPECT_CALL(mock_phase_logger_,
              LogResultUploadCompleted(
                  EqualsNetworkStats(kPostReportCompletedNetworkStats -
                                     kPostCheckinNetworkStats),
                  _, _));
}

class FlRunnerImmediateAbortTest : public FlRunnerTestBase {
 public:
  FlRunnerImmediateAbortTest() : FlRunnerTestBase() {
    // We purposely do *not* call MockEligibilityEvalDisabled, to avoid
    // registering any expectations on FederatedProtocol's methods being
    // called, since we do not expect any such calls in the immediate abort
    // case.
  }
};

class FlRunnerHttpInvalidEntryUriTest : public FlRunnerTestBase {
 public:
  FlRunnerHttpInvalidEntryUriTest() : FlRunnerTestBase() {
    // We purposely do *not* call MockEligibilityEvalDisabled, to avoid
    // registering any expectations on FederatedProtocol's methods being
    // called, since we do not expect any such calls in the Http invalid entry
    // uri case.
  }

 protected:
  void TearDown() override {
    // Purposely skip the verification in FlRunnerTestBase's TearDown method.
  }
};

class FlRunnerTensorflowTaskTest : public FlRunnerTestBase {
 public:
  FlRunnerTensorflowTaskTest() : FlRunnerTestBase() {
    MockEligibilityEvalDisabled();
  }
};

class FlRunnerEligibilityEvalTest : public FlRunnerTestBase {
 public:
  explicit FlRunnerEligibilityEvalTest();

 protected:
  void SetUp() override;
  void SetUpEligibilityEvalTask();

  ComputationArtifacts eligibility_eval_artifacts_;
};

FlRunnerEligibilityEvalTest::FlRunnerEligibilityEvalTest()
    : FlRunnerTestBase() {
  EXPECT_CALL(mock_opstats_logger_, GetOpStatsDb())
      .WillRepeatedly(Return(&mock_opstats_db_));
  // Mock an empty OpStats DB.
  fcp::client::opstats::OpStatsSequence opstats_sequence;
  EXPECT_CALL(mock_opstats_db_, Read())
      .WillRepeatedly(Return(opstats_sequence));
}

void FlRunnerEligibilityEvalTest::SetUp() {
  ::testing::Test::SetUp();
  SetUpEligibilityEvalTask();
}

void FlRunnerEligibilityEvalTest::SetUpEligibilityEvalTask() {
  ClientOnlyPlan plan;
  plan.mutable_phase()
      ->mutable_tensorflow_spec()
      ->set_dataset_token_tensor_name("dataset_token");
  plan.mutable_phase()
      ->mutable_federated_compute_eligibility()
      ->set_task_eligibility_info_tensor_name("task_eligibility_info");
  eligibility_eval_artifacts_.plan = std::move(plan);
  eligibility_eval_artifacts_.checkpoint = kInitialCheckpoint;
  PopulationEligibilitySpec population_eligibility_spec;
  auto task_info = population_eligibility_spec.add_task_info();
  task_info->set_task_name(kTaskName);
  task_info->set_task_assignment_mode(
      PopulationEligibilitySpec::TaskInfo::TASK_ASSIGNMENT_MODE_SINGLE);
  task_info->add_eligibility_policy_indices(0);
  auto policy = population_eligibility_spec.add_eligibility_policies();
  policy->set_name("custom_tf_policy");
  *policy->mutable_tf_custom_policy()->mutable_arguments() = "tfl_policy_args";
  eligibility_eval_artifacts_.population_eligibility_spec =
      population_eligibility_spec;
}

class FlRunnerEligibilityEvalWithCriteriaTest : public FlRunnerTestBase {
 public:
  explicit FlRunnerEligibilityEvalWithCriteriaTest();

 protected:
  void SetUp() override;
  void SetUpEligibilityEvalTask();

  ComputationArtifacts eligibility_eval_artifacts_;
};

FlRunnerEligibilityEvalWithCriteriaTest::
    FlRunnerEligibilityEvalWithCriteriaTest()
    : FlRunnerTestBase() {
  EXPECT_CALL(mock_opstats_logger_, GetOpStatsDb())
      .WillRepeatedly(Return(&mock_opstats_db_));
  // Mock an empty OpStats DB.
  fcp::client::opstats::OpStatsSequence opstats_sequence;
  EXPECT_CALL(mock_opstats_db_, Read())
      .WillRepeatedly(Return(opstats_sequence));
}

void FlRunnerEligibilityEvalWithCriteriaTest::SetUp() {
  FlRunnerTestBase::SetUp();
  SetUpEligibilityEvalTask();
}

void FlRunnerEligibilityEvalWithCriteriaTest::SetUpEligibilityEvalTask() {
  PopulationEligibilitySpec population_eligibility_spec;
  auto task_info = population_eligibility_spec.add_task_info();
  task_info->set_task_name(kTaskName);
  task_info->set_task_assignment_mode(
      PopulationEligibilitySpec::TaskInfo::TASK_ASSIGNMENT_MODE_SINGLE);
  task_info->add_eligibility_policy_indices(0);
  auto policy = population_eligibility_spec.add_eligibility_policies();
  policy->set_name("min_2_example");
  auto data_availability_policy = policy->mutable_data_availability_policy();
  data_availability_policy->set_min_example_count(2);
  ExampleSelector example_selector;
  example_selector.set_collection_uri(
      std::string(kEligibilityEvalCollectionUri));
  google::internal::federated::plan::AverageOptions average_options;
  average_options.set_average_stat_name("count");
  example_selector.mutable_criteria()->PackFrom(average_options);
  *data_availability_policy->mutable_selector() = example_selector;
  eligibility_eval_artifacts_.population_eligibility_spec =
      population_eligibility_spec;

  Dataset dataset;
  auto client_data = dataset.add_client_data();
  client_data->set_client_id("client_1");
  auto selected_example = client_data->add_selected_example();
  *selected_example->mutable_selector() = example_selector;
  for (int i = 0; i < 3; i++) {
    selected_example->add_example(absl::StrCat("example_", i));
  }
  eligibility_eval_artifacts_.dataset = dataset;

  // Set up the mock example iterator for running the "eligibility eval" task
  // payload.
  EXPECT_CALL(mock_task_env_,
              CreateExampleIterator(EqualsProto(example_selector), _))
      // We use AtMost(1) because not all tests actually execute the task, so
      // we don't always expect the call to occur.
      .Times(AtMost(1))
      // The eligibility eval task's artifacts contain a multi-selector
      // Dataset, so we have specify exactly which example collection URI the
      // SimpleExampleIterator should return (as opposed to the regular task's
      // artifacts, which only contain data for a single, unnamed collection
      // URI).
      .WillOnce(DoAll(SaveArg<1>(&latest_eligibility_selector_context_),
                      Return(ByMove(std::make_unique<SimpleExampleIterator>(
                          eligibility_eval_artifacts_.dataset,
                          kEligibilityEvalCollectionUri)))));
}

class FlRunnerExampleQueryEligibilityEvalTest : public FlRunnerTestBase {
 public:
  explicit FlRunnerExampleQueryEligibilityEvalTest();

 protected:
  void SetUp() override;
  void SetUpEligibilityEvalTask();

  ComputationArtifacts eligibility_eval_artifacts_;
};

FlRunnerExampleQueryEligibilityEvalTest::
    FlRunnerExampleQueryEligibilityEvalTest()
    : FlRunnerTestBase() {
  EXPECT_CALL(mock_opstats_logger_, GetOpStatsDb())
      .WillRepeatedly(Return(&mock_opstats_db_));
  // Mock an empty OpStats DB.
  fcp::client::opstats::OpStatsSequence opstats_sequence;
  EXPECT_CALL(mock_opstats_db_, Read())
      .WillRepeatedly(Return(opstats_sequence));
}

void FlRunnerExampleQueryEligibilityEvalTest::SetUp() {
  FlRunnerTestBase::SetUp();
  SetUpEligibilityEvalTask();
}

void FlRunnerExampleQueryEligibilityEvalTest::SetUpEligibilityEvalTask() {
  PopulationEligibilitySpec population_eligibility_spec;
  auto task_info = population_eligibility_spec.add_task_info();
  task_info->set_task_name(kTaskName);
  task_info->set_task_assignment_mode(
      PopulationEligibilitySpec::TaskInfo::TASK_ASSIGNMENT_MODE_SINGLE);
  task_info->add_eligibility_policy_indices(0);
  auto policy = population_eligibility_spec.add_eligibility_policies();
  policy->set_name("min_5_example");
  auto data_availability_policy = policy->mutable_data_availability_policy();
  data_availability_policy->set_min_example_count(5);
  ExampleSelector example_selector;
  example_selector.set_collection_uri(
      std::string(kEligibilityEvalCollectionUri));
  google::internal::federated::plan::AverageOptions average_options;
  average_options.set_average_stat_name("count");
  example_selector.mutable_criteria()->PackFrom(average_options);
  *data_availability_policy->mutable_selector() = example_selector;
  data_availability_policy->set_use_example_query_result_format(true);
  eligibility_eval_artifacts_.population_eligibility_spec =
      population_eligibility_spec;

  ExampleQueryResult example_query_result;
  example_query_result.mutable_stats()->set_output_rows_count(5);
  std::string example = example_query_result.SerializeAsString();

  Dataset dataset;
  Dataset::ClientDataset client_dataset;
  client_dataset.set_client_id("client_id");
  auto selected_example = client_dataset.add_selected_example();
  *selected_example->mutable_selector() = example_selector;
  selected_example->add_example(example);
  dataset.mutable_client_data()->Add(std::move(client_dataset));
  eligibility_eval_artifacts_.dataset = dataset;

  // Set up the mock example iterator for running the "eligibility eval" task
  // payload.
  EXPECT_CALL(mock_task_env_,
              CreateExampleIterator(EqualsProto(example_selector), _))
      // We use AtMost(1) because not all tests actually execute the task, so
      // we don't always expect the call to occur.
      .Times(AtMost(1))
      // The eligibility eval task's artifacts contain a multi-selector
      // Dataset, so we have specify exactly which example collection URI the
      // SimpleExampleIterator should return (as opposed to the regular task's
      // artifacts, which only contain data for a single, unnamed collection
      // URI).
      .WillOnce(DoAll(SaveArg<1>(&latest_eligibility_selector_context_),
                      Return(ByMove(std::make_unique<SimpleExampleIterator>(
                          dataset, kEligibilityEvalCollectionUri)))));
}

class FlRunnerExampleQueryTest : public FlRunnerTestBase {
 public:
  FlRunnerExampleQueryTest() : FlRunnerTestBase() {
    MockEligibilityEvalDisabled();
  }

 protected:
  void SetUp() override;
  void SetUpDirectDataUploadTask();
  void SetUpExampleQueryWithEventTimeRange();
  void ExpectExampleQueryCheckinTrainingLogEvents();
  void ExpectExampleQueryCheckinTrainingReportLogEvents();
  void ExpectComputationFailureWithInvalidArgument();

  Dataset dataset_;
  ExampleQueryResult example_query_result_;
  std::optional<PayloadMetadata> payload_metadata_;
};

void FlRunnerExampleQueryTest::SetUp() {
  FlRunnerTestBase::SetUp();

  ExampleQuerySpec::OutputVectorSpec string_vector_spec;
  string_vector_spec.set_vector_name("string_vector");
  string_vector_spec.set_data_type(DataType::STRING);
  ExampleQuerySpec::OutputVectorSpec int_vector_spec;
  int_vector_spec.set_vector_name("int_vector");
  int_vector_spec.set_data_type(DataType::INT64);

  ExampleQuerySpec::ExampleQuery example_query;
  example_query.mutable_example_selector()->set_collection_uri(
      "app:/test_collection");
  (*example_query.mutable_output_vector_specs())["string_tensor"] =
      string_vector_spec;
  (*example_query.mutable_output_vector_specs())["int_tensor"] =
      int_vector_spec;
  single_task_assignment_client_only_plan_.mutable_phase()
      ->mutable_example_query_spec()
      ->mutable_example_queries()
      ->Add(std::move(example_query));

  AggregationConfig aggregation_config;
  aggregation_config.mutable_tf_v1_checkpoint_aggregation();
  (*single_task_assignment_client_only_plan_.mutable_phase()
        ->mutable_federated_example_query()
        ->mutable_aggregations())["string_tensor"] = aggregation_config;
  (*single_task_assignment_client_only_plan_.mutable_phase()
        ->mutable_federated_example_query()
        ->mutable_aggregations())["int_tensor"] = aggregation_config;

  ExampleQueryResult::VectorData::Values int_values;
  int_values.mutable_int64_values()->add_value(42);
  int_values.mutable_int64_values()->add_value(24);
  ExampleQueryResult::VectorData::Values string_values;
  string_values.mutable_string_values()->add_value("value1");
  string_values.mutable_string_values()->add_value("value2");

  (*example_query_result_.mutable_vector_data()
        ->mutable_vectors())["int_vector"] = int_values;
  (*example_query_result_.mutable_vector_data()
        ->mutable_vectors())["string_vector"] = string_values;
  std::string example = example_query_result_.SerializeAsString();

  dataset_.clear_client_data();
  Dataset::ClientDataset client_dataset;
  client_dataset.set_client_id("client_id");
  client_dataset.add_example(example);
  dataset_.mutable_client_data()->Add(std::move(client_dataset));

  // Set up the mock example iterator for running the "regular" task payload.
  EXPECT_CALL(mock_task_env_,
              CreateExampleIterator(
                  EqualsProto("collection_uri: 'app:/test_collection'"), _))
      // We use AtMost(1) because not all tests actually execute the task, so
      // we don't always expect the call to occur. This use of AtMost(1) also
      // allows tests to still 'override' this default behavior (e.g. the
      // abort test).
      .Times(AtMost(1))
      .WillOnce(DoAll(
          SaveArg<1>(&latest_selector_context_),
          Return(ByMove(std::make_unique<SimpleExampleIterator>(dataset_)))));
}

void FlRunnerExampleQueryTest::SetUpExampleQueryWithEventTimeRange() {
  EventTimeRange event_time_range;
  event_time_range.mutable_start_event_time()->set_year(2025);
  event_time_range.mutable_start_event_time()->set_month(1);
  event_time_range.mutable_start_event_time()->set_day(1);
  event_time_range.mutable_end_event_time()->set_year(2025);
  event_time_range.mutable_end_event_time()->set_month(1);
  event_time_range.mutable_end_event_time()->set_day(7);
  example_query_result_.mutable_stats()->mutable_event_time_range()->insert(
      {"query_name", event_time_range});
  std::string example = example_query_result_.SerializeAsString();

  PayloadMetadata payload_metadata;
  *payload_metadata.mutable_event_time_range() = event_time_range;
  payload_metadata_ = payload_metadata;

  dataset_.clear_client_data();
  Dataset::ClientDataset client_dataset;
  client_dataset.set_client_id("client_id");
  client_dataset.add_example(example);
  dataset_.mutable_client_data()->Add(std::move(client_dataset));

  // Set up the mock example iterator for running the "regular" task payload.
  EXPECT_CALL(mock_task_env_,
              CreateExampleIterator(
                  EqualsProto("collection_uri: 'app:/test_collection'"), _))
      // We use AtMost(1) because not all tests actually execute the task, so
      // we don't always expect the call to occur. This use of AtMost(1) also
      // allows tests to still 'override' this default behavior (e.g. the
      // abort test).
      .Times(AtMost(1))
      .WillOnce(DoAll(
          SaveArg<1>(&latest_selector_context_),
          Return(ByMove(std::make_unique<SimpleExampleIterator>(dataset_)))));
}

void FlRunnerExampleQueryTest::SetUpDirectDataUploadTask() {
  single_task_assignment_client_only_plan_.mutable_phase()
      ->clear_example_query_spec();
  single_task_assignment_client_only_plan_.mutable_phase()
      ->clear_federated_example_query();

  ExampleQuerySpec::ExampleQuery example_query;
  example_query.mutable_example_selector()->set_collection_uri(
      "app:/test_collection");
  example_query.set_direct_output_tensor_name("data_upload_tensor");
  single_task_assignment_client_only_plan_.mutable_phase()
      ->mutable_example_query_spec()
      ->mutable_example_queries()
      ->Add(std::move(example_query));

  AggregationConfig aggregation_config;
  aggregation_config.mutable_federated_compute_checkpoint_aggregation();
  (*single_task_assignment_client_only_plan_.mutable_phase()
        ->mutable_federated_example_query()
        ->mutable_aggregations())["data_upload_tensor"] = aggregation_config;

  tensorflow::Example example;
  (*example.mutable_features()->mutable_feature())["col1"]
      .mutable_int64_list()
      ->add_value(1);

  dataset_.clear_client_data();
  Dataset::ClientDataset client_dataset;
  client_dataset.set_client_id("client_id");
  client_dataset.add_example(example.SerializeAsString());
  dataset_.mutable_client_data()->Add(std::move(client_dataset));

  // Set up the mock example iterator for running the "regular" task payload.
  EXPECT_CALL(mock_task_env_,
              CreateExampleIterator(
                  EqualsProto("collection_uri: 'app:/test_collection'"), _))
      // We use AtMost(1) because not all tests actually execute the task, so
      // we don't always expect the call to occur. This use of AtMost(1) also
      // allows tests to still 'override' this default behavior (e.g. the
      // abort test).
      .Times(AtMost(1))
      .WillOnce(DoAll(
          SaveArg<1>(&latest_selector_context_),
          Return(ByMove(std::make_unique<SimpleExampleIterator>(dataset_)))));
}

void FlRunnerExampleQueryTest::ExpectExampleQueryCheckinTrainingLogEvents() {
  InSequence seq;
  EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
  EXPECT_CALL(mock_phase_logger_, LogCheckinStarted());
  EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(kTaskName));
  ExpectCheckinCompletedLogEvents();
  EXPECT_CALL(mock_phase_logger_, LogComputationStarted(kTaskName));
  EXPECT_CALL(mock_phase_logger_, MaybeLogCollectionFirstAccessTime(_));
  EXPECT_CALL(mock_opstats_logger_,
              UpdateDatasetStats("app:/test_collection",
                                 /*additional_example_count=*/Gt(0),
                                 /*additional_example_size_bytes=*/Gt(0)));

  EXPECT_CALL(
      mock_phase_logger_,
      LogComputationCompleted(FieldsAre(Ge(0), Ge(0)),
                              EqualsNetworkStats(NetworkStats()), _, _, _));
}

void FlRunnerExampleQueryTest::
    ExpectExampleQueryCheckinTrainingReportLogEvents() {
  ExpectExampleQueryCheckinTrainingLogEvents();
  EXPECT_CALL(mock_phase_logger_, LogResultUploadStarted())
      .WillOnce(Return(absl::OkStatus()));

  EXPECT_CALL(mock_phase_logger_,
              LogResultUploadCompleted(
                  EqualsNetworkStats(kPostReportCompletedNetworkStats -
                                     kPostCheckinNetworkStats),
                  _, _));
}

void FlRunnerExampleQueryTest::ExpectComputationFailureWithInvalidArgument() {
  EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
  EXPECT_CALL(mock_phase_logger_, LogCheckinStarted());
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_client_only_plan_.SerializeAsString(), ""},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));
  EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(kTaskName));
  ExpectCheckinCompletedLogEvents();
  EXPECT_CALL(mock_phase_logger_, LogComputationStarted(kTaskName));
  EXPECT_CALL(mock_phase_logger_, LogComputationInvalidArgument(_, _, _, _));
  EXPECT_CALL(mock_phase_logger_, LogFailureUploadStarted())
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(mock_federated_protocol_,
              MockReportNotCompleted(engine::PhaseOutcome::ERROR, _, _))
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(mock_phase_logger_,
              LogFailureUploadCompleted(
                  EqualsNetworkStats(kPostReportNotCompletedNetworkStats -
                                     kPostCheckinNetworkStats),
                  _, _));
  EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultFailure()))
      .WillOnce(Return(true));

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  EXPECT_EQ(result->contribution_result(), FLRunnerResult::FAIL);
}

class FlRunnerMultipleTaskAssignmentsTest : public FlRunnerEligibilityEvalTest {
 public:
  FlRunnerMultipleTaskAssignmentsTest() : FlRunnerEligibilityEvalTest() {
    ON_CALL(mock_fedselect_manager_, CreateExampleIteratorFactoryForUriTemplate(
                                         kAnotherFederatedSelectUriTemplate))
        .WillByDefault([](absl::string_view uri_template) {
          return std::make_unique<MockFederatedSelectExampleIteratorFactory>();
        });
  }

 protected:
  void SetUp() override;
  void SetUpPopulationEligibilitySpec();
  void ExpectEventsForEligibilityEvalComputationWithThreeEligibleTasks();
  void MockEligibilityEvalCheckIn();
  FederatedProtocol::TaskAssignment task_assignment_1_;
  FederatedProtocol::TaskAssignment task_assignment_2_;
  ClientOnlyPlan plan_1_;
  ClientOnlyPlan plan_2_;
};

void FlRunnerMultipleTaskAssignmentsTest::SetUp() {
  FlRunnerTestBase::SetUp();
  task_assignment_1_.federated_select_uri_template =
      kAnotherFederatedSelectUriTemplate;
  task_assignment_1_.aggregation_session_id =
      kMultipleTaskAggregationSessionId1;
  task_assignment_1_.sec_agg_info = kSecAggInfoForMixedSecAgg;
  task_assignment_1_.task_name = kSwor24HourTaskName;
  task_assignment_1_.task_identifier = kTaskIdentifier1;
  task_assignment_1_.payloads.checkpoint = std::string(kInitialCheckpoint);
  *plan_1_.mutable_phase()
       ->mutable_tensorflow_spec()
       ->mutable_dataset_token_tensor_name() = "plan_1_dataset_token";
  task_assignment_1_.payloads.plan = plan_1_.SerializeAsString();

  // Set up the second task assignment.
  task_assignment_2_.federated_select_uri_template =
      kAnotherFederatedSelectUriTemplate;
  task_assignment_2_.aggregation_session_id =
      kMultipleTaskAggregationSessionId2;
  task_assignment_2_.sec_agg_info = kSecAggInfoForPureSecAggTask;
  task_assignment_2_.task_name = kRequires5ExamplesTaskName;
  task_assignment_2_.task_identifier = kTaskIdentifier2;
  task_assignment_2_.payloads.checkpoint = std::string(kInitialCheckpoint);
  *plan_2_.mutable_phase()
       ->mutable_tensorflow_spec()
       ->mutable_dataset_token_tensor_name() = "plan_2_dataset_token";
  task_assignment_2_.payloads.plan = plan_2_.SerializeAsString();
}

void FlRunnerMultipleTaskAssignmentsTest::SetUpPopulationEligibilitySpec() {
  PopulationEligibilitySpec population_eligibility_spec;
  auto task_info = population_eligibility_spec.add_task_info();
  task_info->set_task_name(kTaskName);
  task_info->set_task_assignment_mode(
      PopulationEligibilitySpec::TaskInfo::TASK_ASSIGNMENT_MODE_SINGLE);
  task_info->add_eligibility_policy_indices(0);
  auto policy = population_eligibility_spec.add_eligibility_policies();
  policy->set_name("custom_tf_policy");
  *policy->mutable_tf_custom_policy()->mutable_arguments() = "tfl_policy_args";
  auto mta_task_info_1 = population_eligibility_spec.add_task_info();
  mta_task_info_1->set_task_name(std::string(kSwor24HourTaskName));
  mta_task_info_1->set_task_assignment_mode(
      PopulationEligibilitySpec::TaskInfo::TASK_ASSIGNMENT_MODE_MULTIPLE);
  mta_task_info_1->add_eligibility_policy_indices(0);
  auto mta_task_info_2 = population_eligibility_spec.add_task_info();
  mta_task_info_2->set_task_name(std::string(kRequires5ExamplesTaskName));
  mta_task_info_2->set_task_assignment_mode(
      PopulationEligibilitySpec::TaskInfo::TASK_ASSIGNMENT_MODE_MULTIPLE);
  mta_task_info_2->add_eligibility_policy_indices(0);
  eligibility_eval_artifacts_.population_eligibility_spec =
      population_eligibility_spec;
}

// Setup the expectations for running the eligibility task which returns 3
// eligible tasks.
void FlRunnerMultipleTaskAssignmentsTest::
    ExpectEventsForEligibilityEvalComputationWithThreeEligibleTasks() {
  EXPECT_CALL(mock_phase_logger_, LogEligibilityEvalCheckinStarted());
  EXPECT_CALL(mock_phase_logger_,
              SetModelIdentifier(kEligibilityEvalExecutionId));
  EXPECT_CALL(mock_phase_logger_,
              LogEligibilityEvalCheckinPlanUriReceived(_, _));

  ExpectEligibilityCheckinCompletedLogEvent();
  EXPECT_CALL(mock_phase_logger_, LogEligibilityEvalComputationStarted());
  EXPECT_CALL(mock_phase_logger_, LogEligibilityEvalComputationCompleted(
                                      FieldsAre(Eq(0), Eq(0)), _, _));
}

void FlRunnerMultipleTaskAssignmentsTest::MockEligibilityEvalCheckIn() {
  // Mock a successful eligibility eval checkin.

  // Get the PopulationEligibilitySpec from the task artifacts, and make all of
  // the tasks that use policies TASK_ASSIGNMENT_MODE_MULTIPLE
  PopulationEligibilitySpec population_spec =
      eligibility_eval_artifacts_.population_eligibility_spec;
  for (auto& t : *population_spec.mutable_task_info()) {
    if (t.task_name() == kSwor24HourTaskName ||
        t.task_name() == kRequires5ExamplesTaskName) {
      t.set_task_assignment_mode(
          PopulationEligibilitySpec::TaskInfo::TASK_ASSIGNMENT_MODE_MULTIPLE);
    }
  }

  EXPECT_CALL(mock_federated_protocol_, MockEligibilityEvalCheckin())
      .WillOnce(Return(FederatedProtocol::EligibilityEvalTask{
          {eligibility_eval_artifacts_.plan.SerializeAsString(),
           eligibility_eval_artifacts_.checkpoint},
          kEligibilityEvalExecutionId,
          population_spec}));
}
class FlRunnerSourceIdSeedTest : public FlRunnerTestBase {
 public:
  FlRunnerSourceIdSeedTest() : FlRunnerTestBase() {
    EXPECT_CALL(mock_opstats_logger_, GetOpStatsDb())
        .WillRepeatedly(Return(&mock_opstats_db_));
    MockEligibilityEvalDisabled();
    // Mock the protocol flow. Have the regular check-in get rejected. This is
    // enough to trigger GetOrCreateSourceIdSeed and then exit cleanly.
    EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
        .WillOnce(Return(FederatedProtocol::Rejection{}));

    // Mock the expected logging calls for this flow.
    EXPECT_CALL(mock_phase_logger_,
                SetModelIdentifier(/*model_identifier=*/""));
    EXPECT_CALL(mock_phase_logger_, LogCheckinStarted());
    EXPECT_CALL(mock_phase_logger_, LogCheckinTurnedAway(_, _, _));

    // Enable the flag to generate a source ID seed.
    EXPECT_CALL(mock_flags_, enable_privacy_id_generation())
        .WillRepeatedly(Return(true));
  }
};

TEST_F(FlRunnerSourceIdSeedTest, GeneratesSourceIdSeedWhenNoneExists) {
  // Setup OpStats to simulate no pre-existing seed.
  fcp::client::opstats::OpStatsSequence empty_sequence;
  EXPECT_CALL(mock_opstats_db_, Read()).WillOnce(Return(empty_sequence));
  std::function<void(opstats::OpStatsSequence&)> transform_fn;
  EXPECT_CALL(mock_opstats_db_, Transform(_))
      .WillOnce(DoAll(SaveArg<0>(&transform_fn), Return(absl::OkStatus())));

  ASSERT_OK(RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_));

  opstats::OpStatsSequence sequence_to_transform;
  transform_fn(sequence_to_transform);
  ASSERT_TRUE(sequence_to_transform.has_source_id_seed());
  EXPECT_FALSE(sequence_to_transform.source_id_seed().salt().empty());
}

TEST_F(FlRunnerSourceIdSeedTest, UsesExistingSourceIdSeed) {
  // Setup OpStats to simulate a pre-existing seed.
  fcp::client::opstats::OpStatsSequence sequence_with_seed;
  const std::string existing_salt = "existing_salt";
  sequence_with_seed.mutable_source_id_seed()->set_salt(existing_salt);
  EXPECT_CALL(mock_opstats_db_, Read()).WillOnce(Return(sequence_with_seed));

  // Expect that Transform is NOT called to create a new seed. The OpStatsLogger
  // dtor will still commit, so we expect at most one call.
  EXPECT_CALL(mock_opstats_db_, Transform(_)).Times(AtMost(1));

  ASSERT_OK(RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_));
}

TEST_F(FlRunnerImmediateAbortTest, ImmediateAbort) {
  EXPECT_CALL(mock_phase_logger_, LogTaskNotStarted(_));

  // Make RunFederatedComputation abort itself ASAP.
  training_conditions_satisfied_.store(false);

  // Call RunFederatedComputation, and check that it returns a non-empty
  // RetryWindow (which it should've received from
  // FederatedProtocol::GetLatestRetryWindow()).
  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::FAIL);
  EXPECT_THAT(*result, EqualsProto(expected_result));
}

TEST_F(FlRunnerHttpInvalidEntryUriTest, HttpProtocolWithInvalidEntryUri) {
  EXPECT_CALL(mock_task_env_, GetBaseDir()).WillOnce(Return(""));
  ASSERT_THAT(RunFederatedComputation(
                  &mock_task_env_, &mock_event_publisher_, &files_impl_,
                  &mock_log_manager_, &mock_flags_, "http://invalid", "api_key",
                  "test_cert_path", kSessionName, kPopulationName,
                  "retry_token", "client_version", "attestation_measurement"),
              IsCode(absl::StatusCode::kInvalidArgument));
}

TEST_F(FlRunnerTensorflowTaskTest, MockCheckinFails) {
  // Make the Checkin(...) method fail with a network error.
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(absl::UnavailableError("foo")));
  EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
  EXPECT_CALL(mock_phase_logger_, LogCheckinStarted());
  EXPECT_CALL(mock_phase_logger_, LogCheckinIOError(_, _, _, _));

  // Even though the Checkin(...) method fails with a network error, we expect
  // an FLRunnerResult to be returned with the most recent RetryWindow, when
  // the flag above is on.
  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::FAIL);
  EXPECT_THAT(*result, EqualsProto(expected_result));
}

// Test the case where the protocol indicates a rejection.
TEST_F(FlRunnerTensorflowTaskTest, RejectionTest) {
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::Rejection{}));

  EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
  EXPECT_CALL(mock_phase_logger_, LogCheckinStarted());
  EXPECT_CALL(mock_phase_logger_, LogCheckinTurnedAway(_, _, _));

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::FAIL);
  EXPECT_THAT(*result, EqualsProto(expected_result));
}

TEST_F(FlRunnerTensorflowTaskTest, SimpleAggregationPlan) {
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));

  ComputationResults computation_results;
  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        computation_results = std::move(reported_results);
        return absl::OkStatus();
      });
  {
    InSequence seq;
    ExpectCheckinTrainingReportLogEvents();
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true));
  }
  MockSuccessfulPlanExecution(/*has_checkpoint=*/true,
                              /*has_secagg_output=*/false);

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::SUCCESS);
  expected_result.mutable_contributed_task_names()->Add(kTaskName);
  EXPECT_THAT(*result, EqualsProto(expected_result));
  // There should be one checkpoint and no secagg results.
  EXPECT_THAT(computation_results,
              ElementsAre(Pair(kTensorflowCheckpointAggregand,
                               VariantWith<TFCheckpoint>(Not(IsEmpty())))));
}

TEST_F(FlRunnerTensorflowTaskTest, SimpleAggregationPlanWithMinSepPolicy) {
  single_task_assignment_artifacts_.plan.mutable_client_persisted_data()
      ->set_min_sep_policy_index(kMinSepPolicyCurrentIndex);

  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));

  ComputationResults computation_results;
  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        computation_results = std::move(reported_results);
        return absl::OkStatus();
      });
  {
    InSequence seq;
    // Assert that the `min_sep_policy_index` is logged.
    ExpectCheckinTrainingReportLogEvents(/*federated_select_enabled=*/false,
                                         /*has_min_sep_policy=*/true);
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true));
  }

  MockSuccessfulPlanExecution(/*has_checkpoint=*/true,
                              /*has_secagg_output=*/false);

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::SUCCESS);
  expected_result.mutable_contributed_task_names()->Add(kTaskName);
  EXPECT_THAT(*result, EqualsProto(expected_result));
  // There should be one checkpoint and no secagg results.
  EXPECT_THAT(computation_results,
              ElementsAre(Pair(kTensorflowCheckpointAggregand,
                               VariantWith<TFCheckpoint>(Not(IsEmpty())))));
}

TEST_F(FlRunnerTensorflowTaskTest,
       SimpleAggregationPlanWithTaskResourcesAsAbslCord) {
  // We return the task resources as absl::Cords instead of std::strings, to
  // exercise the absl::Cord-specific code paths.
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {absl::Cord(
               single_task_assignment_artifacts_.plan.SerializeAsString()),
           absl::Cord(single_task_assignment_artifacts_.checkpoint)},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));

  ComputationResults computation_results;
  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        computation_results = std::move(reported_results);
        return absl::OkStatus();
      });

  {
    InSequence seq;
    ExpectCheckinTrainingReportLogEvents();
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true));
  }

  MockSuccessfulPlanExecution(/*has_checkpoint=*/true,
                              /*has_secagg_output=*/false);

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::SUCCESS);
  expected_result.add_contributed_task_names(kTaskName);
  EXPECT_THAT(*result, EqualsProto(expected_result));
  // There should be one checkpoint and no secagg results.
  EXPECT_THAT(computation_results,
              ElementsAre(Pair(kTensorflowCheckpointAggregand,
                               VariantWith<TFCheckpoint>(Not(IsEmpty())))));
}

// Checks that when reporting fails with an ABORTED protocol error, the most
// recent retry window is still used.
TEST_F(FlRunnerTensorflowTaskTest,
       SimpleAggregationPlanWithReportAbortedErrorReturnsRetryWindow) {
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));

  // Make the call to ReportCompleted(...) fail with an ABORTED error.
  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce(Return(absl::AbortedError("foo")));

  {
    InSequence seq;
    ExpectCheckinTrainingLogEvents();
    EXPECT_CALL(mock_phase_logger_, LogResultUploadStarted())
        .WillOnce(Return(absl::OkStatus()));
    EXPECT_CALL(mock_phase_logger_, LogResultUploadServerAborted(_, _, _, _));
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultFailure()))
        .WillOnce(Return(true));
  }

  MockSuccessfulPlanExecution(/*has_checkpoint=*/true,
                              /*has_secagg_output=*/false);

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  // Even though the ReportCompleted(...) call fails, it should result in an
  // FLRunnerResult with the most recent RetryWindow.
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::FAIL);
  EXPECT_THAT(*result, EqualsProto(expected_result));
}

// Checks that when reporting fails with an UNAVAILABLE protocol error the
// most recent retry window is still returned and the error is logged (i.e.
// the new behavior).
TEST_F(FlRunnerTensorflowTaskTest,
       SimpleAggregationPlanWithReportUnavailableErrorReturnsRetryWindow) {
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));

  // Make the call to ReportCompleted(...) fail with an UNAVAILABLE error.
  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce(Return(absl::UnavailableError("foo")));

  {
    InSequence seq;
    ExpectCheckinTrainingLogEvents();
    EXPECT_CALL(mock_phase_logger_, LogResultUploadStarted())
        .WillOnce(Return(absl::OkStatus()));
    EXPECT_CALL(mock_phase_logger_, LogResultUploadIOError(_, _, _, _));
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultFailure()))
        .WillOnce(Return(true));
  }

  MockSuccessfulPlanExecution(/*has_checkpoint=*/true,
                              /*has_secagg_output=*/false);

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  // Even though the ReportCompleted(...) call fails, it should result in an
  // FLRunnerResult with the most recent RetryWindow. This is the new
  // behavior.
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::FAIL);
  EXPECT_THAT(*result, EqualsProto(expected_result));
}

TEST_F(FlRunnerTensorflowTaskTest, TfPlanLightweightComputationIdNull) {
  EXPECT_CALL(mock_flags_, enable_computation_id())
      .WillRepeatedly(Return(false));

  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));

  ComputationResults computation_results;
  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        computation_results = std::move(reported_results);
        return absl::OkStatus();
      });

  EXPECT_CALL(mock_phase_logger_, LogComputationCompleted(_, _, _, _, _))
      .Times(0);

  {
    InSequence seq;
    ExpectCheckinTrainingReportLogEvents();
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true));
  }

  MockSuccessfulPlanExecution(/*has_checkpoint=*/true,
                              /*has_secagg_output=*/false);

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);

  // Computation id should not be calculated for TF based tasks if
  // enable_computation_id is false.
  EXPECT_TRUE(latest_selector_context_.computation_properties()
                  .federated()
                  .computation_id()
                  .empty());
}

TEST_F(FlRunnerTensorflowTaskTest, SecaggPlan) {
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          FederatedProtocol::SecAggInfo{
              .expected_number_of_clients = 4,
              .minimum_clients_in_server_visible_aggregate = 3},
          std::nullopt,
          kTaskName}));

  ComputationResults computation_results;
  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        computation_results = std::move(reported_results);
        return absl::OkStatus();
      });

  {
    InSequence seq;
    ExpectCheckinTrainingReportLogEvents();
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true));
  }

  MockSuccessfulPlanExecution(/*has_checkpoint=*/true,
                              /*has_secagg_output=*/true);

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::SUCCESS);
  expected_result.mutable_contributed_task_names()->Add(kTaskName);
  EXPECT_THAT(*result, EqualsProto(expected_result));

  // There should be one checkpoint and one secagg tensor.
  EXPECT_EQ(computation_results.size(), 2);

  // Check that the checkpoint was populated.
  EXPECT_THAT(computation_results,
              Contains(Pair(kTensorflowCheckpointAggregand,
                            VariantWith<TFCheckpoint>(Not(IsEmpty())))));
}

TEST_F(FlRunnerTensorflowTaskTest, SecaggPlanOnlySecaggOutputTensors) {
  FederatedProtocol::SecAggInfo sec_agg_info{
      .expected_number_of_clients = 3,
      .minimum_clients_in_server_visible_aggregate = 2};
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          sec_agg_info,
          std::nullopt,
          kTaskName}));

  ComputationResults computation_results;
  std::vector<std::pair<std::string, double>> stats;
  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        computation_results = std::move(reported_results);
        return absl::OkStatus();
      });

  {
    InSequence seq;
    ExpectCheckinTrainingReportLogEvents();
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true));
  }

  MockSuccessfulPlanExecution(/*has_checkpoint=*/false,
                              /*has_secagg_output=*/true);

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::SUCCESS);
  expected_result.mutable_contributed_task_names()->Add(kTaskName);
  EXPECT_THAT(*result, EqualsProto(expected_result));

  // There should be one secagg tensor.
  EXPECT_EQ(computation_results.size(), 1);
}

TEST_F(FlRunnerTensorflowTaskTest, AbortPlan) {
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));
  engine::PlanResult plan_result(engine::PlanOutcome::kInterrupted,
                                 absl::AbortedError("Aborted"));
  PlanResultAndCheckpointFile plan_result_and_checkpoint_file(
      std::move(plan_result));
  EXPECT_CALL(*mock_tensorflow_runner_,
              RunPlanWithTensorflowSpec(_, _, _, _, _, _, _, _, _, _))
      .WillOnce(Return(std::move(plan_result_and_checkpoint_file)));

  {
    InSequence seq;
    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
    EXPECT_CALL(mock_phase_logger_, LogCheckinStarted());
    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(kTaskName));
    ExpectCheckinCompletedLogEvents();
    EXPECT_CALL(mock_phase_logger_, LogComputationStarted(kTaskName));
    EXPECT_CALL(mock_phase_logger_, LogComputationInterrupted(_, _, _, _, _));
    EXPECT_CALL(mock_phase_logger_, LogFailureUploadStarted())
        .WillOnce(Return(absl::OkStatus()));
    EXPECT_CALL(mock_phase_logger_,
                LogFailureUploadCompleted(
                    EqualsNetworkStats(kPostReportNotCompletedNetworkStats -
                                       kPostCheckinNetworkStats),
                    _, _));
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultFailure()))
        .WillOnce(Return(true));
  }

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);

  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::FAIL);
  EXPECT_THAT(*result, EqualsProto(expected_result));
}

TEST_F(FlRunnerTensorflowTaskTest, ExampleIteratorError) {
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));
  engine::PlanResult plan_result(engine::PlanOutcome::kExampleIteratorError,
                                 absl::InternalError("Example iterator error"));
  PlanResultAndCheckpointFile plan_result_and_checkpoint_file(
      std::move(plan_result));
  EXPECT_CALL(*mock_tensorflow_runner_,
              RunPlanWithTensorflowSpec(_, _, _, _, _, _, _, _, _, _))
      .WillOnce(Return(std::move(plan_result_and_checkpoint_file)));

  {
    InSequence seq;
    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
    EXPECT_CALL(mock_phase_logger_, LogCheckinStarted());
    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(kTaskName));
    ExpectCheckinCompletedLogEvents();
    EXPECT_CALL(mock_phase_logger_, LogComputationStarted(kTaskName));
    EXPECT_CALL(mock_phase_logger_,
                LogComputationExampleIteratorError(_, _, _, _));
    EXPECT_CALL(mock_phase_logger_, LogFailureUploadStarted())
        .WillOnce(Return(absl::OkStatus()));
    EXPECT_CALL(mock_phase_logger_,
                LogFailureUploadCompleted(
                    EqualsNetworkStats(kPostReportNotCompletedNetworkStats -
                                       kPostCheckinNetworkStats),
                    _, _));
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultFailure()))
        .WillOnce(Return(true));
  }

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);

  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::FAIL);
  EXPECT_THAT(*result, EqualsProto(expected_result));
}

TEST_F(FlRunnerTensorflowTaskTest, ComputationInvalidArgument) {
  EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
  EXPECT_CALL(mock_phase_logger_, LogCheckinStarted());
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));
  EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(kTaskName));
  ExpectCheckinCompletedLogEvents();
  EXPECT_CALL(mock_phase_logger_, LogComputationStarted(kTaskName));
  EXPECT_CALL(mock_phase_logger_, LogComputationInvalidArgument(_, _, _, _));
  EXPECT_CALL(mock_phase_logger_, LogFailureUploadStarted())
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(mock_federated_protocol_,
              MockReportNotCompleted(engine::PhaseOutcome::ERROR, _, _))
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(mock_phase_logger_,
              LogFailureUploadCompleted(
                  EqualsNetworkStats(kPostReportNotCompletedNetworkStats -
                                     kPostCheckinNetworkStats),
                  _, _));
  EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultFailure()))
      .WillOnce(Return(true));

  engine::PlanResult plan_result(engine::PlanOutcome::kInvalidArgument,
                                 absl::InvalidArgumentError("Invalid plan."));
  PlanResultAndCheckpointFile plan_result_and_checkpoint_file(
      std::move(plan_result));
  EXPECT_CALL(*mock_tensorflow_runner_,
              RunPlanWithTensorflowSpec(_, _, _, _, _, _, _, _, _, _))
      .WillOnce(Return(std::move(plan_result_and_checkpoint_file)));

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  EXPECT_EQ(result->contribution_result(), FLRunnerResult::FAIL);
}

TEST_F(FlRunnerTensorflowTaskTest, MockCheckinInvalidPlan) {
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {"im_not_a_plan", single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));

  {
    InSequence seq;
    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
    EXPECT_CALL(mock_phase_logger_, LogCheckinStarted());
    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(kTaskName));
    EXPECT_CALL(mock_phase_logger_, LogCheckinPlanUriReceived(_, _, _));
    EXPECT_CALL(mock_phase_logger_, LogCheckinInvalidPayload(_, _, _, _));
  }

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  EXPECT_EQ(result->contribution_result(), FLRunnerResult::FAIL);
}

TEST_F(FlRunnerTensorflowTaskTest, TaskCompletionCallbackEnabledUploadFailed) {
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));

  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce(Return(absl::InternalError("Something's wrong.")));
  EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultFailure()));

  {
    InSequence seq;
    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
    EXPECT_CALL(mock_phase_logger_, LogCheckinStarted());
    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(kTaskName));
    ExpectCheckinCompletedLogEvents();
    EXPECT_CALL(mock_phase_logger_, LogComputationStarted(kTaskName));
    EXPECT_CALL(mock_phase_logger_, LogComputationCompleted(_, _, _, _, _));
    EXPECT_CALL(mock_phase_logger_, LogResultUploadStarted())
        .WillOnce(Return(absl::OkStatus()));
    EXPECT_CALL(mock_phase_logger_, LogResultUploadIOError(_, _, _, _));
  }

  MockSuccessfulPlanExecution(/*has_checkpoint=*/true,
                              /*has_secagg_output=*/false);

  ASSERT_OK(RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_));
}

TEST_F(FlRunnerEligibilityEvalTest, EvalCheckinFails) {
  // Mock an eligibility eval checkin that fails with a network error.
  EXPECT_CALL(mock_phase_logger_, UpdateRetryWindowAndNetworkStats(_, _))
      .RetiresOnSaturation();
  EXPECT_CALL(mock_phase_logger_, LogEligibilityEvalCheckinStarted());
  EXPECT_CALL(mock_federated_protocol_, MockEligibilityEvalCheckin())
      .WillOnce(Return(absl::UnavailableError("foo")));
  EXPECT_CALL(mock_phase_logger_, LogEligibilityEvalCheckinIOError(_, _, _));

  // Since the eligibility eval checkin receives an error, no further
  // Checkin(...) request should be made, and the FLRunnerResult should return
  // the rejected retry window and a FAIL result.
  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::FAIL);
  EXPECT_THAT(*result, EqualsProto(expected_result));
}

TEST_F(FlRunnerEligibilityEvalTest, EvalCheckinRejected) {
  // Mock a rejected eligibility eval checkin.
  EXPECT_CALL(mock_phase_logger_, LogEligibilityEvalCheckinStarted());
  EXPECT_CALL(mock_federated_protocol_, MockEligibilityEvalCheckin())
      .WillOnce(Return(FederatedProtocol::Rejection{}));
  EXPECT_CALL(mock_phase_logger_,
              LogEligibilityEvalCheckinTurnedAway(
                  EqualsNetworkStats(
                      kPostEligibilityCheckinPlanUriReceivedNetworkStats),
                  _));

  // Since the eligibility eval checkin receives a Rejection, no further
  // Checkin(...) request should be made, and the FLRunnerResult should return
  // the rejected retry window and a FAIL result.
  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::FAIL);
  EXPECT_THAT(*result, EqualsProto(expected_result));
}

TEST_F(FlRunnerEligibilityEvalTest, EvalCheckinSucceedsRegularCheckinFails) {
  // Mock a successful eligibility eval checkin.
  EXPECT_CALL(mock_federated_protocol_, MockEligibilityEvalCheckin())
      .WillOnce(Return(FederatedProtocol::EligibilityEvalTask{
          {eligibility_eval_artifacts_.plan.SerializeAsString(),
           eligibility_eval_artifacts_.checkpoint},
          kEligibilityEvalExecutionId}));

  // Make the subsequent Checkin(...) method fail due to interruption.
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(absl::CancelledError("foo")));

  {
    InSequence seq;
    // The eligibility plan execution will log a set of training-related log
    // events, followed by a single checkin related log events for the regular
    // checkin.
    ExpectEligibilityEvalLogEvents();
    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
    EXPECT_CALL(mock_phase_logger_, LogCheckinStarted());
    EXPECT_CALL(mock_phase_logger_, LogCheckinClientInterrupted(_, _, _, _));
  }

  // Even though the Checkin(...) method fails with a network error, we expect
  // an FLRunnerResult to be returned with the most recent RetryWindow, when
  // the flag above is on.
  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::FAIL);
  EXPECT_THAT(*result, EqualsProto(expected_result));
}

TEST_F(FlRunnerEligibilityEvalTest,
       EvalCheckinSucceedsWithTaskResourcesAsAbslCordRegularCheckinFails) {
  // Mock a successful eligibility eval checkin.
  // We return the task resources as absl::Cords instead of std::strings, to
  // exercise the absl::Cord-specific code paths.
  EXPECT_CALL(mock_federated_protocol_, MockEligibilityEvalCheckin())
      .WillOnce(Return(FederatedProtocol::EligibilityEvalTask{
          {absl::Cord(eligibility_eval_artifacts_.plan.SerializeAsString()),
           absl::Cord(eligibility_eval_artifacts_.checkpoint)},
          kEligibilityEvalExecutionId}));

  // Make the subsequent Checkin(...) method fail due to interruption.
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(absl::CancelledError("foo")));

  {
    InSequence seq;
    // The eligibility plan execution will log a set of training-related log
    // events, followed by a single checkin related log events for the regular
    // checkin.
    ExpectEligibilityEvalLogEvents();
    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
    EXPECT_CALL(mock_phase_logger_, LogCheckinStarted());
    EXPECT_CALL(mock_phase_logger_, LogCheckinClientInterrupted(_, _, _, _));
  }

  // Even though the Checkin(...) method fails with a network error, we expect
  // an FLRunnerResult to be returned with the most recent RetryWindow, when
  // the flag above is on.
  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::FAIL);
  EXPECT_THAT(*result, EqualsProto(expected_result));
}

TEST_F(FlRunnerEligibilityEvalTest, EvalCheckinInvalidPlan) {
  // Mock a successful eligibility eval checkin that returns an invalid plan
  // proto.
  EXPECT_CALL(mock_federated_protocol_, MockEligibilityEvalCheckin())
      .WillOnce(Return(FederatedProtocol::EligibilityEvalTask{
          {"im_not_a_plan", eligibility_eval_artifacts_.checkpoint},
          kEligibilityEvalExecutionId}));
  EXPECT_CALL(mock_federated_protocol_,
              MockReportEligibilityEvalError(
                  absl::InternalError("Failed to compute eligibility info")));

  {
    InSequence seq;
    EXPECT_CALL(mock_phase_logger_, LogEligibilityEvalCheckinStarted());
    EXPECT_CALL(mock_phase_logger_,
                SetModelIdentifier(kEligibilityEvalExecutionId));
    EXPECT_CALL(mock_phase_logger_,
                LogEligibilityEvalCheckinPlanUriReceived(_, _));
    // We expect an IO error to be published reflecting the plan parsing
    // error.
    EXPECT_CALL(mock_phase_logger_,
                LogEligibilityEvalCheckinInvalidPayloadError(_, _, _));
  }

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  EXPECT_THAT(result->contribution_result(), FLRunnerResult::FAIL);
}

TEST_F(FlRunnerEligibilityEvalTest, EvalCheckinSucceedsRegularCheckinRejected) {
  // Mock a successful eligibility eval checkin.
  EXPECT_CALL(mock_federated_protocol_, MockEligibilityEvalCheckin())
      .WillOnce(Return(FederatedProtocol::EligibilityEvalTask{
          {eligibility_eval_artifacts_.plan.SerializeAsString(),
           eligibility_eval_artifacts_.checkpoint},
          kEligibilityEvalExecutionId}));

  // Make the subsequent Checkin(...) method return a rejection.
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::Rejection{}));

  {
    InSequence seq;

    // The eligibility plan execution will log a set of training-related log
    // events, followed by a single checkin related log events for the regular
    // checkin.
    ExpectEligibilityEvalLogEvents();
    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
    EXPECT_CALL(mock_phase_logger_, LogCheckinStarted());
    EXPECT_CALL(mock_phase_logger_, LogCheckinTurnedAway(_, _, _));
  }

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  // Ensure fl_runner.cc used the most recent RetryWindow, after the final
  // failed checkin request.
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::FAIL);
  EXPECT_THAT(*result, EqualsProto(expected_result));
}

TEST_F(FlRunnerEligibilityEvalTest, EvalCheckinSucceedsRegularCheckinSucceeds) {
  // The EligibilityEvalCheckin() will result in an eval task payload being
  // returned. This payload should be run by the runner, and the payload's
  // TaskEligibilityInfo result should be passed to the subsequent
  // Checkin(...) request.
  EXPECT_CALL(mock_federated_protocol_, MockEligibilityEvalCheckin())
      .WillOnce(Return(FederatedProtocol::EligibilityEvalTask{
          {eligibility_eval_artifacts_.plan.SerializeAsString(),
           eligibility_eval_artifacts_.checkpoint},
          kEligibilityEvalExecutionId,
          eligibility_eval_artifacts_.population_eligibility_spec}));

  // Check that the Checkin(...) call after the EligibilityEvalCheckin() call
  // uses the expected TaskEligibilityInfo (as generated by the test
  // TffEligibilityEvalTask).
  TaskEligibilityInfo expected_eligibility_info;
  expected_eligibility_info.set_version(1);
  TaskWeight* task_weight = expected_eligibility_info.add_task_weights();
  task_weight->set_task_name(kTaskName);
  task_weight->set_weight(1);
  MockSuccessfulEligibilityPlanExecution(expected_eligibility_info);
  EXPECT_CALL(mock_federated_protocol_,
              MockCheckin(Optional(EqualsProto(expected_eligibility_info)), _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));
  MockSuccessfulPlanExecution(/*has_checkpoint=*/true,
                              /*has_secagg_output=*/false);
  // We expect the regular plan to execute successfully, resulting in a
  // ReportCompleted call.
  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce(Return(absl::OkStatus()));

  {
    InSequence seq;
    // The eligibility plan execution will log a set of training-related log
    // events, followed by a full set of checkin-training-upload related log
    // events for the regular plan.
    ExpectEligibilityEvalLogEvents();
    ExpectCheckinTrainingReportLogEvents();
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true));
  }

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::SUCCESS);
  expected_result.mutable_contributed_task_names()->Add(kTaskName);
  EXPECT_THAT(*result, EqualsProto(expected_result));
}

TEST_F(FlRunnerEligibilityEvalTest,
       CheckinSucceedsIfLightweightClientReportWireFormat) {
  EXPECT_CALL(mock_flags_, enable_lightweight_client_report_wire_format())
      .WillRepeatedly(testing::Return(true));
  // The EligibilityEvalCheckin() will result in an eval task payload being
  // returned. This payload should be run by the runner, and the payload's
  // TaskEligibilityInfo result should be passed to the subsequent
  // Checkin(...) request.
  EXPECT_CALL(mock_federated_protocol_, MockEligibilityEvalCheckin())
      .WillOnce(Return(FederatedProtocol::EligibilityEvalTask{
          {eligibility_eval_artifacts_.plan.SerializeAsString(),
           eligibility_eval_artifacts_.checkpoint},
          kEligibilityEvalExecutionId,
          eligibility_eval_artifacts_.population_eligibility_spec}));

  // Check that the Checkin(...) call after the EligibilityEvalCheckin() call
  // uses the expected TaskEligibilityInfo (as generated by the test
  // TffEligibilityEvalTask).
  TaskEligibilityInfo expected_eligibility_info;
  expected_eligibility_info.set_version(1);
  TaskWeight* task_weight = expected_eligibility_info.add_task_weights();
  task_weight->set_task_name(kTaskName);
  task_weight->set_weight(1);
  MockSuccessfulEligibilityPlanExecution(expected_eligibility_info);
  EXPECT_CALL(mock_federated_protocol_,
              MockCheckin(Optional(EqualsProto(expected_eligibility_info)), _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));

  // We expect the regular plan to execute successfully, resulting in a
  // ReportCompleted call.
  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce(Return(absl::OkStatus()));
  MockSuccessfulPlanExecution(/*has_checkpoint=*/true,
                              /*has_secagg_output=*/false);

  {
    InSequence seq;
    // The eligibility plan execution will log a set of training-related log
    // events, followed by a full set of checkin-training-upload related log
    // events for the regular plan.
    ExpectEligibilityEvalLogEvents();
    ExpectCheckinTrainingReportLogEvents();
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true));
  }

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::SUCCESS);
  expected_result.mutable_contributed_task_names()->Add(kTaskName);
  EXPECT_THAT(*result, EqualsProto(expected_result));
}

TEST_F(FlRunnerEligibilityEvalWithCriteriaTest, ComputationIdSet) {
  // We need to find the selection criteria passed into eligibility_decider
  // when constructing the selector context for the data availability task.
  ExampleSelector expected_example_selector;
  for (const auto& client_data :
       eligibility_eval_artifacts_.dataset.client_data()) {
    for (const Dataset::ClientDataset::SelectedExample& selected_example :
         client_data.selected_example()) {
      if (selected_example.selector().collection_uri() ==
          kEligibilityEvalCollectionUri) {
        expected_example_selector = selected_example.selector();
        break;
      }
    }
  }

  EXPECT_CALL(mock_federated_protocol_, MockEligibilityEvalCheckin())
      .WillOnce(Return(FederatedProtocol::EligibilityEvalTask{
          {eligibility_eval_artifacts_.plan.SerializeAsString(),
           eligibility_eval_artifacts_.checkpoint},
          kEligibilityEvalExecutionId,
          eligibility_eval_artifacts_.population_eligibility_spec}));

  TaskEligibilityInfo expected_eligibility_info;
  expected_eligibility_info.set_version(1);
  TaskWeight* task_weight = expected_eligibility_info.add_task_weights();
  task_weight->set_task_name(kTaskName);
  task_weight->set_weight(1);
  EXPECT_CALL(mock_federated_protocol_,
              MockCheckin(Optional(EqualsProto(expected_eligibility_info)), _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));
  MockSuccessfulPlanExecution(/*has_checkpoint=*/true,
                              /*has_secagg_output=*/false);
  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce(Return(absl::OkStatus()));

  {
    InSequence seq;
    ExpectEligibilityEvalLogEvents();
    ExpectCheckinTrainingReportLogEvents();
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true));
  }

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::SUCCESS);
  expected_result.mutable_contributed_task_names()->Add(kTaskName);
  EXPECT_THAT(*result, EqualsProto(expected_result));

  // Ensure that the example queries were passed the correct selector
  // contexts, including the computation id calculated based on the selection
  // criteria.
  SelectorContext expected_eligibility_eval_selector_context;
  expected_eligibility_eval_selector_context.mutable_computation_properties()
      ->set_session_name(kSessionName);
  expected_eligibility_eval_selector_context.mutable_computation_properties()
      ->mutable_eligibility_eval()
      ->set_population_name(kPopulationName);
  expected_eligibility_eval_selector_context.mutable_computation_properties()
      ->mutable_eligibility_eval()
      ->set_computation_id(ComputeSHA256(
          expected_example_selector.criteria().SerializeAsString()));
  EXPECT_THAT(latest_eligibility_selector_context_,
              EqualsProto(expected_eligibility_eval_selector_context));
}

TEST_F(FlRunnerExampleQueryEligibilityEvalTest, UseExampleQueryResultFormat) {
  // We need to find the selection criteria passed into eligibility_decider
  // when constructing the selector context for the data availability task.
  ExampleSelector expected_example_selector;
  for (const auto& client_data :
       eligibility_eval_artifacts_.dataset.client_data()) {
    for (const Dataset::ClientDataset::SelectedExample& selected_example :
         client_data.selected_example()) {
      if (selected_example.selector().collection_uri() ==
          kEligibilityEvalCollectionUri) {
        expected_example_selector = selected_example.selector();
        break;
      }
    }
  }

  EXPECT_CALL(mock_federated_protocol_, MockEligibilityEvalCheckin())
      .WillOnce(Return(FederatedProtocol::EligibilityEvalTask{
          {eligibility_eval_artifacts_.plan.SerializeAsString(),
           eligibility_eval_artifacts_.checkpoint},
          kEligibilityEvalExecutionId,
          eligibility_eval_artifacts_.population_eligibility_spec}));

  TaskEligibilityInfo expected_eligibility_info;
  expected_eligibility_info.set_version(1);
  TaskWeight* task_weight = expected_eligibility_info.add_task_weights();
  task_weight->set_task_name(kTaskName);
  task_weight->set_weight(1);

  EXPECT_CALL(mock_federated_protocol_,
              MockCheckin(Optional(EqualsProto(expected_eligibility_info)), _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));
  MockSuccessfulPlanExecution(/*has_checkpoint=*/true,
                              /*has_secagg_output=*/false);
  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce(Return(absl::OkStatus()));

  {
    InSequence seq;
    ExpectEligibilityEvalLogEvents();
    ExpectCheckinTrainingReportLogEvents();
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true));
  }

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::SUCCESS);
  expected_result.mutable_contributed_task_names()->Add(kTaskName);
  EXPECT_THAT(*result, EqualsProto(expected_result));

  // Ensure that the eligibility decider passed the correct selector context.
  SelectorContext expected_eligibility_eval_selector_context;
  expected_eligibility_eval_selector_context.mutable_computation_properties()
      ->set_session_name(kSessionName);
  expected_eligibility_eval_selector_context.mutable_computation_properties()
      ->mutable_eligibility_eval()
      ->set_population_name(kPopulationName);
  expected_eligibility_eval_selector_context.mutable_computation_properties()
      ->mutable_eligibility_eval()
      ->set_computation_id(ComputeSHA256(
          expected_example_selector.criteria().SerializeAsString()));
  // Verify that the eligibility decider requested results to be returned in
  // the example query result format.
  expected_eligibility_eval_selector_context.mutable_computation_properties()
      ->set_example_iterator_output_format(
          ::fcp::client::QueryTimeComputationProperties::EXAMPLE_QUERY_RESULT);
  EXPECT_THAT(latest_eligibility_selector_context_,
              EqualsProto(expected_eligibility_eval_selector_context));
}

TEST_F(FlRunnerExampleQueryTest, TaskSucceeds) {
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_client_only_plan_.SerializeAsString(), ""},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));

  EXPECT_CALL(*mock_tensorflow_runner_, WriteTFV1Checkpoint(_, _))
      .WillOnce(
          [](const std::string& output_checkpoint_filename,
             const std::vector<std::pair<google::internal::federated::plan::
                                             ExampleQuerySpec::ExampleQuery,
                                         ExampleQueryResult>>&
                 example_query_results) {
            // Write something to the checkpoint file to make the file
            // non-empty.
            WriteContentToFile(output_checkpoint_filename, "query_results");
            return absl::OkStatus();
          });

  ComputationResults computation_results;
  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        computation_results = std::move(reported_results);
        return absl::OkStatus();
      });

  ExpectExampleQueryCheckinTrainingReportLogEvents();
  EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
      .WillOnce(Return(true));

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::SUCCESS);
  expected_result.mutable_contributed_task_names()->Add(kTaskName);

  EXPECT_THAT(*result, EqualsProto(expected_result));

  EXPECT_EQ(computation_results.size(), 1 /*checkpoint*/);

  // Check that the checkpoint was populated.
  EXPECT_THAT(computation_results,
              Contains(Pair(kTensorflowCheckpointAggregand,
                            VariantWith<TFCheckpoint>(Not(IsEmpty())))));
  EXPECT_EQ(
      latest_selector_context_.computation_properties()
          .example_iterator_output_format(),
      ::fcp::client::QueryTimeComputationProperties::EXAMPLE_QUERY_RESULT);
}

TEST_F(FlRunnerExampleQueryTest, FederatedComputeWireFormat) {
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_client_only_plan_.SerializeAsString(), ""},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));
  EXPECT_CALL(mock_flags_, enable_lightweight_client_report_wire_format())
      .WillRepeatedly(testing::Return(true));
  ComputationResults computation_results;
  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        computation_results = std::move(reported_results);
        return absl::OkStatus();
      });

  ExpectExampleQueryCheckinTrainingReportLogEvents();
  EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
      .WillOnce(Return(true));

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::SUCCESS);
  expected_result.mutable_contributed_task_names()->Add(kTaskName);

  EXPECT_THAT(*result, EqualsProto(expected_result));

  EXPECT_EQ(computation_results.size(), 1 /*checkpoint*/);

  // Check that the checkpoint was populated.
  EXPECT_THAT(computation_results,
              Contains(Pair(kFederatedComputeCheckpoint,
                            VariantWith<FCCheckpoint>(Not(IsEmpty())))));
  EXPECT_EQ(
      latest_selector_context_.computation_properties()
          .example_iterator_output_format(),
      ::fcp::client::QueryTimeComputationProperties::EXAMPLE_QUERY_RESULT);
}

TEST_F(FlRunnerExampleQueryTest, FCCheckpointAggregationEnabled) {
  // We intentionally set this to false to ensure that the client report wire
  // format is still used when the flag is false, but aggregation config is
  // FCCheckpointAggregation.
  EXPECT_CALL(mock_flags_, enable_lightweight_client_report_wire_format())
      .WillRepeatedly(testing::Return(false));

  single_task_assignment_client_only_plan_.mutable_phase()
      ->clear_federated_example_query();
  AggregationConfig aggregation_config;
  aggregation_config.mutable_federated_compute_checkpoint_aggregation();
  (*single_task_assignment_client_only_plan_.mutable_phase()
        ->mutable_federated_example_query()
        ->mutable_aggregations())["string_tensor"] = aggregation_config;
  (*single_task_assignment_client_only_plan_.mutable_phase()
        ->mutable_federated_example_query()
        ->mutable_aggregations())["int_tensor"] = aggregation_config;

  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_client_only_plan_.SerializeAsString(), ""},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));

  ComputationResults computation_results;
  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        computation_results = std::move(reported_results);
        return absl::OkStatus();
      });

  ExpectExampleQueryCheckinTrainingReportLogEvents();
  EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
      .WillOnce(Return(true));

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::SUCCESS);
  expected_result.mutable_contributed_task_names()->Add(kTaskName);

  EXPECT_THAT(*result, EqualsProto(expected_result));

  EXPECT_EQ(computation_results.size(), 1 /*checkpoint*/);

  // Check that the checkpoint was populated.
  EXPECT_THAT(computation_results,
              Contains(Pair(kFederatedComputeCheckpoint,
                            VariantWith<FCCheckpoint>(Not(IsEmpty())))));
  EXPECT_EQ(
      latest_selector_context_.computation_properties()
          .example_iterator_output_format(),
      ::fcp::client::QueryTimeComputationProperties::EXAMPLE_QUERY_RESULT);
}

TEST_F(FlRunnerExampleQueryTest, LightweightTaskDoesNotCreateTempFiles) {
  // Set up a temp files impl
  std::string root_dir = testing::TempDir();
  std::filesystem::path root_dir_path(root_dir);
  std::filesystem::path temp_file_dir = root_dir_path /
                                        cache::TempFiles::kParentDir /
                                        cache::TempFiles::kTempFilesDir;

  absl::StatusOr<std::unique_ptr<cache::TempFiles>> temp_files =
      cache::TempFiles::Create(root_dir, &mock_log_manager_);
  ASSERT_OK(temp_files);

  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_client_only_plan_.SerializeAsString(), ""},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));
  EXPECT_CALL(mock_flags_, enable_lightweight_client_report_wire_format())
      .WillRepeatedly(testing::Return(true));

  ComputationResults computation_results;
  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        computation_results = std::move(reported_results);
        return absl::OkStatus();
      });

  ExpectExampleQueryCheckinTrainingReportLogEvents();
  EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
      .WillOnce(Return(true));

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_,
      temp_files->get(), &mock_log_manager_, &mock_opstats_logger_,
      &mock_flags_, &mock_federated_protocol_, &mock_fedselect_manager_,
      timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::SUCCESS);
  expected_result.mutable_contributed_task_names()->Add(kTaskName);

  EXPECT_THAT(*result, EqualsProto(expected_result));

  // Make sure we made a single temp file in the right directory
  int num_files = 0;
  for ([[maybe_unused]] auto const& unused :
       std::filesystem::directory_iterator{temp_file_dir}) {
    num_files++;
  }
  // Should create zero files, as lightweight tasks do not have graphs or output
  // checkpoints.
  ASSERT_EQ(num_files, 0);
}

TEST_F(FlRunnerExampleQueryTest, NoExampleQueryIORouter) {
  single_task_assignment_client_only_plan_.mutable_phase()
      ->clear_federated_example_query();

  EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
  EXPECT_CALL(mock_phase_logger_, LogCheckinStarted());
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_client_only_plan_.SerializeAsString(), ""},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));
  EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(kTaskName));
  ExpectCheckinCompletedLogEvents();
  EXPECT_CALL(mock_phase_logger_, LogComputationStarted(kTaskName));
  EXPECT_CALL(mock_phase_logger_, LogComputationInvalidArgument(_, _, _, _));
  EXPECT_CALL(mock_phase_logger_, LogFailureUploadStarted())
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(mock_federated_protocol_,
              MockReportNotCompleted(engine::PhaseOutcome::ERROR, _, _))
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(mock_phase_logger_,
              LogFailureUploadCompleted(
                  EqualsNetworkStats(kPostReportNotCompletedNetworkStats -
                                     kPostCheckinNetworkStats),
                  _, _));
  EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultFailure()))
      .WillOnce(Return(true));

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  EXPECT_EQ(result->contribution_result(), FLRunnerResult::FAIL);
}

TEST_F(FlRunnerExampleQueryTest, ExampleQueryIORouterUnsupportedAggregation) {
  single_task_assignment_client_only_plan_.mutable_phase()
      ->clear_federated_example_query();
  AggregationConfig tf_v1_aggregation_config;
  tf_v1_aggregation_config.mutable_tf_v1_checkpoint_aggregation();
  (*single_task_assignment_client_only_plan_.mutable_phase()
        ->mutable_federated_example_query()
        ->mutable_aggregations())["string_tensor"] = tf_v1_aggregation_config;
  AggregationConfig sec_aggregation_config;
  sec_aggregation_config.mutable_secure_aggregation();
  (*single_task_assignment_client_only_plan_.mutable_phase()
        ->mutable_federated_example_query()
        ->mutable_aggregations())["int_tensor"] = sec_aggregation_config;

  ExpectComputationFailureWithInvalidArgument();
}

TEST_F(FlRunnerExampleQueryTest, ExampleQueryIORouterMixedAggregation) {
  single_task_assignment_client_only_plan_.mutable_phase()
      ->clear_federated_example_query();
  AggregationConfig tf_v1_aggregation_config;
  tf_v1_aggregation_config.mutable_tf_v1_checkpoint_aggregation();
  (*single_task_assignment_client_only_plan_.mutable_phase()
        ->mutable_federated_example_query()
        ->mutable_aggregations())["string_tensor"] = tf_v1_aggregation_config;
  AggregationConfig fccheckpoint_aggregation_config;
  fccheckpoint_aggregation_config
      .mutable_federated_compute_checkpoint_aggregation();
  (*single_task_assignment_client_only_plan_.mutable_phase()
        ->mutable_federated_example_query()
        ->mutable_aggregations())["int_tensor"] =
      fccheckpoint_aggregation_config;

  ExpectComputationFailureWithInvalidArgument();
}

TEST_F(FlRunnerExampleQueryTest, ExampleQueryPlanLightweightComputation) {
  // Metric is an example of a custom selection criteria here.
  Metric metric;
  metric.set_variable_name("metric");
  metric.set_stat_name("stat");
  single_task_assignment_client_only_plan_.mutable_phase()
      ->mutable_example_query_spec()
      ->mutable_example_queries(0)
      ->mutable_example_selector()
      ->mutable_criteria()
      ->PackFrom(metric);

  EXPECT_CALL(mock_task_env_,
              CreateExampleIterator(
                  EqualsProto("collection_uri: 'app:/test_collection'"
                              "criteria {"
                              "[type.googleapis.com/"
                              "google.internal.federated.plan.Metric] {"
                              "    variable_name: \"metric\""
                              "    stat_name: \"stat\""
                              "  }"
                              "}"),
                  _))
      .Times(AtMost(1))
      .WillOnce(DoAll(
          SaveArg<1>(&latest_selector_context_),
          Return(ByMove(std::make_unique<SimpleExampleIterator>(dataset_)))));

  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_client_only_plan_.SerializeAsString(), ""},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));

  EXPECT_CALL(*mock_tensorflow_runner_, WriteTFV1Checkpoint(_, _))
      .WillOnce(
          [](const std::string& output_checkpoint_filename,
             const std::vector<std::pair<google::internal::federated::plan::
                                             ExampleQuerySpec::ExampleQuery,
                                         ExampleQueryResult>>&
                 example_query_results) {
            WriteContentToFile(output_checkpoint_filename, "query_results");
            return absl::OkStatus();
          });

  ComputationResults computation_results;
  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        computation_results = std::move(reported_results);
        return absl::OkStatus();
      });

  ExpectExampleQueryCheckinTrainingReportLogEvents();
  EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
      .WillOnce(Return(true));

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::SUCCESS);
  expected_result.mutable_contributed_task_names()->Add(kTaskName);

  EXPECT_THAT(*result, EqualsProto(expected_result));

  EXPECT_EQ(computation_results.size(), 1 /*checkpoint*/);

  // Check that the checkpoint was populated.
  EXPECT_THAT(computation_results,
              Contains(Pair(kTensorflowCheckpointAggregand,
                            VariantWith<TFCheckpoint>(Not(IsEmpty())))));
  EXPECT_EQ(
      latest_selector_context_.computation_properties()
          .example_iterator_output_format(),
      ::fcp::client::QueryTimeComputationProperties::EXAMPLE_QUERY_RESULT);

  // Verify that the computation id is calculated as the hash of the selection
  // criteria.
  Any criteria;
  criteria.PackFrom(metric);
  std::string computation_id = ComputeSHA256(criteria.SerializeAsString());
  EXPECT_EQ(latest_selector_context_.computation_properties()
                .federated()
                .computation_id(),
            computation_id);
}

TEST_F(FlRunnerExampleQueryTest, ConfidentialAggInSelectorContext) {
  // Metric is an example of a custom selection criteria here.
  Metric metric;
  metric.set_variable_name("metric");
  metric.set_stat_name("stat");
  single_task_assignment_client_only_plan_.mutable_phase()
      ->mutable_example_query_spec()
      ->mutable_example_queries(0)
      ->mutable_example_selector()
      ->mutable_criteria()
      ->PackFrom(metric);

  EXPECT_CALL(mock_task_env_,
              CreateExampleIterator(
                  EqualsProto("collection_uri: 'app:/test_collection'"
                              "criteria {"
                              "[type.googleapis.com/"
                              "google.internal.federated.plan.Metric] {"
                              "    variable_name: \"metric\""
                              "    stat_name: \"stat\""
                              "  }"
                              "}"),
                  _))
      .Times(AtMost(1))
      .WillOnce(DoAll(
          SaveArg<1>(&latest_selector_context_),
          Return(ByMove(std::make_unique<SimpleExampleIterator>(dataset_)))));

  FederatedProtocol::ConfidentialAggInfo confidential_agg_info = {
      .data_access_policy = absl::Cord("data_access_policy")};

  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_client_only_plan_.SerializeAsString(), ""},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          confidential_agg_info,
          kTaskName}));

  EXPECT_CALL(*mock_tensorflow_runner_, WriteTFV1Checkpoint(_, _))
      .WillOnce(
          [](const std::string& output_checkpoint_filename,
             const std::vector<std::pair<google::internal::federated::plan::
                                             ExampleQuerySpec::ExampleQuery,
                                         ExampleQueryResult>>&
                 example_query_results) {
            WriteContentToFile(output_checkpoint_filename, "query_results");
            return absl::OkStatus();
          });

  ComputationResults computation_results;
  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        computation_results = std::move(reported_results);
        return absl::OkStatus();
      });

  ExpectExampleQueryCheckinTrainingReportLogEvents();
  EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
      .WillOnce(Return(true));

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);

  EXPECT_TRUE(latest_selector_context_.computation_properties()
                  .federated()
                  .has_confidential_aggregation());
}

TEST_F(FlRunnerExampleQueryTest, DirectDataUploadTaskSucceeds) {
  EXPECT_CALL(mock_flags_, enable_direct_data_upload_task())
      .WillRepeatedly(Return(true));
  SetUpDirectDataUploadTask();
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_client_only_plan_.SerializeAsString(), ""},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));

  ComputationResults computation_results;
  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        computation_results = std::move(reported_results);
        return absl::OkStatus();
      });

  ExpectExampleQueryCheckinTrainingReportLogEvents();
  EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
      .WillOnce(Return(true));

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::SUCCESS);
  expected_result.mutable_contributed_task_names()->Add(kTaskName);

  EXPECT_THAT(*result, EqualsProto(expected_result));

  EXPECT_EQ(computation_results.size(), 1 /*checkpoint*/);

  // Check that the checkpoint was populated.
  EXPECT_THAT(computation_results,
              Contains(Pair(kFederatedComputeCheckpoint,
                            VariantWith<FCCheckpoint>(Not(IsEmpty())))));
}

TEST_F(FlRunnerExampleQueryTest, DirectDataUploadTaskNotEnabled) {
  EXPECT_CALL(mock_flags_, enable_direct_data_upload_task())
      .WillRepeatedly(Return(false));
  SetUpDirectDataUploadTask();

  ExpectComputationFailureWithInvalidArgument();
}

TEST_F(FlRunnerExampleQueryTest,
       DirectDataUploadTaskWithUnsupportedAggregation) {
  EXPECT_CALL(mock_flags_, enable_direct_data_upload_task())
      .WillRepeatedly(Return(true));
  SetUpDirectDataUploadTask();

  AggregationConfig unsupported_aggregation;
  unsupported_aggregation.mutable_tf_v1_checkpoint_aggregation();
  auto* aggregations = single_task_assignment_client_only_plan_.mutable_phase()
                           ->mutable_federated_example_query()
                           ->mutable_aggregations();
  (*aggregations)["data_upload_tensor"] = unsupported_aggregation;

  ExpectComputationFailureWithInvalidArgument();
}

TEST_F(FlRunnerExampleQueryTest, InvalidTaskWithInconsistentAggregationConfig) {
  auto* aggregations = single_task_assignment_client_only_plan_.mutable_phase()
                           ->mutable_federated_example_query()
                           ->mutable_aggregations();
  AggregationConfig config;
  config.mutable_federated_compute_checkpoint_aggregation();
  (*aggregations)["int_tensor"] = config;

  ExpectComputationFailureWithInvalidArgument();
}

TEST_F(FlRunnerExampleQueryTest,
       InvalidQueryWithBothDirectUploadTensorNameAndOutputVectorSpecs) {
  auto* example_queries =
      single_task_assignment_client_only_plan_.mutable_phase()
          ->mutable_example_query_spec()
          ->mutable_example_queries();
  example_queries->at(0).set_direct_output_tensor_name("redundant_name");

  ExpectComputationFailureWithInvalidArgument();
}

TEST_F(FlRunnerExampleQueryTest,
       InvalidQueryWithNeitherDirectUploadTensorNameAndOutputVectorSpecs) {
  auto* example_queries =
      single_task_assignment_client_only_plan_.mutable_phase()
          ->mutable_example_query_spec()
          ->mutable_example_queries();
  example_queries->at(0).clear_output_vector_specs();

  ExpectComputationFailureWithInvalidArgument();
}

TEST_F(FlRunnerExampleQueryTest, ExampleQueryWithEventTimeRange) {
  SetUpExampleQueryWithEventTimeRange();

  // Mock the opstats logger to return an empty sequence so source ID creation
  // doesn't fail.
  EXPECT_CALL(mock_opstats_logger_, GetOpStatsDb())
      .WillRepeatedly(Return(&mock_opstats_db_));
  fcp::client::opstats::OpStatsSequence empty_sequence;
  EXPECT_CALL(mock_opstats_db_, Read()).WillRepeatedly(Return(empty_sequence));

  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_client_only_plan_.SerializeAsString(), ""},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));
  EXPECT_CALL(mock_flags_, enable_lightweight_client_report_wire_format())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_flags_, enable_event_time_data_upload())
      .WillRepeatedly(testing::Return(true));
  ComputationResults computation_results;
  EventTimeRange event_time_range;
  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> report_payload_metadata) {
        computation_results = std::move(reported_results);
        event_time_range =
            std::move(report_payload_metadata->event_time_range());
        return absl::OkStatus();
      });

  ExpectExampleQueryCheckinTrainingReportLogEvents();
  EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
      .WillOnce(Return(true));

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::SUCCESS);
  expected_result.mutable_contributed_task_names()->Add(kTaskName);

  EXPECT_THAT(*result, EqualsProto(expected_result));
  EXPECT_EQ(computation_results.size(), 1 /*checkpoint*/);
  EXPECT_THAT(computation_results,
              Contains(Pair(kFederatedComputeCheckpoint,
                            VariantWith<FCCheckpoint>(Not(IsEmpty())))));
  EXPECT_THAT(event_time_range,
              EqualsProto(payload_metadata_.value().event_time_range()));
}

// This tests the case where the multiple task assignments feature is enabled
// but where the server doesn't return a PopulationSpec at all, and hence no
// multiple task assignment request is expected to be issued."
TEST_F(FlRunnerMultipleTaskAssignmentsTest, EmptyPopulationSpec) {
  SetUpEligibilityEvalTask();
  ExpectEligibilityEvalLogEvents();

  // Mock a successful eligibility eval checkin. The returned
  // EligibilityEvalTask contains no population eligibility spec.
  EXPECT_CALL(mock_federated_protocol_, MockEligibilityEvalCheckin())
      .WillOnce(Return(FederatedProtocol::EligibilityEvalTask{
          {eligibility_eval_artifacts_.plan.SerializeAsString(),
           eligibility_eval_artifacts_.checkpoint},
          kEligibilityEvalExecutionId}));

  // The client should proceed to single task assignment directly.
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));

  MockSuccessfulPlanExecution(/*has_checkpoint=*/true,
                              /*has_secagg_output=*/false);

  ComputationResults computation_results;
  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        computation_results = std::move(reported_results);
        return absl::OkStatus();
      });

  {
    InSequence seq;
    ExpectCheckinTrainingReportLogEvents();
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true));
  }

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::SUCCESS);
  expected_result.mutable_contributed_task_names()->Add(kTaskName);
  EXPECT_THAT(*result, EqualsProto(expected_result));
  // There should be one checkpoint and no secagg results.
  EXPECT_THAT(computation_results,
              ElementsAre(Pair(kTensorflowCheckpointAggregand,
                               VariantWith<TFCheckpoint>(Not(IsEmpty())))));
}

// This tests the case where the multiple task assignments feature is enabled
// and eligibility eval is not configured. We expect multiple task assignment
// request is expected to be issued.
TEST_F(FlRunnerMultipleTaskAssignmentsTest,
       EligibilityEvalDisabledMultipleTaskAssignmentsEnabled) {
  // Mock a eligibility eval checkin where eligibility eval is disabled, but
  // multiple task assignments is enabled.
  PopulationEligibilitySpec population_spec;
  auto task_info_1 = population_spec.add_task_info();
  task_info_1->set_task_name(std::string(kSwor24HourTaskName));
  task_info_1->set_task_assignment_mode(
      PopulationEligibilitySpec::TaskInfo::TASK_ASSIGNMENT_MODE_MULTIPLE);
  auto task_info_2 = population_spec.add_task_info();
  task_info_2->set_task_name(std::string(kRequires5ExamplesTaskName));
  task_info_2->set_task_assignment_mode(
      PopulationEligibilitySpec::TaskInfo::TASK_ASSIGNMENT_MODE_MULTIPLE);
  auto task_info_3 = population_spec.add_task_info();
  task_info_3->set_task_name(kTaskName);
  task_info_3->set_task_assignment_mode(
      PopulationEligibilitySpec::TaskInfo::TASK_ASSIGNMENT_MODE_SINGLE);
  EXPECT_CALL(mock_federated_protocol_, MockEligibilityEvalCheckin())
      .WillOnce(
          Return(FederatedProtocol::EligibilityEvalDisabled{population_spec}));
  FederatedProtocol::MultipleTaskAssignments multiple_task_assignments;
  multiple_task_assignments
      .task_assignments[kMultipleTaskAggregationSessionId1] =
      task_assignment_1_;
  multiple_task_assignments
      .task_assignments[kMultipleTaskAggregationSessionId2] =
      task_assignment_2_;
  EXPECT_CALL(mock_federated_protocol_,
              MockPerformMultipleTaskAssignments(
                  ElementsAre(std::string(kSwor24HourTaskName),
                              std::string(kRequires5ExamplesTaskName)),
                  _, _))
      .WillOnce(Return(std::move(multiple_task_assignments)));

  MockSuccessfulPlanExecution(/*has_checkpoint=*/true,
                              /*has_secagg_output=*/true, EqualsProto(plan_1_));

  MockSuccessfulPlanExecution(/*has_checkpoint=*/false,
                              /*has_secagg_output=*/true, EqualsProto(plan_2_));
  MockSuccessfulPlanExecution(
      /*has_checkpoint=*/true,
      /*has_secagg_output=*/false,
      EqualsProto(single_task_assignment_client_only_plan_));

  ComputationResults multiple_task_assignment_computation_results_1;
  EXPECT_CALL(
      mock_federated_protocol_,
      MockReportCompleted(_, _, Eq(task_assignment_1_.task_identifier), _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        multiple_task_assignment_computation_results_1 =
            std::move(reported_results);
        return absl::OkStatus();
      });
  ComputationResults multiple_task_assignment_computation_results_2;
  EXPECT_CALL(
      mock_federated_protocol_,
      MockReportCompleted(_, _, Eq(task_assignment_2_.task_identifier), _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        multiple_task_assignment_computation_results_2 =
            std::move(reported_results);
        return absl::OkStatus();
      });

  EXPECT_CALL(mock_federated_protocol_, MockCheckin(Eq(std::nullopt), _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));
  ComputationResults computation_results;
  EXPECT_CALL(mock_federated_protocol_,
              MockReportCompleted(_, _, Eq(std::nullopt), _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        computation_results = std::move(reported_results);
        return absl::OkStatus();
      });

  {
    InSequence seq;

    EXPECT_CALL(mock_phase_logger_, LogEligibilityEvalCheckinStarted());
    EXPECT_CALL(mock_phase_logger_,
                LogEligibilityEvalNotConfigured(
                    EqualsNetworkStats(
                        kPostEligibilityCheckinPlanUriReceivedNetworkStats),
                    _));

    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
    EXPECT_CALL(mock_phase_logger_, LogMultipleTaskAssignmentsStarted());
    EXPECT_CALL(mock_phase_logger_,
                LogMultipleTaskAssignmentsPlanUriReceived(
                    MockFederatedProtocol::
                        kMultipleTaskAssignmentsPlanUriReceivedNetworkStats,
                    _));
    EXPECT_CALL(mock_phase_logger_,
                LogMultipleTaskAssignmentsCompleted(
                    MockFederatedProtocol::
                        kMultipleTaskAssignmentsArtifactRetrievalNetworkStats,
                    _, _, _));
    // Computation and upload for the first task from multiple task assignments.
    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(kSwor24HourTaskName));
    EXPECT_CALL(mock_phase_logger_, LogComputationStarted(kSwor24HourTaskName));
    EXPECT_CALL(
        mock_phase_logger_,
        LogComputationCompleted(FieldsAre(Gt(0), Gt(0)),
                                EqualsNetworkStats(NetworkStats()), _, _, _));
    EXPECT_CALL(mock_phase_logger_, LogResultUploadStarted())
        .WillOnce(Return(absl::OkStatus()));

    EXPECT_CALL(mock_phase_logger_,
                LogResultUploadCompleted(
                    EqualsNetworkStats(
                        MockFederatedProtocol::kReportCompletedNetworkStats),
                    _, _));
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true))
        .RetiresOnSaturation();
    // Computation and upload for the second task from multiple task
    // assignments.
    EXPECT_CALL(mock_phase_logger_,
                SetModelIdentifier(kRequires5ExamplesTaskName));
    EXPECT_CALL(mock_phase_logger_,
                LogComputationStarted(kRequires5ExamplesTaskName));
    EXPECT_CALL(
        mock_phase_logger_,
        LogComputationCompleted(FieldsAre(Gt(0), Gt(0)),
                                EqualsNetworkStats(NetworkStats()), _, _, _));
    EXPECT_CALL(mock_phase_logger_, LogResultUploadStarted())
        .WillOnce(Return(absl::OkStatus()));

    EXPECT_CALL(mock_phase_logger_,
                LogResultUploadCompleted(
                    EqualsNetworkStats(
                        MockFederatedProtocol::kReportCompletedNetworkStats),
                    _, _));
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true))
        .RetiresOnSaturation();
    // Check-in, computation and upload for the regular task assignment.
    ExpectCheckinTrainingReportLogEvents();
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true));
  }

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::SUCCESS);
  expected_result.mutable_contributed_task_names()->Add(
      std::string(kSwor24HourTaskName));
  expected_result.mutable_contributed_task_names()->Add(
      std::string(kRequires5ExamplesTaskName));
  expected_result.mutable_contributed_task_names()->Add(kTaskName);
  EXPECT_THAT(*result, EqualsProto(expected_result));
  // For the mixed secagg task, there should be one checkpoint and some secagg
  // tensors.
  EXPECT_THAT(multiple_task_assignment_computation_results_1,
              Contains(Pair(kTensorflowCheckpointAggregand,
                            VariantWith<TFCheckpoint>(Not(IsEmpty())))));
  EXPECT_GT(multiple_task_assignment_computation_results_1.size(), 1);

  // For pure secagg task, there should be no checkpoint, and only secagg
  // tensors.
  EXPECT_GT(multiple_task_assignment_computation_results_2.size(), 0);

  // For the single task assignment, there should be one checkpoint and no
  // secagg results.
  EXPECT_THAT(computation_results,
              ElementsAre(Pair(kTensorflowCheckpointAggregand,
                               VariantWith<TFCheckpoint>(Not(IsEmpty())))));
}

// This tests the case where the multiple task assignments feature is enabled
// ,eligibility eval is not configured and the population only supports multiple
// task assignments. We expect multiple task assignment request is issued, but
// no single task assignment.
TEST_F(FlRunnerMultipleTaskAssignmentsTest,
       EligibilityEvalDisabledMultipleTaskAssignmentsEnabledCheckInDisabled) {
  // Mock a eligibility eval checkin where eligibility eval is disabled, but
  // multiple task assignments is enabled.  The population only has tasks for
  // multiple task assignments.
  PopulationEligibilitySpec population_spec;
  auto task_info_1 = population_spec.add_task_info();
  task_info_1->set_task_name(std::string(kSwor24HourTaskName));
  task_info_1->set_task_assignment_mode(
      PopulationEligibilitySpec::TaskInfo::TASK_ASSIGNMENT_MODE_MULTIPLE);
  auto task_info_2 = population_spec.add_task_info();
  task_info_2->set_task_name(std::string(kRequires5ExamplesTaskName));
  task_info_2->set_task_assignment_mode(
      PopulationEligibilitySpec::TaskInfo::TASK_ASSIGNMENT_MODE_MULTIPLE);
  EXPECT_CALL(mock_federated_protocol_, MockEligibilityEvalCheckin())
      .WillOnce(
          Return(FederatedProtocol::EligibilityEvalDisabled{population_spec}));
  FederatedProtocol::MultipleTaskAssignments multiple_task_assignments;
  multiple_task_assignments
      .task_assignments[kMultipleTaskAggregationSessionId1] =
      task_assignment_1_;
  multiple_task_assignments
      .task_assignments[kMultipleTaskAggregationSessionId2] =
      task_assignment_2_;
  EXPECT_CALL(mock_federated_protocol_,
              MockPerformMultipleTaskAssignments(
                  ElementsAre(std::string(kSwor24HourTaskName),
                              std::string(kRequires5ExamplesTaskName)),
                  _, _))
      .WillOnce(Return(std::move(multiple_task_assignments)));
  ComputationResults multiple_task_assignment_computation_results_1;
  EXPECT_CALL(
      mock_federated_protocol_,
      MockReportCompleted(_, _, Eq(task_assignment_1_.task_identifier), _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        multiple_task_assignment_computation_results_1 =
            std::move(reported_results);
        return absl::OkStatus();
      });
  ComputationResults multiple_task_assignment_computation_results_2;
  EXPECT_CALL(
      mock_federated_protocol_,
      MockReportCompleted(_, _, Eq(task_assignment_2_.task_identifier), _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        multiple_task_assignment_computation_results_2 =
            std::move(reported_results);
        return absl::OkStatus();
      });

  MockSuccessfulPlanExecution(/*has_checkpoint=*/true,
                              /*has_secagg_output=*/true, EqualsProto(plan_1_));

  MockSuccessfulPlanExecution(/*has_checkpoint=*/false,
                              /*has_secagg_output=*/true, EqualsProto(plan_2_));

  {
    InSequence seq;

    EXPECT_CALL(mock_phase_logger_, LogEligibilityEvalCheckinStarted());
    EXPECT_CALL(mock_phase_logger_,
                LogEligibilityEvalNotConfigured(
                    EqualsNetworkStats(
                        kPostEligibilityCheckinPlanUriReceivedNetworkStats),
                    _));

    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
    EXPECT_CALL(mock_phase_logger_, LogMultipleTaskAssignmentsStarted());
    EXPECT_CALL(mock_phase_logger_,
                LogMultipleTaskAssignmentsPlanUriReceived(
                    MockFederatedProtocol::
                        kMultipleTaskAssignmentsPlanUriReceivedNetworkStats,
                    _));
    EXPECT_CALL(mock_phase_logger_,
                LogMultipleTaskAssignmentsCompleted(
                    MockFederatedProtocol::
                        kMultipleTaskAssignmentsArtifactRetrievalNetworkStats,
                    _, _, _));
    // Computation and upload for the first task from multiple task assignments.
    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(kSwor24HourTaskName));
    EXPECT_CALL(mock_phase_logger_, LogComputationStarted(kSwor24HourTaskName));
    EXPECT_CALL(
        mock_phase_logger_,
        LogComputationCompleted(FieldsAre(Gt(0), Gt(0)),
                                EqualsNetworkStats(NetworkStats()), _, _, _));
    EXPECT_CALL(mock_phase_logger_, LogResultUploadStarted())
        .WillOnce(Return(absl::OkStatus()));

    EXPECT_CALL(mock_phase_logger_,
                LogResultUploadCompleted(
                    EqualsNetworkStats(
                        MockFederatedProtocol::kReportCompletedNetworkStats),
                    _, _));

    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true))
        .RetiresOnSaturation();
    // Computation and upload for the second task from multiple task
    // assignments.
    EXPECT_CALL(mock_phase_logger_,
                SetModelIdentifier(kRequires5ExamplesTaskName));
    EXPECT_CALL(mock_phase_logger_,
                LogComputationStarted(kRequires5ExamplesTaskName));
    EXPECT_CALL(
        mock_phase_logger_,
        LogComputationCompleted(FieldsAre(Gt(0), Gt(0)),
                                EqualsNetworkStats(NetworkStats()), _, _, _));
    EXPECT_CALL(mock_phase_logger_, LogResultUploadStarted())
        .WillOnce(Return(absl::OkStatus()));

    EXPECT_CALL(mock_phase_logger_,
                LogResultUploadCompleted(
                    EqualsNetworkStats(
                        MockFederatedProtocol::kReportCompletedNetworkStats),
                    _, _));

    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true));
  }

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::SUCCESS);
  expected_result.mutable_contributed_task_names()->Add(
      std::string(kSwor24HourTaskName));
  expected_result.mutable_contributed_task_names()->Add(
      std::string(kRequires5ExamplesTaskName));
  EXPECT_THAT(*result, EqualsProto(expected_result));
  // For the mixed secagg task, there should be one checkpoint and some secagg
  // tensors.
  EXPECT_THAT(multiple_task_assignment_computation_results_1,
              Contains(Pair(kTensorflowCheckpointAggregand,
                            VariantWith<TFCheckpoint>(Not(IsEmpty())))));
  EXPECT_GT(multiple_task_assignment_computation_results_1.size(), 1);

  // For pure secagg task, there should be no checkpoint, and only secagg
  // tensors.
  EXPECT_GT(multiple_task_assignment_computation_results_2.size(), 0);
}

// This tests the case where the client issues a multiple task assignments
// request, but the server returns an empty list of tasks.
TEST_F(FlRunnerMultipleTaskAssignmentsTest, MultipleTaskAssignmentsTurnedAway) {
  SetUpEligibilityEvalTask();
  SetUpPopulationEligibilitySpec();
  MockEligibilityEvalCheckIn();

  TaskEligibilityInfo task_eligibility_info;
  task_eligibility_info.set_version(1);
  TaskWeight* task_weight = task_eligibility_info.add_task_weights();
  task_weight->set_task_name(std::string(kRequires5ExamplesTaskName));
  task_weight->set_weight(1);
  TaskWeight* task_weight_2 = task_eligibility_info.add_task_weights();
  task_weight_2->set_task_name(kTaskName);
  task_weight_2->set_weight(1);

  MockSuccessfulEligibilityPlanExecution(task_eligibility_info);
  // Mock a multiple task assignment request which returns no task.
  EXPECT_CALL(mock_federated_protocol_,
              MockPerformMultipleTaskAssignments(
                  ElementsAre(std::string(kRequires5ExamplesTaskName)), _, _))
      .WillOnce(Return(FederatedProtocol::MultipleTaskAssignments{}));
  // The client should proceed to single task assignment directly.
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));

  MockSuccessfulPlanExecution(
      /*has_checkpoint=*/true,
      /*has_secagg_output=*/false,
      EqualsProto(single_task_assignment_client_only_plan_));
  ComputationResults computation_results;
  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        computation_results = std::move(reported_results);
        return absl::OkStatus();
      });

  // Set up PhaseLogger expectations
  {
    InSequence seq;
    ExpectEligibilityEvalLogEvents();
    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
    EXPECT_CALL(mock_phase_logger_, LogMultipleTaskAssignmentsStarted());
    EXPECT_CALL(mock_phase_logger_,
                LogMultipleTaskAssignmentsTurnedAway(
                    MockFederatedProtocol::
                        kMultipleTaskAssignmentsPlanUriReceivedNetworkStats,
                    _, _));
    ExpectCheckinTrainingReportLogEvents();
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true));
  }

  // Call RunFederatedComputation and check results.
  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::PARTIAL);
  expected_result.mutable_contributed_task_names()->Add(kTaskName);
  EXPECT_THAT(*result, EqualsProto(expected_result));
  // There should be one checkpoint and no secagg results.
  EXPECT_THAT(computation_results,
              ElementsAre(Pair(kTensorflowCheckpointAggregand,
                               VariantWith<TFCheckpoint>(Not(IsEmpty())))));
}

// This tests the case where the multiple task assignments request failed with
// IO error.
TEST_F(FlRunnerMultipleTaskAssignmentsTest, MultipleTaskAssignmentsIOError) {
  SetUpEligibilityEvalTask();
  SetUpPopulationEligibilitySpec();
  MockEligibilityEvalCheckIn();

  TaskEligibilityInfo task_eligibility_info;
  task_eligibility_info.set_version(1);
  TaskWeight* task_weight = task_eligibility_info.add_task_weights();
  task_weight->set_task_name(std::string(kRequires5ExamplesTaskName));
  task_weight->set_weight(1);
  TaskWeight* task_weight_2 = task_eligibility_info.add_task_weights();
  task_weight_2->set_task_name(kTaskName);
  task_weight_2->set_weight(1);

  MockSuccessfulEligibilityPlanExecution(task_eligibility_info);
  // Mock a multiple task assignment request which fails with IO error.
  EXPECT_CALL(mock_federated_protocol_,
              MockPerformMultipleTaskAssignments(
                  ElementsAre(std::string(kRequires5ExamplesTaskName)), _, _))
      .WillOnce(Return(absl::InternalError("Something's wrong")));
  // The client should proceed to single task assignment directly.
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));

  MockSuccessfulPlanExecution(
      /*has_checkpoint=*/true,
      /*has_secagg_output=*/false,
      EqualsProto(single_task_assignment_client_only_plan_));
  ComputationResults computation_results;
  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        computation_results = std::move(reported_results);
        return absl::OkStatus();
      });

  // Set up PhaseLogger expectations
  {
    InSequence seq;
    ExpectEligibilityEvalLogEvents();
    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
    EXPECT_CALL(mock_phase_logger_, LogMultipleTaskAssignmentsStarted());
    EXPECT_CALL(mock_phase_logger_,
                LogMultipleTaskAssignmentsIOError(
                    IsCode(absl::StatusCode::kInternal),
                    MockFederatedProtocol::
                        kMultipleTaskAssignmentsPlanUriReceivedNetworkStats,
                    _, _));
    ExpectCheckinTrainingReportLogEvents();
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true));
  }

  // Call RunFederatedComputation and check results.
  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::PARTIAL);
  expected_result.mutable_contributed_task_names()->Add(kTaskName);
  EXPECT_THAT(*result, EqualsProto(expected_result));
  // There should be one checkpoint and no secagg results.
  EXPECT_THAT(computation_results,
              ElementsAre(Pair(kTensorflowCheckpointAggregand,
                               VariantWith<TFCheckpoint>(Not(IsEmpty())))));
}

// This tests the case where the multiple task assignments request is aborted by
// the server.
TEST_F(FlRunnerMultipleTaskAssignmentsTest,
       MultipleTaskAssignmentsServerAborted) {
  SetUpEligibilityEvalTask();
  SetUpPopulationEligibilitySpec();
  MockEligibilityEvalCheckIn();

  TaskEligibilityInfo task_eligibility_info;
  task_eligibility_info.set_version(1);
  TaskWeight* task_weight = task_eligibility_info.add_task_weights();
  task_weight->set_task_name(std::string(kRequires5ExamplesTaskName));
  task_weight->set_weight(1);
  TaskWeight* task_weight_2 = task_eligibility_info.add_task_weights();
  task_weight_2->set_task_name(kTaskName);
  task_weight_2->set_weight(1);

  MockSuccessfulEligibilityPlanExecution(task_eligibility_info);
  // Mock a multiple task assignment request which is aborted by the server.
  EXPECT_CALL(mock_federated_protocol_,
              MockPerformMultipleTaskAssignments(
                  ElementsAre(std::string(kRequires5ExamplesTaskName)), _, _))
      .WillOnce(Return(absl::AbortedError("Abort")));
  // The client should proceed to single task assignment directly.
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));

  MockSuccessfulPlanExecution(
      /*has_checkpoint=*/true,
      /*has_secagg_output=*/false,
      EqualsProto(single_task_assignment_client_only_plan_));

  ComputationResults computation_results;
  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        computation_results = std::move(reported_results);
        return absl::OkStatus();
      });

  // Set up PhaseLogger expectations
  {
    InSequence seq;
    ExpectEligibilityEvalLogEvents();
    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
    EXPECT_CALL(mock_phase_logger_, LogMultipleTaskAssignmentsStarted());
    EXPECT_CALL(mock_phase_logger_,
                LogMultipleTaskAssignmentsServerAborted(
                    IsCode(absl::StatusCode::kAborted),
                    MockFederatedProtocol::
                        kMultipleTaskAssignmentsPlanUriReceivedNetworkStats,
                    _, _));
    ExpectCheckinTrainingReportLogEvents();
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true));
  }

  // Call RunFederatedComputation and check results.
  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::PARTIAL);
  expected_result.mutable_contributed_task_names()->Add(kTaskName);
  EXPECT_THAT(*result, EqualsProto(expected_result));
  // There should be one checkpoint and no secagg results.
  EXPECT_THAT(computation_results,
              ElementsAre(Pair(kTensorflowCheckpointAggregand,
                               VariantWith<TFCheckpoint>(Not(IsEmpty())))));
}

// This tests the case where the multiple task assignments request is
// interrupted on the client side.
TEST_F(FlRunnerMultipleTaskAssignmentsTest,
       MultipleTaskAssignmentsClientInterrupted) {
  SetUpEligibilityEvalTask();
  SetUpPopulationEligibilitySpec();
  MockEligibilityEvalCheckIn();

  TaskEligibilityInfo task_eligibility_info;
  task_eligibility_info.set_version(1);
  TaskWeight* task_weight = task_eligibility_info.add_task_weights();
  task_weight->set_task_name(std::string(kRequires5ExamplesTaskName));
  task_weight->set_weight(1);
  TaskWeight* task_weight_2 = task_eligibility_info.add_task_weights();
  task_weight_2->set_task_name(kTaskName);
  task_weight_2->set_weight(1);

  MockSuccessfulEligibilityPlanExecution(task_eligibility_info);
  // Mock a multiple task assignment request which is cancelled by the client.
  EXPECT_CALL(mock_federated_protocol_,
              MockPerformMultipleTaskAssignments(
                  ElementsAre(std::string(kRequires5ExamplesTaskName)), _, _))
      .WillOnce(Return(absl::CancelledError("Client side cancellation.")));
  // The client should proceed to single task assignment directly.
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));

  MockSuccessfulPlanExecution(
      /*has_checkpoint=*/true,
      /*has_secagg_output=*/false,
      EqualsProto(single_task_assignment_client_only_plan_));

  ComputationResults computation_results;
  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        computation_results = std::move(reported_results);
        return absl::OkStatus();
      });

  // Set up PhaseLogger expectations
  {
    InSequence seq;
    ExpectEligibilityEvalLogEvents();
    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
    EXPECT_CALL(mock_phase_logger_, LogMultipleTaskAssignmentsStarted());
    EXPECT_CALL(mock_phase_logger_,
                LogMultipleTaskAssignmentsClientInterrupted(
                    IsCode(absl::StatusCode::kCancelled),
                    MockFederatedProtocol::
                        kMultipleTaskAssignmentsPlanUriReceivedNetworkStats,
                    _, _));
    ExpectCheckinTrainingReportLogEvents();
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true));
  }

  // Call RunFederatedComputation and check results.
  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::PARTIAL);
  expected_result.mutable_contributed_task_names()->Add(kTaskName);
  EXPECT_THAT(*result, EqualsProto(expected_result));
  // There should be one checkpoint and no secagg results.
  EXPECT_THAT(computation_results,
              ElementsAre(Pair(kTensorflowCheckpointAggregand,
                               VariantWith<TFCheckpoint>(Not(IsEmpty())))));
}

// This tests the case where the multiple task assignments request succeeded and
// the server returned the artifact uris for all the requested tasks. However,
// some uris are failed to download.
TEST_F(FlRunnerMultipleTaskAssignmentsTest,
       MultipleTaskAssignmentsUriAllReceivedPayloadIOError) {
  SetUpEligibilityEvalTask();
  SetUpPopulationEligibilitySpec();
  MockEligibilityEvalCheckIn();

  TaskEligibilityInfo task_eligibility_info;
  task_eligibility_info.set_version(1);
  TaskWeight* task_weight = task_eligibility_info.add_task_weights();
  task_weight->set_task_name(std::string(kRequires5ExamplesTaskName));
  task_weight->set_weight(1);
  TaskWeight* task_weight_2 = task_eligibility_info.add_task_weights();
  task_weight_2->set_task_name(kTaskName);
  task_weight_2->set_weight(1);

  MockSuccessfulEligibilityPlanExecution(task_eligibility_info);
  // Mock a multiple task assignment request which 1) the artifact uris for all
  // of the requested tasks are received; 2) IO error happened during the
  // retrieval of computation artifact.
  FederatedProtocol::MultipleTaskAssignments multiple_task_assignments;
  multiple_task_assignments.task_assignments[kRequires5ExamplesTaskName] =
      absl::InternalError("Something's wrong");
  EXPECT_CALL(mock_federated_protocol_,
              MockPerformMultipleTaskAssignments(
                  ElementsAre(std::string(kRequires5ExamplesTaskName)), _, _))
      .WillOnce(Return(std::move(multiple_task_assignments)));
  // The client should proceed to single task assignment directly.
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));

  MockSuccessfulPlanExecution(
      /*has_checkpoint=*/true,
      /*has_secagg_output=*/false,
      EqualsProto(single_task_assignment_client_only_plan_));

  ComputationResults computation_results;
  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        computation_results = std::move(reported_results);
        return absl::OkStatus();
      });

  // Set up PhaseLogger expectations
  {
    InSequence seq;
    ExpectEligibilityEvalLogEvents();
    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
    EXPECT_CALL(mock_phase_logger_, LogMultipleTaskAssignmentsStarted());
    EXPECT_CALL(mock_phase_logger_,
                LogMultipleTaskAssignmentsPlanUriReceived(
                    MockFederatedProtocol::
                        kMultipleTaskAssignmentsPlanUriReceivedNetworkStats,
                    _));
    EXPECT_CALL(mock_phase_logger_,
                SetModelIdentifier(kRequires5ExamplesTaskName));
    EXPECT_CALL(mock_phase_logger_, LogMultipleTaskAssignmentsPayloadIOError(
                                        IsCode(absl::StatusCode::kInternal)));
    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
    EXPECT_CALL(mock_phase_logger_,
                LogMultipleTaskAssignmentsPartialCompleted(
                    MockFederatedProtocol::
                        kMultipleTaskAssignmentsArtifactRetrievalNetworkStats,
                    _, _, _));
    ExpectCheckinTrainingReportLogEvents();
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true));
  }

  // Call RunFederatedComputation and check results.
  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::PARTIAL);
  expected_result.mutable_contributed_task_names()->Add(kTaskName);
  EXPECT_THAT(*result, EqualsProto(expected_result));
  // There should be one checkpoint and no secagg results.
  EXPECT_THAT(computation_results,
              ElementsAre(Pair(kTensorflowCheckpointAggregand,
                               VariantWith<TFCheckpoint>(Not(IsEmpty())))));
}

// This tests the case where the multiple task assignments request succeeded and
// the server returned uris for all the requested tasks. However, invalid
// payloads were downloaded from some of the uris..
TEST_F(FlRunnerMultipleTaskAssignmentsTest,
       MultipleTaskAssignmentsUriAllReceivedInvalidPayloadError) {
  SetUpEligibilityEvalTask();
  SetUpPopulationEligibilitySpec();
  MockEligibilityEvalCheckIn();

  TaskEligibilityInfo task_eligibility_info;
  task_eligibility_info.set_version(1);
  TaskWeight* task_weight = task_eligibility_info.add_task_weights();
  task_weight->set_task_name(std::string(kRequires5ExamplesTaskName));
  task_weight->set_weight(1);
  TaskWeight* task_weight_2 = task_eligibility_info.add_task_weights();
  task_weight_2->set_task_name(kTaskName);
  task_weight_2->set_weight(1);

  MockSuccessfulEligibilityPlanExecution(task_eligibility_info);

  // Mock a multiple task assignment request which 1) the artifact uris for all
  // of the requested tasks are received; 2) one of the downloaded artifacts is
  // invalid.
  FederatedProtocol::TaskAssignment invalid_task_assignment =
      task_assignment_2_;
  invalid_task_assignment.payloads.plan = "INVALID_PLAN";
  FederatedProtocol::MultipleTaskAssignments multiple_task_assignments;
  multiple_task_assignments.task_assignments[kRequires5ExamplesTaskName] =
      invalid_task_assignment;
  EXPECT_CALL(mock_federated_protocol_,
              MockPerformMultipleTaskAssignments(
                  ElementsAre(std::string(kRequires5ExamplesTaskName)), _, _))
      .WillOnce(Return(std::move(multiple_task_assignments)));
  // The client should proceed to single task assignment directly.
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));

  MockSuccessfulPlanExecution(
      /*has_checkpoint=*/true,
      /*has_secagg_output=*/false,
      EqualsProto(single_task_assignment_client_only_plan_));

  ComputationResults computation_results;
  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        computation_results = std::move(reported_results);
        return absl::OkStatus();
      });

  // Set up PhaseLogger expectations
  {
    InSequence seq;
    ExpectEligibilityEvalLogEvents();
    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
    EXPECT_CALL(mock_phase_logger_, LogMultipleTaskAssignmentsStarted());
    EXPECT_CALL(mock_phase_logger_,
                LogMultipleTaskAssignmentsPlanUriReceived(
                    MockFederatedProtocol::
                        kMultipleTaskAssignmentsPlanUriReceivedNetworkStats,
                    _));
    EXPECT_CALL(mock_phase_logger_,
                SetModelIdentifier(kRequires5ExamplesTaskName));
    EXPECT_CALL(mock_phase_logger_,
                LogMultipleTaskAssignmentsInvalidPayload(_));
    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
    EXPECT_CALL(mock_phase_logger_,
                LogMultipleTaskAssignmentsPartialCompleted(
                    MockFederatedProtocol::
                        kMultipleTaskAssignmentsArtifactRetrievalNetworkStats,
                    _, _, _));
    ExpectCheckinTrainingReportLogEvents();
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true));
  }

  // Call RunFederatedComputation and check results.
  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::PARTIAL);
  expected_result.mutable_contributed_task_names()->Add(kTaskName);
  EXPECT_THAT(*result, EqualsProto(expected_result));
  // There should be one checkpoint and no secagg results.
  EXPECT_THAT(computation_results,
              ElementsAre(Pair(kTensorflowCheckpointAggregand,
                               VariantWith<TFCheckpoint>(Not(IsEmpty())))));
}

// This tests the case where the multiple task assignments request succeeded,
// but the server didn't assign all the requested tasks to the client.
TEST_F(FlRunnerMultipleTaskAssignmentsTest,
       MultipleTaskAssignmentsPartialUriReceived) {
  SetUpEligibilityEvalTask();
  SetUpPopulationEligibilitySpec();
  MockEligibilityEvalCheckIn();

  TaskEligibilityInfo task_eligibility_info;
  task_eligibility_info.set_version(1);
  TaskWeight* task_weight = task_eligibility_info.add_task_weights();
  task_weight->set_task_name(std::string(kSwor24HourTaskName));
  task_weight->set_weight(1);
  TaskWeight* task_weight_2 = task_eligibility_info.add_task_weights();
  task_weight_2->set_task_name(std::string(kRequires5ExamplesTaskName));
  task_weight_2->set_weight(1);
  TaskWeight* task_weight_3 = task_eligibility_info.add_task_weights();
  task_weight_3->set_task_name(kTaskName);
  task_weight_3->set_weight(1);

  MockSuccessfulEligibilityPlanExecution(task_eligibility_info);

  // Mock a multiple task assignment request which the server assigned some of
  // the requested tasks, not all.
  FederatedProtocol::MultipleTaskAssignments multiple_task_assignments;
  multiple_task_assignments.task_assignments[kSwor24HourTaskName] =
      task_assignment_1_;
  EXPECT_CALL(mock_federated_protocol_,
              MockPerformMultipleTaskAssignments(
                  ElementsAre(std::string(kSwor24HourTaskName),
                              std::string(kRequires5ExamplesTaskName)),
                  _, _))
      .WillOnce(Return(std::move(multiple_task_assignments)));
  ComputationResults multiple_task_assignment_computation_results_1;
  EXPECT_CALL(
      mock_federated_protocol_,
      MockReportCompleted(_, _, Eq(task_assignment_1_.task_identifier), _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        multiple_task_assignment_computation_results_1 =
            std::move(reported_results);
        return absl::OkStatus();
      });

  MockSuccessfulPlanExecution(/*has_checkpoint=*/true,
                              /*has_secagg_output=*/true, EqualsProto(plan_1_));

  // The client should proceed to single task assignment.
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));

  MockSuccessfulPlanExecution(
      /*has_checkpoint=*/true,
      /*has_secagg_output=*/false,
      EqualsProto(single_task_assignment_client_only_plan_));

  ComputationResults single_task_assignment_computation_results;
  EXPECT_CALL(mock_federated_protocol_,
              MockReportCompleted(_, _, Eq(std::nullopt), _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        single_task_assignment_computation_results =
            std::move(reported_results);
        return absl::OkStatus();
      });

  // Set up PhaseLogger expectations
  {
    InSequence seq;
    ExpectEventsForEligibilityEvalComputationWithThreeEligibleTasks();
    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
    EXPECT_CALL(mock_phase_logger_, LogMultipleTaskAssignmentsStarted());
    EXPECT_CALL(mock_phase_logger_,
                LogMultipleTaskAssignmentsPlanUriPartialReceived(
                    MockFederatedProtocol::
                        kMultipleTaskAssignmentsPlanUriReceivedNetworkStats,
                    _));
    EXPECT_CALL(mock_phase_logger_,
                LogMultipleTaskAssignmentsCompleted(
                    MockFederatedProtocol::
                        kMultipleTaskAssignmentsArtifactRetrievalNetworkStats,
                    _, _, _));
    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(kSwor24HourTaskName));
    EXPECT_CALL(mock_phase_logger_, LogComputationStarted(kSwor24HourTaskName));
    EXPECT_CALL(
        mock_phase_logger_,
        LogComputationCompleted(FieldsAre(Gt(0), Gt(0)),
                                EqualsNetworkStats(NetworkStats()), _, _, _));
    EXPECT_CALL(mock_phase_logger_, LogResultUploadStarted())
        .WillOnce(Return(absl::OkStatus()));

    EXPECT_CALL(mock_phase_logger_,
                LogResultUploadCompleted(
                    EqualsNetworkStats(
                        MockFederatedProtocol::kReportCompletedNetworkStats),
                    _, _));
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true))
        .RetiresOnSaturation();

    ExpectCheckinTrainingReportLogEvents();

    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true));
  }

  // Call RunFederatedComputation and check results.
  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::PARTIAL);
  expected_result.mutable_contributed_task_names()->Add(
      std::string(kSwor24HourTaskName));
  expected_result.mutable_contributed_task_names()->Add(kTaskName);
  EXPECT_THAT(*result, EqualsProto(expected_result));
  // For the mixed secagg task, there should be one checkpoint and some secagg
  // tensors.
  EXPECT_THAT(multiple_task_assignment_computation_results_1,
              Contains(Pair(kTensorflowCheckpointAggregand,
                            VariantWith<TFCheckpoint>(Not(IsEmpty())))));
  EXPECT_GT(multiple_task_assignment_computation_results_1.size(), 1);
  // For the single task assignment, there should be one checkpoint and no
  // secagg results.
  EXPECT_THAT(single_task_assignment_computation_results,
              ElementsAre(Pair(kTensorflowCheckpointAggregand,
                               VariantWith<TFCheckpoint>(Not(IsEmpty())))));
}

// This tests the case where the multiple task assignments request succeeded,
// the server assigned all the requested tasks and all the artifacts are also
// successfully downloaded
TEST_F(FlRunnerMultipleTaskAssignmentsTest,
       MultipleTaskAssignmentsUriAllReceivedPayloadRetrievalAllSucceeded) {
  SetUpEligibilityEvalTask();
  SetUpPopulationEligibilitySpec();
  MockEligibilityEvalCheckIn();

  TaskEligibilityInfo task_eligibility_info;
  task_eligibility_info.set_version(1);
  TaskWeight* task_weight = task_eligibility_info.add_task_weights();
  task_weight->set_task_name(std::string(kSwor24HourTaskName));
  task_weight->set_weight(1);
  TaskWeight* task_weight_2 = task_eligibility_info.add_task_weights();
  task_weight_2->set_task_name(std::string(kRequires5ExamplesTaskName));
  task_weight_2->set_weight(1);
  TaskWeight* task_weight_3 = task_eligibility_info.add_task_weights();
  task_weight_3->set_task_name(kTaskName);
  task_weight_3->set_weight(1);

  MockSuccessfulEligibilityPlanExecution(task_eligibility_info);

  // Mock a successful multiple task assignment request.
  FederatedProtocol::MultipleTaskAssignments multiple_task_assignments;
  multiple_task_assignments.task_assignments[kSwor24HourTaskName] =
      task_assignment_1_;
  multiple_task_assignments.task_assignments[kRequires5ExamplesTaskName] =
      task_assignment_2_;
  EXPECT_CALL(mock_federated_protocol_,
              MockPerformMultipleTaskAssignments(
                  ElementsAre(std::string(kSwor24HourTaskName),
                              std::string(kRequires5ExamplesTaskName)),
                  _, _))
      .WillOnce(Return(std::move(multiple_task_assignments)));
  ComputationResults multiple_task_assignment_computation_results_1;
  EXPECT_CALL(
      mock_federated_protocol_,
      MockReportCompleted(_, _, Eq(task_assignment_1_.task_identifier), _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        multiple_task_assignment_computation_results_1 =
            std::move(reported_results);
        return absl::OkStatus();
      });
  ComputationResults multiple_task_assignment_computation_results_2;
  EXPECT_CALL(
      mock_federated_protocol_,
      MockReportCompleted(_, _, Eq(task_assignment_2_.task_identifier), _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        multiple_task_assignment_computation_results_2 =
            std::move(reported_results);
        return absl::OkStatus();
      });

  MockSuccessfulPlanExecution(/*has_checkpoint=*/true,
                              /*has_secagg_output=*/true, EqualsProto(plan_1_));

  MockSuccessfulPlanExecution(/*has_checkpoint=*/false,
                              /*has_secagg_output=*/true, EqualsProto(plan_2_));

  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));

  MockSuccessfulPlanExecution(
      /*has_checkpoint=*/true,
      /*has_secagg_output=*/false,
      EqualsProto(single_task_assignment_client_only_plan_));

  ComputationResults single_task_assignment_computation_results;
  EXPECT_CALL(mock_federated_protocol_,
              MockReportCompleted(_, _, Eq(std::nullopt), _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        single_task_assignment_computation_results =
            std::move(reported_results);
        return absl::OkStatus();
      });

  // Set up PhaseLogger expectations
  {
    InSequence seq;
    ExpectEventsForEligibilityEvalComputationWithThreeEligibleTasks();
    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
    EXPECT_CALL(mock_phase_logger_, LogMultipleTaskAssignmentsStarted());
    EXPECT_CALL(mock_phase_logger_,
                LogMultipleTaskAssignmentsPlanUriReceived(
                    MockFederatedProtocol::
                        kMultipleTaskAssignmentsPlanUriReceivedNetworkStats,
                    _));
    EXPECT_CALL(mock_phase_logger_,
                LogMultipleTaskAssignmentsCompleted(
                    MockFederatedProtocol::
                        kMultipleTaskAssignmentsArtifactRetrievalNetworkStats,
                    _, _, _));
    // Computation and upload for the first task from multiple task assignments.
    EXPECT_CALL(mock_phase_logger_,
                SetModelIdentifier(kRequires5ExamplesTaskName));
    EXPECT_CALL(mock_phase_logger_,
                LogComputationStarted(kRequires5ExamplesTaskName));
    EXPECT_CALL(
        mock_phase_logger_,
        LogComputationCompleted(FieldsAre(Gt(0), Gt(0)),
                                EqualsNetworkStats(NetworkStats()), _, _, _));
    EXPECT_CALL(mock_phase_logger_, LogResultUploadStarted())
        .WillOnce(Return(absl::OkStatus()));

    EXPECT_CALL(mock_phase_logger_,
                LogResultUploadCompleted(
                    EqualsNetworkStats(
                        MockFederatedProtocol::kReportCompletedNetworkStats),
                    _, _));
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true))
        .RetiresOnSaturation();
    // Computation and upload for the second task from multiple task
    // assignments.
    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(kSwor24HourTaskName));
    EXPECT_CALL(mock_phase_logger_, LogComputationStarted(kSwor24HourTaskName));
    EXPECT_CALL(
        mock_phase_logger_,
        LogComputationCompleted(FieldsAre(Gt(0), Gt(0)),
                                EqualsNetworkStats(NetworkStats()), _, _, _));
    EXPECT_CALL(mock_phase_logger_, LogResultUploadStarted())
        .WillOnce(Return(absl::OkStatus()));

    EXPECT_CALL(mock_phase_logger_,
                LogResultUploadCompleted(
                    EqualsNetworkStats(
                        MockFederatedProtocol::kReportCompletedNetworkStats),
                    _, _));

    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true))
        .RetiresOnSaturation();
    // Check-in, computation and upload for the regular task assignment.
    ExpectCheckinTrainingReportLogEvents();

    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true));
  }

  // Call RunFederatedComputation and check results.
  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::SUCCESS);
  expected_result.mutable_contributed_task_names()->Add(
      std::string(kRequires5ExamplesTaskName));
  expected_result.mutable_contributed_task_names()->Add(
      std::string(kSwor24HourTaskName));
  expected_result.mutable_contributed_task_names()->Add(kTaskName);
  EXPECT_THAT(*result, EqualsProto(expected_result));
  // For the mixed secagg task, there should be one checkpoint and some secagg
  // tensors.
  EXPECT_THAT(multiple_task_assignment_computation_results_1,
              Contains(Pair(kTensorflowCheckpointAggregand,
                            VariantWith<TFCheckpoint>(Not(IsEmpty())))));
  EXPECT_GT(multiple_task_assignment_computation_results_1.size(), 1);

  // For pure secagg task, there should be no checkpoint, and only secagg
  // tensors.
  EXPECT_GT(multiple_task_assignment_computation_results_2.size(), 0);

  // For the single task assignment, there should be one checkpoint and no
  // secagg results.
  EXPECT_THAT(single_task_assignment_computation_results,
              ElementsAre(Pair(kTensorflowCheckpointAggregand,
                               VariantWith<TFCheckpoint>(Not(IsEmpty())))));
}

// This tests the case where the multiple task assignments request succeeded,
// the server assigned all the requested tasks and all the artifacts are also
// successfully downloaded. Single task assignment is not supported in the
// population, so regular check-in is not expected.
TEST_F(FlRunnerMultipleTaskAssignmentsTest,
       MultipleTaskAssignmentsSucceededRegularCheckInDisabled) {
  SetUpEligibilityEvalTask();

  // Mock a successful eligibility eval checkin.
  // Population only supports multiple task assignments.
  PopulationEligibilitySpec population_spec;
  auto task_info_1 = population_spec.add_task_info();
  task_info_1->set_task_name(std::string(kRequires5ExamplesTaskName));
  task_info_1->set_task_assignment_mode(
      PopulationEligibilitySpec::TaskInfo::TASK_ASSIGNMENT_MODE_MULTIPLE);
  task_info_1->add_eligibility_policy_indices(0);
  auto policy = population_spec.add_eligibility_policies();
  policy->set_name("custom_tf_policy");
  *policy->mutable_tf_custom_policy()->mutable_arguments() = "tfl_policy_args";
  eligibility_eval_artifacts_.population_eligibility_spec = population_spec;

  EXPECT_CALL(mock_federated_protocol_, MockEligibilityEvalCheckin())
      .WillOnce(Return(FederatedProtocol::EligibilityEvalTask{
          {eligibility_eval_artifacts_.plan.SerializeAsString(),
           eligibility_eval_artifacts_.checkpoint},
          kEligibilityEvalExecutionId,
          population_spec}));

  TaskEligibilityInfo task_eligibility_info;
  task_eligibility_info.set_version(1);
  TaskWeight* task_weight = task_eligibility_info.add_task_weights();
  task_weight->set_task_name(std::string(kRequires5ExamplesTaskName));
  task_weight->set_weight(1);

  MockSuccessfulEligibilityPlanExecution(task_eligibility_info);

  // Mock a successful multiple task assignment request.
  FederatedProtocol::MultipleTaskAssignments multiple_task_assignments;
  multiple_task_assignments.task_assignments[kRequires5ExamplesTaskName] =
      task_assignment_2_;
  EXPECT_CALL(mock_federated_protocol_,
              MockPerformMultipleTaskAssignments(
                  ElementsAre(std::string(kRequires5ExamplesTaskName)), _, _))
      .WillOnce(Return(std::move(multiple_task_assignments)));

  ComputationResults multiple_task_assignment_computation_results_1;
  EXPECT_CALL(
      mock_federated_protocol_,
      MockReportCompleted(_, _, Eq(task_assignment_2_.task_identifier), _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> aggregation_session_id,
                    std::optional<PayloadMetadata> payload_metadata) {
        multiple_task_assignment_computation_results_1 =
            std::move(reported_results);
        return absl::OkStatus();
      });

  MockSuccessfulPlanExecution(/*has_checkpoint=*/true,
                              /*has_secagg_output=*/true, EqualsProto(plan_2_));
  // Set up PhaseLogger expectations
  {
    InSequence seq;
    ExpectEventsForEligibilityEvalComputationWithThreeEligibleTasks();
    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
    EXPECT_CALL(mock_phase_logger_, LogMultipleTaskAssignmentsStarted());
    EXPECT_CALL(mock_phase_logger_,
                LogMultipleTaskAssignmentsPlanUriReceived(
                    MockFederatedProtocol::
                        kMultipleTaskAssignmentsPlanUriReceivedNetworkStats,
                    _));
    EXPECT_CALL(mock_phase_logger_,
                LogMultipleTaskAssignmentsCompleted(
                    MockFederatedProtocol::
                        kMultipleTaskAssignmentsArtifactRetrievalNetworkStats,
                    _, _, _));
    // Computation and upload for the first task from multiple task assignments.
    EXPECT_CALL(mock_phase_logger_,
                SetModelIdentifier(kRequires5ExamplesTaskName));
    EXPECT_CALL(mock_phase_logger_,
                LogComputationStarted(kRequires5ExamplesTaskName));
    EXPECT_CALL(
        mock_phase_logger_,
        LogComputationCompleted(FieldsAre(Gt(0), Gt(0)),
                                EqualsNetworkStats(NetworkStats()), _, _, _));
    EXPECT_CALL(mock_phase_logger_, LogResultUploadStarted())
        .WillOnce(Return(absl::OkStatus()));

    EXPECT_CALL(mock_phase_logger_,
                LogResultUploadCompleted(
                    EqualsNetworkStats(
                        MockFederatedProtocol::kReportCompletedNetworkStats),
                    _, _));
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true));
  }

  // Call RunFederatedComputation and check results.
  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::SUCCESS);
  expected_result.mutable_contributed_task_names()->Add(
      std::string(kRequires5ExamplesTaskName));
  EXPECT_THAT(*result, EqualsProto(expected_result));
  // For the mixed secagg task, there should be one checkpoint and some secagg
  // tensors.
  EXPECT_THAT(multiple_task_assignment_computation_results_1,
              Contains(Pair(kTensorflowCheckpointAggregand,
                            VariantWith<TFCheckpoint>(Not(IsEmpty())))));
  EXPECT_GT(multiple_task_assignment_computation_results_1.size(), 1);
}

// This tests the case where the multiple task assignments request succeeded,
// the server assigned all the requested tasks and all the artifacts are also
// successfully downloaded. Single task assignment is not supported in the
// population, so regular check-in is not expected.  Create_task_identifier is
// enabled.
TEST_F(FlRunnerMultipleTaskAssignmentsTest,
       MultipleTaskAssignmentsSucceededCreateTaskIdentifier) {
  SetUpEligibilityEvalTask();

  // Mock a successful eligibility eval checkin.
  // Population only supports multiple task assignments.
  PopulationEligibilitySpec population_spec;
  auto task_info_1 = population_spec.add_task_info();
  task_info_1->set_task_name(std::string(kSwor24HourTaskName));
  task_info_1->set_task_assignment_mode(
      PopulationEligibilitySpec::TaskInfo::TASK_ASSIGNMENT_MODE_MULTIPLE);
  auto task_info_2 = population_spec.add_task_info();
  task_info_2->set_task_name(std::string(kRequires5ExamplesTaskName));
  task_info_2->set_task_assignment_mode(
      PopulationEligibilitySpec::TaskInfo::TASK_ASSIGNMENT_MODE_MULTIPLE);
  EXPECT_CALL(mock_federated_protocol_, MockEligibilityEvalCheckin())
      .WillOnce(Return(FederatedProtocol::EligibilityEvalTask{
          {eligibility_eval_artifacts_.plan.SerializeAsString(),
           eligibility_eval_artifacts_.checkpoint},
          kEligibilityEvalExecutionId,
          population_spec}));

  TaskEligibilityInfo task_eligibility_info;
  task_eligibility_info.set_version(1);
  TaskWeight* task_weight_1 = task_eligibility_info.add_task_weights();
  task_weight_1->set_task_name(std::string(kSwor24HourTaskName));
  task_weight_1->set_weight(1);
  TaskWeight* task_weight_2 = task_eligibility_info.add_task_weights();
  task_weight_2->set_task_name(std::string(kRequires5ExamplesTaskName));
  task_weight_2->set_weight(1);

  MockSuccessfulEligibilityPlanExecution(task_eligibility_info);

  // Mock a successful multiple task assignment request.
  FederatedProtocol::MultipleTaskAssignments multiple_task_assignments;
  multiple_task_assignments.task_assignments[kSwor24HourTaskName] =
      task_assignment_1_;
  multiple_task_assignments.task_assignments[kRequires5ExamplesTaskName] =
      task_assignment_2_;
  // Let the second task share the same aggregation session id as the first.
  task_assignment_2_.aggregation_session_id =
      kMultipleTaskAggregationSessionId1;
  EXPECT_CALL(mock_federated_protocol_,
              MockPerformMultipleTaskAssignments(
                  ElementsAre(std::string(kSwor24HourTaskName),
                              std::string(kRequires5ExamplesTaskName)),
                  _, _))
      .WillOnce(Return(std::move(multiple_task_assignments)));

  MockSuccessfulPlanExecution(/*has_checkpoint=*/true,
                              /*has_secagg_output=*/true, EqualsProto(plan_1_));

  MockSuccessfulPlanExecution(/*has_checkpoint=*/false,
                              /*has_secagg_output=*/true, EqualsProto(plan_2_));

  ComputationResults multiple_task_assignment_computation_results_1;
  ComputationResults multiple_task_assignment_computation_results_2;
  EXPECT_CALL(mock_federated_protocol_,
              MockReportCompleted(_, _, Eq(kTaskIdentifier1), _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> task_identifier,
                    std::optional<PayloadMetadata> payload_metadata) {
        multiple_task_assignment_computation_results_1 =
            std::move(reported_results);
        return absl::OkStatus();
      });
  EXPECT_CALL(mock_federated_protocol_,
              MockReportCompleted(_, _, Eq(kTaskIdentifier2), _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> task_identifier,
                    std::optional<PayloadMetadata> payload_metadata) {
        multiple_task_assignment_computation_results_2 =
            std::move(reported_results);
        return absl::OkStatus();
      });

  // Set up PhaseLogger expectations
  {
    InSequence seq;
    ExpectEventsForEligibilityEvalComputationWithThreeEligibleTasks();
    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
    EXPECT_CALL(mock_phase_logger_, LogMultipleTaskAssignmentsStarted());
    EXPECT_CALL(mock_phase_logger_,
                LogMultipleTaskAssignmentsPlanUriReceived(
                    MockFederatedProtocol::
                        kMultipleTaskAssignmentsPlanUriReceivedNetworkStats,
                    _));
    EXPECT_CALL(mock_phase_logger_,
                LogMultipleTaskAssignmentsCompleted(
                    MockFederatedProtocol::
                        kMultipleTaskAssignmentsArtifactRetrievalNetworkStats,
                    _, _, _));

    EXPECT_CALL(mock_phase_logger_,
                SetModelIdentifier(kRequires5ExamplesTaskName));
    EXPECT_CALL(mock_phase_logger_,
                LogComputationStarted(kRequires5ExamplesTaskName));
    EXPECT_CALL(
        mock_phase_logger_,
        LogComputationCompleted(FieldsAre(Gt(0), Gt(0)),
                                EqualsNetworkStats(NetworkStats()), _, _, _));
    EXPECT_CALL(mock_phase_logger_, LogResultUploadStarted())
        .WillOnce(Return(absl::OkStatus()));

    EXPECT_CALL(mock_phase_logger_,
                LogResultUploadCompleted(
                    EqualsNetworkStats(
                        MockFederatedProtocol::kReportCompletedNetworkStats),
                    _, _));

    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true))
        .RetiresOnSaturation();

    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(kSwor24HourTaskName));
    EXPECT_CALL(mock_phase_logger_, LogComputationStarted(kSwor24HourTaskName));
    EXPECT_CALL(
        mock_phase_logger_,
        LogComputationCompleted(FieldsAre(Gt(0), Gt(0)),
                                EqualsNetworkStats(NetworkStats()), _, _, _));
    EXPECT_CALL(mock_phase_logger_, LogResultUploadStarted())
        .WillOnce(Return(absl::OkStatus()));

    EXPECT_CALL(mock_phase_logger_,
                LogResultUploadCompleted(
                    EqualsNetworkStats(
                        MockFederatedProtocol::kReportCompletedNetworkStats),
                    _, _));
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true))
        .RetiresOnSaturation();
  }

  // Call RunFederatedComputation, we don't check results in this test because
  // we have already set expectations above.
  ASSERT_OK(RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_));
}

// This tests the case where the multiple task assignments request succeeded,
// the server assigned all the requested tasks and all the artifacts are also
// successfully downloaded. Task completion callback is called every time a task
// completes.
TEST_F(FlRunnerMultipleTaskAssignmentsTest,
       TaskCompletionCallbackEnabledAllTasksSucceeded) {
  SetUpEligibilityEvalTask();
  SetUpPopulationEligibilitySpec();
  MockEligibilityEvalCheckIn();

  TaskEligibilityInfo task_eligibility_info;
  task_eligibility_info.set_version(1);
  TaskWeight* task_weight_1 = task_eligibility_info.add_task_weights();
  task_weight_1->set_task_name(std::string(kSwor24HourTaskName));
  task_weight_1->set_weight(1);
  TaskWeight* task_weight_2 = task_eligibility_info.add_task_weights();
  task_weight_2->set_task_name(std::string(kRequires5ExamplesTaskName));
  task_weight_2->set_weight(1);
  TaskWeight* task_weight_3 = task_eligibility_info.add_task_weights();
  task_weight_3->set_task_name(kTaskName);
  task_weight_3->set_weight(1);

  MockSuccessfulEligibilityPlanExecution(task_eligibility_info);

  // Mock a successful multiple task assignment request.
  FederatedProtocol::MultipleTaskAssignments multiple_task_assignments;
  multiple_task_assignments.task_assignments[kSwor24HourTaskName] =
      task_assignment_1_;
  multiple_task_assignments.task_assignments[kRequires5ExamplesTaskName] =
      task_assignment_2_;
  EXPECT_CALL(mock_federated_protocol_,
              MockPerformMultipleTaskAssignments(
                  ElementsAre(std::string(kSwor24HourTaskName),
                              std::string(kRequires5ExamplesTaskName)),
                  _, _))
      .WillOnce(Return(std::move(multiple_task_assignments)));
  EXPECT_CALL(
      mock_federated_protocol_,
      MockReportCompleted(_, _, Eq(task_assignment_1_.task_identifier), _))
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(
      mock_federated_protocol_,
      MockReportCompleted(_, _, Eq(task_assignment_2_.task_identifier), _))
      .WillOnce(Return(absl::OkStatus()));

  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));

  EXPECT_CALL(mock_federated_protocol_,
              MockReportCompleted(_, _, Eq(std::nullopt), _))
      .WillOnce(Return(absl::OkStatus()));

  // We expect OnTaskCompleted called 3 times with success TaskResultInfo.
  EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
      .Times(3)
      .WillRepeatedly(Return(true));

  MockSuccessfulPlanExecution(/*has_checkpoint=*/true,
                              /*has_secagg_output=*/true, EqualsProto(plan_1_));

  MockSuccessfulPlanExecution(/*has_checkpoint=*/false,
                              /*has_secagg_output=*/true, EqualsProto(plan_2_));
  MockSuccessfulPlanExecution(
      /*has_checkpoint=*/true,
      /*has_secagg_output=*/false,
      EqualsProto(single_task_assignment_client_only_plan_));

  // Call RunFederatedComputation and check results.
  // Use a NiceMock because we don't care about PhaseLogger behavior in this
  // test.
  NiceMock<MockPhaseLogger> phase_logger;
  // Whenever opstats is given new network stats we store them in a variable
  // for inspection at the end of the test.
  EXPECT_CALL(phase_logger, UpdateRetryWindowAndNetworkStats(_, _))
      .WillRepeatedly(DoAll(SaveArg<0>(&latest_opstats_retry_window_),
                            SaveArg<1>(&logged_network_stats_)));
  ASSERT_OK(RunFederatedComputation(
      &mock_task_env_, phase_logger, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_));
}

// This tests the case where the multiple task assignments request succeeded,
// One of the task failed during computation. Task completion callback is called
// every time a task completes.
TEST_F(FlRunnerMultipleTaskAssignmentsTest,
       TaskCompletionCallbackEnabledOneMTATaskComputationFailed) {
  SetUpEligibilityEvalTask();
  SetUpPopulationEligibilitySpec();
  MockEligibilityEvalCheckIn();

  TaskEligibilityInfo task_eligibility_info;
  task_eligibility_info.set_version(1);
  TaskWeight* task_weight_1 = task_eligibility_info.add_task_weights();
  task_weight_1->set_task_name(std::string(kSwor24HourTaskName));
  task_weight_1->set_weight(1);
  TaskWeight* task_weight_2 = task_eligibility_info.add_task_weights();
  task_weight_2->set_task_name(std::string(kRequires5ExamplesTaskName));
  task_weight_2->set_weight(1);
  TaskWeight* task_weight_3 = task_eligibility_info.add_task_weights();
  task_weight_3->set_task_name(kTaskName);
  task_weight_3->set_weight(1);

  MockSuccessfulEligibilityPlanExecution(task_eligibility_info);

  // Mock a successful multiple task assignment request.
  FederatedProtocol::MultipleTaskAssignments multiple_task_assignments;
  multiple_task_assignments.task_assignments[kSwor24HourTaskName] =
      task_assignment_1_;
  multiple_task_assignments.task_assignments[kRequires5ExamplesTaskName] =
      task_assignment_2_;
  EXPECT_CALL(mock_federated_protocol_,
              MockPerformMultipleTaskAssignments(
                  ElementsAre(std::string(kSwor24HourTaskName),
                              std::string(kRequires5ExamplesTaskName)),

                  _, _))
      .WillOnce(Return(std::move(multiple_task_assignments)));
  EXPECT_CALL(
      mock_federated_protocol_,
      MockReportCompleted(_, _, Eq(task_assignment_1_.task_identifier), _))
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(
      mock_federated_protocol_,
      MockReportNotCompleted(_, _, Eq(task_assignment_2_.task_identifier)))
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));

  EXPECT_CALL(mock_federated_protocol_,
              MockReportCompleted(_, _, Eq(std::nullopt), _))
      .WillOnce(Return(absl::OkStatus()));

  MockSuccessfulPlanExecution(/*has_checkpoint=*/true,
                              /*has_secagg_output=*/true, EqualsProto(plan_1_));

  engine::PlanResult plan_result(engine::PlanOutcome::kInvalidArgument,
                                 absl::InvalidArgumentError("Invalid Plan."));
  PlanResultAndCheckpointFile plan_result_and_checkpoint_file(
      std::move(plan_result));
  EXPECT_CALL(*mock_tensorflow_runner_,
              RunPlanWithTensorflowSpec(_, _, _, _, _, _, EqualsProto(plan_2_),
                                        _, _, _))
      .WillOnce(Return(std::move(plan_result_and_checkpoint_file)))
      .RetiresOnSaturation();
  MockSuccessfulPlanExecution(
      /*has_checkpoint=*/true,
      /*has_secagg_output=*/false,
      EqualsProto(single_task_assignment_client_only_plan_));

  // We expect OnTaskCompleted called 3 times for both success and failure tasks
  {
    InSequence seq;
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultFailure()))
        .WillOnce(Return(true))
        .RetiresOnSaturation();
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true))
        .RetiresOnSaturation();
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true));
  }

  // Call RunFederatedComputation and check results.
  // Use a NiceMock because we don't care about PhaseLogger behavior in this
  // test.
  NiceMock<MockPhaseLogger> phase_logger;
  // Whenever opstats is given new network stats we store them in a variable
  // for inspection at the end of the test.
  EXPECT_CALL(phase_logger, UpdateRetryWindowAndNetworkStats(_, _))
      .WillRepeatedly(DoAll(SaveArg<0>(&latest_opstats_retry_window_),
                            SaveArg<1>(&logged_network_stats_)));
  ASSERT_OK(RunFederatedComputation(
      &mock_task_env_, phase_logger, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_));
}

// This tests the case where the multiple task assignments request succeeded,
// the server assigned all the requested tasks and all the artifacts are also
// successfully downloaded. One of the task failed during upload phase.
// Task completion callback is called every time a task completes.
TEST_F(FlRunnerMultipleTaskAssignmentsTest,
       TaskCompletionCallbackEnabledUploadFailed) {
  SetUpEligibilityEvalTask();
  SetUpPopulationEligibilitySpec();
  MockEligibilityEvalCheckIn();

  TaskEligibilityInfo task_eligibility_info;
  task_eligibility_info.set_version(1);
  TaskWeight* task_weight_1 = task_eligibility_info.add_task_weights();
  task_weight_1->set_task_name(std::string(kSwor24HourTaskName));
  task_weight_1->set_weight(1);
  TaskWeight* task_weight_2 = task_eligibility_info.add_task_weights();
  task_weight_2->set_task_name(std::string(kRequires5ExamplesTaskName));
  task_weight_2->set_weight(1);
  TaskWeight* task_weight_3 = task_eligibility_info.add_task_weights();
  task_weight_3->set_task_name(kTaskName);
  task_weight_3->set_weight(1);

  MockSuccessfulEligibilityPlanExecution(task_eligibility_info);

  // Mock a successful multiple task assignment request.
  FederatedProtocol::MultipleTaskAssignments multiple_task_assignments;
  multiple_task_assignments.task_assignments[kSwor24HourTaskName] =
      task_assignment_1_;
  multiple_task_assignments.task_assignments[kRequires5ExamplesTaskName] =
      task_assignment_2_;
  EXPECT_CALL(mock_federated_protocol_,
              MockPerformMultipleTaskAssignments(
                  ElementsAre(std::string(kSwor24HourTaskName),
                              std::string(kRequires5ExamplesTaskName)),
                  _, _))
      .WillOnce(Return(std::move(multiple_task_assignments)));
  // First task failed upload.
  // Note: the order of the tasks that being run is task 2 then task 1.
  EXPECT_CALL(
      mock_federated_protocol_,
      MockReportCompleted(_, _, Eq(task_assignment_2_.task_identifier), _))
      .WillOnce(Return(absl::InternalError("Something's wrong")));
  EXPECT_CALL(
      mock_federated_protocol_,
      MockReportCompleted(_, _, Eq(task_assignment_1_.task_identifier), _))
      .WillOnce(Return(absl::OkStatus()));

  EXPECT_CALL(mock_federated_protocol_, MockCheckin(_, _))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));

  EXPECT_CALL(mock_federated_protocol_,
              MockReportCompleted(_, _, Eq(std::nullopt), _))
      .WillOnce(Return(absl::OkStatus()));

  MockSuccessfulPlanExecution(/*has_checkpoint=*/true,
                              /*has_secagg_output=*/true, EqualsProto(plan_1_));

  MockSuccessfulPlanExecution(/*has_checkpoint=*/false,
                              /*has_secagg_output=*/true, EqualsProto(plan_2_));
  MockSuccessfulPlanExecution(
      /*has_checkpoint=*/true,
      /*has_secagg_output=*/false,
      EqualsProto(single_task_assignment_client_only_plan_));

  // We expect OnTaskCompleted called 3 times, and in sequence of failure,
  // success, success.
  {
    InSequence seq;
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultFailure()))
        .WillOnce(Return(true))
        .RetiresOnSaturation();
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true))
        .RetiresOnSaturation();
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true));
  }

  // Call RunFederatedComputation and check results.
  // Use a NiceMock because we don't care about PhaseLogger behavior in this
  // test.
  NiceMock<MockPhaseLogger> phase_logger;
  // Whenever opstats is given new network stats we store them in a variable
  // for inspection at the end of the test.
  EXPECT_CALL(phase_logger, UpdateRetryWindowAndNetworkStats(_, _))
      .WillRepeatedly(DoAll(SaveArg<0>(&latest_opstats_retry_window_),
                            SaveArg<1>(&logged_network_stats_)));
  ASSERT_OK(RunFederatedComputation(
      &mock_task_env_, phase_logger, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_));
}

TEST_F(FlRunnerEligibilityEvalTest,
       AttestationMeasurementCallbackEnabled_SingleTaskAssignment) {
  EXPECT_CALL(mock_flags_, move_device_attestation_to_start_task_assignment())
      .WillRepeatedly(Return(true));

  std::string content_binding = "test_content_binding";

  // The EligibilityEvalCheckin() will result in an eval task payload being
  // returned. This payload should be run by the runner, and the payload's
  // TaskEligibilityInfo result should be passed to the subsequent
  // Checkin(...) request.
  EXPECT_CALL(mock_federated_protocol_, MockEligibilityEvalCheckin())
      .WillOnce(Return(FederatedProtocol::EligibilityEvalTask{
          {eligibility_eval_artifacts_.plan.SerializeAsString(),
           eligibility_eval_artifacts_.checkpoint},
          kEligibilityEvalExecutionId,
          eligibility_eval_artifacts_.population_eligibility_spec,
          content_binding}));

  std::string attestation_measurement = "test_attestation_measurement";
  EXPECT_CALL(mock_task_env_, GetAttestationMeasurement(Eq(content_binding)))
      .WillOnce(Return(attestation_measurement));

  // Check that the Checkin(...) call after the EligibilityEvalCheckin() call
  // uses the expected TaskEligibilityInfo (as generated by the test
  // TffEligibilityEvalTask).
  TaskEligibilityInfo expected_eligibility_info;
  expected_eligibility_info.set_version(1);
  TaskWeight* task_weight = expected_eligibility_info.add_task_weights();
  task_weight->set_task_name(kTaskName);
  task_weight->set_weight(1);
  MockSuccessfulEligibilityPlanExecution(expected_eligibility_info);
  EXPECT_CALL(mock_federated_protocol_,
              MockCheckin(Optional(EqualsProto(expected_eligibility_info)),
                          Eq(attestation_measurement)))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));
  MockSuccessfulPlanExecution(/*has_checkpoint=*/true,
                              /*has_secagg_output=*/false);
  // We expect the regular plan to execute successfully, resulting in a
  // ReportCompleted call.
  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce(Return(absl::OkStatus()));

  {
    InSequence seq;
    // The eligibility plan execution will log a set of training-related log
    // events, followed by a full set of checkin-training-upload related log
    // events for the regular plan.
    ExpectEligibilityEvalLogEvents();
    ExpectCheckinTrainingReportLogEvents();
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true));
  }

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::SUCCESS);
  expected_result.mutable_contributed_task_names()->Add(kTaskName);
  EXPECT_THAT(*result, EqualsProto(expected_result));
}

TEST_F(FlRunnerEligibilityEvalTest,
       AttestationMeasurementCallbackDisabled_SingleTaskAssignment) {
  EXPECT_CALL(mock_flags_, move_device_attestation_to_start_task_assignment())
      .WillRepeatedly(Return(false));

  // The EligibilityEvalCheckin() will result in an eval task payload being
  // returned. This payload should be run by the runner, and the payload's
  // TaskEligibilityInfo result should be passed to the subsequent
  // Checkin(...) request.
  EXPECT_CALL(mock_federated_protocol_, MockEligibilityEvalCheckin())
      .WillOnce(Return(FederatedProtocol::EligibilityEvalTask{
          {eligibility_eval_artifacts_.plan.SerializeAsString(),
           eligibility_eval_artifacts_.checkpoint},
          kEligibilityEvalExecutionId,
          eligibility_eval_artifacts_.population_eligibility_spec,
          "unused_content_binding"}));

  EXPECT_CALL(mock_task_env_, GetAttestationMeasurement(_)).Times(0);
  // Check that the Checkin(...) call after the EligibilityEvalCheckin() call
  // uses the expected TaskEligibilityInfo (as generated by the test
  // TffEligibilityEvalTask).
  TaskEligibilityInfo expected_eligibility_info;
  expected_eligibility_info.set_version(1);
  TaskWeight* task_weight = expected_eligibility_info.add_task_weights();
  task_weight->set_task_name(kTaskName);
  task_weight->set_weight(1);
  MockSuccessfulEligibilityPlanExecution(expected_eligibility_info);
  EXPECT_CALL(mock_federated_protocol_,
              MockCheckin(Optional(EqualsProto(expected_eligibility_info)),
                          Eq(std::nullopt)))
      .WillOnce(Return(FederatedProtocol::TaskAssignment{
          {single_task_assignment_artifacts_.plan.SerializeAsString(),
           single_task_assignment_artifacts_.checkpoint},
          /*federated_select_uri_template=*/kFederatedSelectUriTemplate,
          /*aggregation_session_id=*/kAggregationSessionId,
          std::nullopt,
          std::nullopt,
          kTaskName}));
  MockSuccessfulPlanExecution(/*has_checkpoint=*/true,
                              /*has_secagg_output=*/false);
  // We expect the regular plan to execute successfully, resulting in a
  // ReportCompleted call.
  EXPECT_CALL(mock_federated_protocol_, MockReportCompleted(_, _, _, _))
      .WillOnce(Return(absl::OkStatus()));

  {
    InSequence seq;
    // The eligibility plan execution will log a set of training-related log
    // events, followed by a full set of checkin-training-upload related log
    // events for the regular plan.
    ExpectEligibilityEvalLogEvents();
    ExpectCheckinTrainingReportLogEvents();
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true));
  }

  absl::StatusOr<FLRunnerResult> result = RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_);
  ASSERT_OK(result);
  FLRunnerResult expected_result;
  *expected_result.mutable_retry_info() = CreateRetryInfoFromRetryWindow(
      mock_federated_protocol_.GetLatestRetryWindow());
  expected_result.set_contribution_result(FLRunnerResult::SUCCESS);
  expected_result.mutable_contributed_task_names()->Add(kTaskName);
  EXPECT_THAT(*result, EqualsProto(expected_result));
}

TEST_F(FlRunnerMultipleTaskAssignmentsTest,
       AttestationMeasurementCallbackEnabled_MultipleTaskAssignment) {
  EXPECT_CALL(mock_flags_, move_device_attestation_to_start_task_assignment())
      .WillRepeatedly(Return(true));

  SetUpEligibilityEvalTask();

  std::string content_binding = "test_content_binding";

  // Mock a successful eligibility eval checkin.
  // Population only supports multiple task assignments.
  PopulationEligibilitySpec population_spec;
  auto task_info_1 = population_spec.add_task_info();
  task_info_1->set_task_name(std::string(kSwor24HourTaskName));
  task_info_1->set_task_assignment_mode(
      PopulationEligibilitySpec::TaskInfo::TASK_ASSIGNMENT_MODE_MULTIPLE);
  auto task_info_2 = population_spec.add_task_info();
  task_info_2->set_task_name(std::string(kRequires5ExamplesTaskName));
  task_info_2->set_task_assignment_mode(
      PopulationEligibilitySpec::TaskInfo::TASK_ASSIGNMENT_MODE_MULTIPLE);
  EXPECT_CALL(mock_federated_protocol_, MockEligibilityEvalCheckin())
      .WillOnce(Return(FederatedProtocol::EligibilityEvalTask{
          {eligibility_eval_artifacts_.plan.SerializeAsString(),
           eligibility_eval_artifacts_.checkpoint},
          kEligibilityEvalExecutionId,
          population_spec,
          content_binding}));

  TaskEligibilityInfo task_eligibility_info;
  task_eligibility_info.set_version(1);
  TaskWeight* task_weight_1 = task_eligibility_info.add_task_weights();
  task_weight_1->set_task_name(std::string(kSwor24HourTaskName));
  task_weight_1->set_weight(1);
  TaskWeight* task_weight_2 = task_eligibility_info.add_task_weights();
  task_weight_2->set_task_name(std::string(kRequires5ExamplesTaskName));
  task_weight_2->set_weight(1);

  MockSuccessfulEligibilityPlanExecution(task_eligibility_info);

  std::string attestation_measurement = "test_attestation_measurement";
  EXPECT_CALL(mock_task_env_, GetAttestationMeasurement(Eq(content_binding)))
      .WillOnce(Return(attestation_measurement));

  // Mock a successful multiple task assignment request.
  FederatedProtocol::MultipleTaskAssignments multiple_task_assignments;
  multiple_task_assignments.task_assignments[kSwor24HourTaskName] =
      task_assignment_1_;
  multiple_task_assignments.task_assignments[kRequires5ExamplesTaskName] =
      task_assignment_2_;
  // Let the second task share the same aggregation session id as the first.
  task_assignment_2_.aggregation_session_id =
      kMultipleTaskAggregationSessionId1;
  EXPECT_CALL(mock_federated_protocol_,
              MockPerformMultipleTaskAssignments(
                  ElementsAre(std::string(kSwor24HourTaskName),
                              std::string(kRequires5ExamplesTaskName)),
                  _, Eq(attestation_measurement)))
      .WillOnce(Return(std::move(multiple_task_assignments)));

  MockSuccessfulPlanExecution(/*has_checkpoint=*/true,
                              /*has_secagg_output=*/true, EqualsProto(plan_1_));

  MockSuccessfulPlanExecution(/*has_checkpoint=*/false,
                              /*has_secagg_output=*/true, EqualsProto(plan_2_));

  ComputationResults multiple_task_assignment_computation_results_1;
  ComputationResults multiple_task_assignment_computation_results_2;
  EXPECT_CALL(mock_federated_protocol_,
              MockReportCompleted(_, _, Eq(kTaskIdentifier1), _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> task_identifier,
                    std::optional<PayloadMetadata> payload_metadata) {
        multiple_task_assignment_computation_results_1 =
            std::move(reported_results);
        return absl::OkStatus();
      });
  EXPECT_CALL(mock_federated_protocol_,
              MockReportCompleted(_, _, Eq(kTaskIdentifier2), _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> task_identifier,
                    std::optional<PayloadMetadata> payload_metadata) {
        multiple_task_assignment_computation_results_2 =
            std::move(reported_results);
        return absl::OkStatus();
      });

  // Set up PhaseLogger expectations
  {
    InSequence seq;
    ExpectEventsForEligibilityEvalComputationWithThreeEligibleTasks();
    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
    EXPECT_CALL(mock_phase_logger_, LogMultipleTaskAssignmentsStarted());
    EXPECT_CALL(mock_phase_logger_,
                LogMultipleTaskAssignmentsPlanUriReceived(
                    MockFederatedProtocol::
                        kMultipleTaskAssignmentsPlanUriReceivedNetworkStats,
                    _));
    EXPECT_CALL(mock_phase_logger_,
                LogMultipleTaskAssignmentsCompleted(
                    MockFederatedProtocol::
                        kMultipleTaskAssignmentsArtifactRetrievalNetworkStats,
                    _, _, _));

    EXPECT_CALL(mock_phase_logger_,
                SetModelIdentifier(kRequires5ExamplesTaskName));
    EXPECT_CALL(mock_phase_logger_,
                LogComputationStarted(kRequires5ExamplesTaskName));
    EXPECT_CALL(
        mock_phase_logger_,
        LogComputationCompleted(FieldsAre(Gt(0), Gt(0)),
                                EqualsNetworkStats(NetworkStats()), _, _, _));
    EXPECT_CALL(mock_phase_logger_, LogResultUploadStarted())
        .WillOnce(Return(absl::OkStatus()));

    EXPECT_CALL(mock_phase_logger_,
                LogResultUploadCompleted(
                    EqualsNetworkStats(
                        MockFederatedProtocol::kReportCompletedNetworkStats),
                    _, _));

    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true))
        .RetiresOnSaturation();

    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(kSwor24HourTaskName));
    EXPECT_CALL(mock_phase_logger_, LogComputationStarted(kSwor24HourTaskName));
    EXPECT_CALL(
        mock_phase_logger_,
        LogComputationCompleted(FieldsAre(Gt(0), Gt(0)),
                                EqualsNetworkStats(NetworkStats()), _, _, _));
    EXPECT_CALL(mock_phase_logger_, LogResultUploadStarted())
        .WillOnce(Return(absl::OkStatus()));

    EXPECT_CALL(mock_phase_logger_,
                LogResultUploadCompleted(
                    EqualsNetworkStats(
                        MockFederatedProtocol::kReportCompletedNetworkStats),
                    _, _));
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true))
        .RetiresOnSaturation();
  }

  // Call RunFederatedComputation, we don't check results in this test because
  // we have already set expectations above.
  ASSERT_OK(RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_));
}

TEST_F(FlRunnerMultipleTaskAssignmentsTest,
       AttestationMeasurementCallbackDisabled_MultipleTaskAssignment) {
  EXPECT_CALL(mock_flags_, move_device_attestation_to_start_task_assignment())
      .WillRepeatedly(Return(false));

  SetUpEligibilityEvalTask();

  std::string content_binding = "test_content_binding";

  // Mock a successful eligibility eval checkin.
  // Population only supports multiple task assignments.
  PopulationEligibilitySpec population_spec;
  auto task_info_1 = population_spec.add_task_info();
  task_info_1->set_task_name(std::string(kSwor24HourTaskName));
  task_info_1->set_task_assignment_mode(
      PopulationEligibilitySpec::TaskInfo::TASK_ASSIGNMENT_MODE_MULTIPLE);
  auto task_info_2 = population_spec.add_task_info();
  task_info_2->set_task_name(std::string(kRequires5ExamplesTaskName));
  task_info_2->set_task_assignment_mode(
      PopulationEligibilitySpec::TaskInfo::TASK_ASSIGNMENT_MODE_MULTIPLE);
  EXPECT_CALL(mock_federated_protocol_, MockEligibilityEvalCheckin())
      .WillOnce(Return(FederatedProtocol::EligibilityEvalTask{
          {eligibility_eval_artifacts_.plan.SerializeAsString(),
           eligibility_eval_artifacts_.checkpoint},
          kEligibilityEvalExecutionId,
          population_spec,
          content_binding}));

  TaskEligibilityInfo task_eligibility_info;
  task_eligibility_info.set_version(1);
  TaskWeight* task_weight_1 = task_eligibility_info.add_task_weights();
  task_weight_1->set_task_name(std::string(kSwor24HourTaskName));
  task_weight_1->set_weight(1);
  TaskWeight* task_weight_2 = task_eligibility_info.add_task_weights();
  task_weight_2->set_task_name(std::string(kRequires5ExamplesTaskName));
  task_weight_2->set_weight(1);

  MockSuccessfulEligibilityPlanExecution(task_eligibility_info);

  EXPECT_CALL(mock_task_env_, GetAttestationMeasurement(_)).Times(0);

  // Mock a successful multiple task assignment request.
  FederatedProtocol::MultipleTaskAssignments multiple_task_assignments;
  multiple_task_assignments.task_assignments[kSwor24HourTaskName] =
      task_assignment_1_;
  multiple_task_assignments.task_assignments[kRequires5ExamplesTaskName] =
      task_assignment_2_;
  // Let the second task share the same aggregation session id as the first.
  task_assignment_2_.aggregation_session_id =
      kMultipleTaskAggregationSessionId1;
  EXPECT_CALL(mock_federated_protocol_,
              MockPerformMultipleTaskAssignments(
                  ElementsAre(std::string(kSwor24HourTaskName),
                              std::string(kRequires5ExamplesTaskName)),
                  _, Eq(std::nullopt)))
      .WillOnce(Return(std::move(multiple_task_assignments)));

  MockSuccessfulPlanExecution(/*has_checkpoint=*/true,
                              /*has_secagg_output=*/true, EqualsProto(plan_1_));

  MockSuccessfulPlanExecution(/*has_checkpoint=*/false,
                              /*has_secagg_output=*/true, EqualsProto(plan_2_));

  ComputationResults multiple_task_assignment_computation_results_1;
  ComputationResults multiple_task_assignment_computation_results_2;
  EXPECT_CALL(mock_federated_protocol_,
              MockReportCompleted(_, _, Eq(kTaskIdentifier1), _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> task_identifier,
                    std::optional<PayloadMetadata> payload_metadata) {
        multiple_task_assignment_computation_results_1 =
            std::move(reported_results);
        return absl::OkStatus();
      });
  EXPECT_CALL(mock_federated_protocol_,
              MockReportCompleted(_, _, Eq(kTaskIdentifier2), _))
      .WillOnce([&](ComputationResults reported_results,
                    absl::Duration plan_duration,
                    std::optional<std::string> task_identifier,
                    std::optional<PayloadMetadata> payload_metadata) {
        multiple_task_assignment_computation_results_2 =
            std::move(reported_results);
        return absl::OkStatus();
      });

  // Set up PhaseLogger expectations
  {
    InSequence seq;
    ExpectEventsForEligibilityEvalComputationWithThreeEligibleTasks();
    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(""));
    EXPECT_CALL(mock_phase_logger_, LogMultipleTaskAssignmentsStarted());
    EXPECT_CALL(mock_phase_logger_,
                LogMultipleTaskAssignmentsPlanUriReceived(
                    MockFederatedProtocol::
                        kMultipleTaskAssignmentsPlanUriReceivedNetworkStats,
                    _));
    EXPECT_CALL(mock_phase_logger_,
                LogMultipleTaskAssignmentsCompleted(
                    MockFederatedProtocol::
                        kMultipleTaskAssignmentsArtifactRetrievalNetworkStats,
                    _, _, _));

    EXPECT_CALL(mock_phase_logger_,
                SetModelIdentifier(kRequires5ExamplesTaskName));
    EXPECT_CALL(mock_phase_logger_,
                LogComputationStarted(kRequires5ExamplesTaskName));
    EXPECT_CALL(
        mock_phase_logger_,
        LogComputationCompleted(FieldsAre(Gt(0), Gt(0)),
                                EqualsNetworkStats(NetworkStats()), _, _, _));
    EXPECT_CALL(mock_phase_logger_, LogResultUploadStarted())
        .WillOnce(Return(absl::OkStatus()));

    EXPECT_CALL(mock_phase_logger_,
                LogResultUploadCompleted(
                    EqualsNetworkStats(
                        MockFederatedProtocol::kReportCompletedNetworkStats),
                    _, _));

    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true))
        .RetiresOnSaturation();

    EXPECT_CALL(mock_phase_logger_, SetModelIdentifier(kSwor24HourTaskName));
    EXPECT_CALL(mock_phase_logger_, LogComputationStarted(kSwor24HourTaskName));
    EXPECT_CALL(
        mock_phase_logger_,
        LogComputationCompleted(FieldsAre(Gt(0), Gt(0)),
                                EqualsNetworkStats(NetworkStats()), _, _, _));
    EXPECT_CALL(mock_phase_logger_, LogResultUploadStarted())
        .WillOnce(Return(absl::OkStatus()));

    EXPECT_CALL(mock_phase_logger_,
                LogResultUploadCompleted(
                    EqualsNetworkStats(
                        MockFederatedProtocol::kReportCompletedNetworkStats),
                    _, _));
    EXPECT_CALL(mock_task_env_, OnTaskCompleted(IsTaskResultSuccess()))
        .WillOnce(Return(true))
        .RetiresOnSaturation();
  }

  // Call RunFederatedComputation, we don't check results in this test because
  // we have already set expectations above.
  ASSERT_OK(RunFederatedComputation(
      &mock_task_env_, mock_phase_logger_, &mock_event_publisher_, &files_impl_,
      &mock_log_manager_, &mock_opstats_logger_, &mock_flags_,
      &mock_federated_protocol_, &mock_fedselect_manager_, timing_config_,
      /*reference_time=*/absl::Now(), kSessionName, kPopulationName, clock_));
}
}  // namespace
}  // namespace client
}  // namespace fcp
