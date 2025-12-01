/*
 * Copyright 2021 Google LLC
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
#include "fcp/client/opstats/opstats_example_store.h"

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/util/time_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/simple_task_environment.h"
#include "fcp/client/test_helpers.h"
#include "fcp/protos/federated_api.pb.h"
#include "fcp/protos/opstats.pb.h"
#include "fcp/protos/plan.pb.h"
#include "fcp/testing/testing.h"
#include "tensorflow/core/example/example.pb.h"
#include "tensorflow/core/example/feature.pb.h"

namespace fcp {
namespace client {
namespace opstats {
namespace {

using ::google::internal::federated::plan::ExampleSelector;
using ::google::internal::federatedml::v2::RetryWindow;
using ::google::protobuf::util::TimeUtil;
using ::testing::ElementsAreArray;
using ::testing::Return;

constexpr char kTestTaskName[] = "stefans_really_cool_task";

class OpStatsExampleStoreTest : public testing::Test {
 public:
  OpStatsExampleStoreTest() {
    EXPECT_CALL(mock_opstats_logger_, GetOpStatsDb())
        .WillRepeatedly(Return(&mock_db_));
    EXPECT_CALL(mock_opstats_logger_, GetCurrentTaskName())
        .WillRepeatedly(Return(kTestTaskName));
  }

 protected:
  static OperationalStats::Event CreateEvent(
      OperationalStats::Event::EventKind event_kind, int64_t event_time_ms) {
    OperationalStats::Event event;
    event.set_event_type(event_kind);
    *event.mutable_timestamp() =
        TimeUtil::MillisecondsToTimestamp(event_time_ms);
    return event;
  }

  static OperationalStats::DatasetStats CreateDatasetStats(
      int64_t num_examples_read, int64_t num_bytes_read,
      int64_t first_access_time_ms) {
    OperationalStats::DatasetStats stats;
    stats.set_num_bytes_read(num_bytes_read);
    stats.set_num_examples_read(num_examples_read);
    *stats.mutable_first_access_timestamp() =
        TimeUtil::MillisecondsToTimestamp(first_access_time_ms);
    return stats;
  }

  static void VerifyExample(
      const tensorflow::Example& example, absl::string_view session,
      absl::string_view population, absl::string_view task_name,
      absl::string_view error, int64_t bytes_downloaded, int64_t bytes_uploaded,
      int64_t network_duration_ms, const std::vector<int64_t>& event_types,
      const std::vector<int64_t>& event_times_ms,
      const absl::flat_hash_map<std::string, OperationalStats::DatasetStats>&
          dataset_stats_map,
      int64_t min_delay_ms, int64_t max_delay_ms,
      int64_t earliest_trust_worthy_time_ms) {
    ASSERT_EQ(ExtractSingleString(example, kSessionName), session);
    ASSERT_EQ(ExtractSingleString(example, kPopulationName), population);
    ASSERT_EQ(ExtractSingleString(example, kTaskName), task_name);
    ASSERT_EQ(ExtractSingleString(example, kErrorMessage), error);
    ASSERT_EQ(ExtractSingleInt64(example, kChunkingLayerBytesDownloaded),
              bytes_downloaded);
    ASSERT_EQ(ExtractSingleInt64(example, kChunkingLayerBytesUploaded),
              bytes_uploaded);
    ASSERT_EQ(ExtractSingleInt64(example, kNetworkDuration),
              network_duration_ms);
    ASSERT_EQ(ExtractSingleInt64(example, kEarliestTrustWorthyTimeMillis),
              earliest_trust_worthy_time_ms);

    // Events
    EXPECT_THAT(ExtractRepeatedInt64(example, kEventsEventType),
                ElementsAreArray(event_types));
    EXPECT_THAT(ExtractRepeatedInt64(example, kEventsTimestampMillis),
                ElementsAreArray(event_times_ms));

    // Dataset stats
    auto dataset_urls = ExtractRepeatedString(example, kDatasetStatsUri);
    auto example_counts =
        ExtractRepeatedInt64(example, kDatasetStatsNumExamplesRead);
    auto example_bytes =
        ExtractRepeatedInt64(example, kDatasetStatsNumBytesRead);
    ASSERT_EQ(dataset_urls.size(), dataset_stats_map.size());
    for (int i = 0; i < dataset_urls.size(); i++) {
      const std::string& key = dataset_urls.at(i);
      ASSERT_TRUE(dataset_stats_map.contains(key));
      ASSERT_EQ(dataset_stats_map.at(key).num_examples_read(),
                example_counts.at(i));
      ASSERT_EQ(dataset_stats_map.at(key).num_bytes_read(),
                example_bytes.at(i));
    }

    // RetryWindow
    ASSERT_EQ(ExtractSingleInt64(example, kRetryWindowDelayMinMillis),
              min_delay_ms);
    ASSERT_EQ(ExtractSingleInt64(example, kRetryWindowDelayMaxMillis),
              max_delay_ms);
  }

  testing::StrictMock<MockOpStatsLogger> mock_opstats_logger_;
  testing::StrictMock<MockOpStatsDb> mock_db_;
  testing::StrictMock<MockLogManager> mock_log_manager_;
  OpStatsExampleIteratorFactory iterator_factory_ =
      OpStatsExampleIteratorFactory(&mock_opstats_logger_, &mock_log_manager_);
};

TEST_F(OpStatsExampleStoreTest, TestInvalidCollectionUrl) {
  ExampleSelector selector;
  selector.set_collection_uri("INVALID");
  EXPECT_CALL(mock_log_manager_,
              LogDiag(ProdDiagCode::OPSTATS_INCORRECT_COLLECTION_URI));

  EXPECT_FALSE(iterator_factory_.CanHandle(selector));

  absl::StatusOr<std::unique_ptr<ExampleIterator>> status_or =
      iterator_factory_.CreateExampleIterator(selector);
  EXPECT_THAT(status_or.status(), IsCode(absl::StatusCode::kInvalidArgument));
}

TEST_F(OpStatsExampleStoreTest, TestMalformedCriteria) {
  ExampleSelector selector;
  selector.set_collection_uri(kOpStatsCollectionUri);
  selector.mutable_criteria()->set_value("NOT_A_PROTO");
  EXPECT_CALL(mock_log_manager_,
              LogDiag(ProdDiagCode::OPSTATS_INVALID_SELECTION_CRITERIA));
  absl::StatusOr<std::unique_ptr<ExampleIterator>> status_or =
      iterator_factory_.CreateExampleIterator(selector);
  EXPECT_THAT(status_or.status(), IsCode(absl::StatusCode::kInvalidArgument));
}

TEST_F(OpStatsExampleStoreTest, TestInvalidCriteria) {
  ExampleSelector selector;
  selector.set_collection_uri(kOpStatsCollectionUri);
  OpStatsSelectionCriteria criteria;
  *criteria.mutable_start_time() = TimeUtil::MillisecondsToTimestamp(2000L);
  *criteria.mutable_end_time() = TimeUtil::MillisecondsToTimestamp(1000L);
  selector.mutable_criteria()->PackFrom(criteria);
  EXPECT_CALL(mock_log_manager_,
              LogDiag(ProdDiagCode::OPSTATS_INVALID_SELECTION_CRITERIA));
  absl::StatusOr<std::unique_ptr<ExampleIterator>> status_or =
      iterator_factory_.CreateExampleIterator(selector);
  EXPECT_THAT(status_or.status(), IsCode(absl::StatusCode::kInvalidArgument));
}

TEST_F(OpStatsExampleStoreTest, TestReadFromDbFailed) {
  ExampleSelector selector;
  selector.set_collection_uri(kOpStatsCollectionUri);
  EXPECT_CALL(mock_db_, Read())
      .WillOnce(Return(absl::InternalError("Something's wrong.")));
  absl::StatusOr<std::unique_ptr<ExampleIterator>> status_or =
      iterator_factory_.CreateExampleIterator(selector);
  EXPECT_THAT(status_or.status(), IsCode(absl::StatusCode::kInternal));
}

TEST_F(OpStatsExampleStoreTest, Success) {
  // Prepare some data
  OpStatsSequence opstats_sequence;

  OperationalStats* stats_first = opstats_sequence.add_opstats();
  std::string session_first = "session_first";
  std::string population_first = "population_first";
  stats_first->set_session_name(session_first);
  stats_first->set_population_name(population_first);

  OperationalStats* stats_last = opstats_sequence.add_opstats();
  std::string session_last = "session_last";
  std::string population_last = "population_last";
  stats_last->set_session_name(session_last);
  stats_last->set_population_name(population_last);

  EXPECT_CALL(mock_db_, Read()).WillOnce(Return(opstats_sequence));

  ExampleSelector selector;
  selector.set_collection_uri(kOpStatsCollectionUri);
  absl::StatusOr<std::unique_ptr<ExampleIterator>> iterator_or =
      iterator_factory_.CreateExampleIterator(selector);
  ASSERT_TRUE(iterator_or.ok());
  std::unique_ptr<ExampleIterator> iterator = std::move(iterator_or.value());
  absl::StatusOr<std::string> example_or = iterator->Next();
  ASSERT_TRUE(example_or.ok());
  tensorflow::Example example_last;
  ASSERT_TRUE(example_last.ParseFromString(example_or.value()));
  example_or = iterator->Next();
  ASSERT_TRUE(example_or.ok());
  tensorflow::Example example_first;
  ASSERT_TRUE(example_first.ParseFromString(example_or.value()));

  // Check if the examples contain the expected data. Opstats examples are
  // returned in last in, first out order.
  std::set<std::string> actual_session_names;
  actual_session_names.insert(ExtractSingleString(example_last, kSessionName));
  actual_session_names.insert(ExtractSingleString(example_first, kSessionName));
  std::set<std::string> expected_session_names = {session_last, session_first};
  EXPECT_EQ(actual_session_names, expected_session_names);

  std::set<std::string> actual_population_names;
  actual_population_names.insert(
      ExtractSingleString(example_last, kPopulationName));
  actual_population_names.insert(
      ExtractSingleString(example_first, kPopulationName));
  std::set<std::string> expected_population_names = {population_last,
                                                     population_first};
  EXPECT_EQ(actual_population_names, expected_population_names);

  // We should have arrived at the end of the iterator.
  example_or = iterator->Next();
  EXPECT_THAT(example_or.status(), IsCode(absl::StatusCode::kOutOfRange));

  // Subsequent Next() calls should all return OUT_OF_RANGE.
  example_or = iterator->Next();
  EXPECT_THAT(example_or.status(), IsCode(absl::StatusCode::kOutOfRange));

  // Close() should work without exceptions.
  iterator->Close();
}

TEST_F(OpStatsExampleStoreTest, EmptyDataSupportsNeetContext) {
  EXPECT_CALL(mock_db_, Read())
      .WillOnce(Return(OpStatsSequence::default_instance()));

  ExampleSelector selector;
  selector.set_collection_uri(kOpStatsCollectionUri);
  absl::StatusOr<std::unique_ptr<ExampleIterator>> iterator_or =
      iterator_factory_.CreateExampleIterator(selector);
  ASSERT_TRUE(iterator_or.ok());
  std::unique_ptr<ExampleIterator> iterator = std::move(iterator_or.value());
  absl::StatusOr<std::string> example_or = iterator->Next();
  ASSERT_OK(example_or);
  tensorflow::Example example;
  example.ParseFromString(*example_or);
  auto supports_neet_context =
      ExtractSingleInt64(example, kSupportsNeetContext);
  ASSERT_EQ(supports_neet_context, 1);
  // We should have arrived at the end of the iterator.
  EXPECT_THAT(iterator->Next().status(), IsCode(absl::StatusCode::kOutOfRange));
}

TEST_F(OpStatsExampleStoreTest, DataIsFilteredBySelectionCriteria) {
  OperationalStats included;
  included.mutable_events()->Add(CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED, 900L));
  included.mutable_events()->Add(CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED, 1000L));

  OperationalStats excluded_early;
  excluded_early.mutable_events()->Add(CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED, 500L));
  excluded_early.mutable_events()->Add(CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED, 700L));

  OperationalStats excluded_late;
  excluded_late.mutable_events()->Add(CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED, 800L));
  excluded_late.mutable_events()->Add(CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED, 2001L));

  OpStatsSequence opstats_sequence;
  *opstats_sequence.add_opstats() = std::move(excluded_early);
  *opstats_sequence.add_opstats() = std::move(included);
  *opstats_sequence.add_opstats() = std::move(excluded_late);
  EXPECT_CALL(mock_db_, Read()).WillOnce(Return(opstats_sequence));

  ExampleSelector selector;
  selector.set_collection_uri(kOpStatsCollectionUri);
  OpStatsSelectionCriteria criteria;
  *criteria.mutable_start_time() = TimeUtil::MillisecondsToTimestamp(1000L);
  *criteria.mutable_end_time() = TimeUtil::MillisecondsToTimestamp(2000L);
  selector.mutable_criteria()->PackFrom(criteria);
  absl::StatusOr<std::unique_ptr<ExampleIterator>> iterator_or =
      iterator_factory_.CreateExampleIterator(selector);

  ASSERT_TRUE(iterator_or.ok());
  std::unique_ptr<ExampleIterator> iterator = std::move(iterator_or.value());
  absl::StatusOr<std::string> example_or = iterator->Next();
  ASSERT_TRUE(example_or.ok());
  tensorflow::Example example;
  example.ParseFromString(example_or.value());
  auto event_type_list = ExtractRepeatedInt64(example, kEventsEventType);
  ASSERT_EQ(event_type_list.at(0),
            OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED);
  ASSERT_EQ(event_type_list.at(1),
            OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED);
  auto event_time_ms_list =
      ExtractRepeatedInt64(example, kEventsTimestampMillis);
  ASSERT_EQ(event_time_ms_list.at(0), 900);
  ASSERT_EQ(event_time_ms_list.at(1), 1000);

  // We expect the iterator reaches the end because there's only 1 example.
  example_or = iterator->Next();
  EXPECT_THAT(example_or.status(), IsCode(absl::StatusCode::kOutOfRange));
}

TEST_F(OpStatsExampleStoreTest, SelectionCriteriaOnlyContainsBeginTime) {
  OperationalStats included;
  included.mutable_events()->Add(CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED, 900L));
  included.mutable_events()->Add(CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED, 1000L));

  OperationalStats excluded_early;
  excluded_early.mutable_events()->Add(CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED, 500L));
  excluded_early.mutable_events()->Add(CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED, 700L));

  OpStatsSequence opstats_sequence;
  *opstats_sequence.add_opstats() = std::move(excluded_early);
  *opstats_sequence.add_opstats() = std::move(included);
  EXPECT_CALL(mock_db_, Read()).WillOnce(Return(opstats_sequence));

  ExampleSelector selector;
  selector.set_collection_uri(kOpStatsCollectionUri);
  OpStatsSelectionCriteria criteria;
  *criteria.mutable_start_time() = TimeUtil::MillisecondsToTimestamp(1000L);
  selector.mutable_criteria()->PackFrom(criteria);
  absl::StatusOr<std::unique_ptr<ExampleIterator>> iterator_or =
      iterator_factory_.CreateExampleIterator(selector);

  ASSERT_TRUE(iterator_or.ok());
  std::unique_ptr<ExampleIterator> iterator = std::move(iterator_or.value());
  absl::StatusOr<std::string> example_or = iterator->Next();
  ASSERT_TRUE(example_or.ok());
  tensorflow::Example example;
  example.ParseFromString(example_or.value());
  auto event_type_list = ExtractRepeatedInt64(example, kEventsEventType);
  ASSERT_EQ(event_type_list.at(0),
            OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED);
  ASSERT_EQ(event_type_list.at(1),
            OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED);
  auto event_time_ms_list =
      ExtractRepeatedInt64(example, kEventsTimestampMillis);
  ASSERT_EQ(event_time_ms_list.at(0), 900);
  ASSERT_EQ(event_time_ms_list.at(1), 1000);

  // We expect the iterator reaches the end because there's only 1 example.
  example_or = iterator->Next();
  EXPECT_THAT(example_or.status(), IsCode(absl::StatusCode::kOutOfRange));
}

TEST_F(OpStatsExampleStoreTest, SelectionCriteriaOnlyContainsEndTime) {
  OperationalStats included;
  included.mutable_events()->Add(CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED, 900L));
  included.mutable_events()->Add(CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED, 1000L));

  OperationalStats excluded_late;
  excluded_late.mutable_events()->Add(CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED, 800L));
  excluded_late.mutable_events()->Add(CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED, 2001L));

  OpStatsSequence opstats_sequence;
  *opstats_sequence.add_opstats() = std::move(included);
  *opstats_sequence.add_opstats() = std::move(excluded_late);
  EXPECT_CALL(mock_db_, Read()).WillOnce(Return(opstats_sequence));

  ExampleSelector selector;
  selector.set_collection_uri(kOpStatsCollectionUri);
  OpStatsSelectionCriteria criteria;
  *criteria.mutable_start_time() = TimeUtil::MillisecondsToTimestamp(1000L);
  *criteria.mutable_end_time() = TimeUtil::MillisecondsToTimestamp(2000L);
  selector.mutable_criteria()->PackFrom(criteria);
  absl::StatusOr<std::unique_ptr<ExampleIterator>> iterator_or =
      iterator_factory_.CreateExampleIterator(selector);

  ASSERT_TRUE(iterator_or.ok());
  std::unique_ptr<ExampleIterator> iterator = std::move(iterator_or.value());
  absl::StatusOr<std::string> example_or = iterator->Next();
  ASSERT_TRUE(example_or.ok());
  tensorflow::Example example;
  example.ParseFromString(example_or.value());
  auto event_type_list = ExtractRepeatedInt64(example, kEventsEventType);
  ASSERT_EQ(event_type_list.at(0),
            OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED);
  ASSERT_EQ(event_type_list.at(1),
            OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED);
  auto event_time_ms_list =
      ExtractRepeatedInt64(example, kEventsTimestampMillis);
  ASSERT_EQ(event_time_ms_list.at(0), 900);
  ASSERT_EQ(event_time_ms_list.at(1), 1000);

  // We expect the iterator reaches the end because there's only 1 example.
  example_or = iterator->Next();
  EXPECT_THAT(example_or.status(), IsCode(absl::StatusCode::kOutOfRange));
}

TEST_F(OpStatsExampleStoreTest,
       SelectionCriteriaLastSuccessfulContributionEnabledAndExists) {
  OpStatsExampleIteratorFactory iterator_factory =
      OpStatsExampleIteratorFactory(&mock_opstats_logger_, &mock_log_manager_);
  OperationalStats included;
  included.set_task_name(kTestTaskName);
  included.mutable_events()->Add(CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED, 900L));
  included.mutable_events()->Add(CreateEvent(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_STARTED, 1000L));

  OperationalStats last_successful_contribution;
  last_successful_contribution.set_task_name(kTestTaskName);
  last_successful_contribution.mutable_events()->Add(CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED, 1200L));
  last_successful_contribution.mutable_events()->Add(CreateEvent(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_STARTED, 2001L));

  OpStatsSequence opstats_sequence;
  *opstats_sequence.add_opstats() = std::move(included);
  *opstats_sequence.add_opstats() = std::move(last_successful_contribution);
  EXPECT_CALL(mock_db_, Read()).WillOnce(Return(opstats_sequence));

  ExampleSelector selector;
  selector.set_collection_uri(kOpStatsCollectionUri);
  OpStatsSelectionCriteria criteria;
  criteria.set_last_successful_contribution(true);
  selector.mutable_criteria()->PackFrom(criteria);
  absl::StatusOr<std::unique_ptr<ExampleIterator>> iterator_or =
      iterator_factory.CreateExampleIterator(selector);

  EXPECT_OK(iterator_or);
  std::unique_ptr<ExampleIterator> iterator = std::move(iterator_or.value());
  absl::StatusOr<std::string> example_or = iterator->Next();
  EXPECT_OK(example_or);
  tensorflow::Example example;
  example.ParseFromString(example_or.value());
  auto event_type_list = ExtractRepeatedInt64(example, kEventsEventType);
  ASSERT_EQ(event_type_list.at(0),
            OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED);
  ASSERT_EQ(event_type_list.at(1),
            OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_STARTED);
  auto event_time_ms_list =
      ExtractRepeatedInt64(example, kEventsTimestampMillis);
  ASSERT_EQ(event_time_ms_list.at(0), 1200L);
  ASSERT_EQ(event_time_ms_list.at(1), 2001L);

  // We expect the iterator reaches the end because there's only 1 example.
  example_or = iterator->Next();
  EXPECT_THAT(example_or.status(), IsCode(absl::StatusCode::kOutOfRange));
}

TEST_F(OpStatsExampleStoreTest,
       SelectionCriteriaLastSuccessfulContributionEnabledAndDoesNotExist) {
  OpStatsExampleIteratorFactory iterator_factory =
      OpStatsExampleIteratorFactory(&mock_opstats_logger_, &mock_log_manager_);
  OperationalStats non_matching;
  non_matching.set_task_name("non_matching_task_name");
  non_matching.mutable_events()->Add(CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED, 900L));
  non_matching.mutable_events()->Add(CreateEvent(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_STARTED, 1000L));

  OperationalStats matching_but_no_upload;
  matching_but_no_upload.set_task_name(kTestTaskName);
  matching_but_no_upload.mutable_events()->Add(CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED, 1200L));

  OpStatsSequence opstats_sequence;
  *opstats_sequence.add_opstats() = std::move(non_matching);
  *opstats_sequence.add_opstats() = std::move(matching_but_no_upload);
  EXPECT_CALL(mock_db_, Read()).WillOnce(Return(opstats_sequence));

  ExampleSelector selector;
  selector.set_collection_uri(kOpStatsCollectionUri);
  OpStatsSelectionCriteria criteria;
  criteria.set_last_successful_contribution(true);
  selector.mutable_criteria()->PackFrom(criteria);
  absl::StatusOr<std::unique_ptr<ExampleIterator>> iterator_or =
      iterator_factory.CreateExampleIterator(selector);

  EXPECT_OK(iterator_or);
  std::unique_ptr<ExampleIterator> iterator = std::move(iterator_or.value());
  absl::StatusOr<std::string> example_or = iterator->Next();
  ASSERT_OK(example_or);
  tensorflow::Example example;
  example.ParseFromString(*example_or);
  auto supports_neet_context =
      ExtractSingleInt64(example, kSupportsNeetContext);
  ASSERT_EQ(supports_neet_context, 1);
  // We should have arrived at the end of the iterator.
  EXPECT_THAT(iterator->Next().status(), IsCode(absl::StatusCode::kOutOfRange));
}

TEST_F(OpStatsExampleStoreTest, FullSerialization) {
  OperationalStats stats;
  // Set singular fields
  std::string session = "session";
  std::string population = "population";
  std::string task_name = "task";
  std::string error = "error";
  int64_t chunking_layer_bytes_downloaded = 200;
  int64_t chunking_layer_bytes_uploaded = 600;
  int64_t network_duration_ms = 700;
  stats.set_session_name(session);
  stats.set_population_name(population);
  stats.set_task_name(task_name);
  stats.set_error_message(error);
  stats.set_chunking_layer_bytes_downloaded(chunking_layer_bytes_downloaded);
  stats.set_chunking_layer_bytes_uploaded(chunking_layer_bytes_uploaded);
  *stats.mutable_network_duration() =
      TimeUtil::MillisecondsToDuration(network_duration_ms);

  // Set two events
  OperationalStats::Event::EventKind event_kind_a =
      OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED;
  int64_t event_time_ms_a = 1000;
  OperationalStats::Event::EventKind event_kind_b =
      OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED;
  int64_t event_time_ms_b = 1500;
  stats.mutable_events()->Add(CreateEvent(event_kind_a, event_time_ms_a));
  stats.mutable_events()->Add(CreateEvent(event_kind_b, event_time_ms_b));

  // Set two dataset stats
  std::string uri_a = "app:/train";
  int64_t num_examples_a = 10;
  int64_t example_bytes_a = 1000;
  int64_t first_access_time_ms_a = 1000;
  std::string uri_b = "app:/test";
  int64_t num_examples_b = 5;
  int64_t example_bytes_b = 500;
  int64_t first_access_time_ms_b = 1500;

  OperationalStats::DatasetStats dataset_stats_1 = CreateDatasetStats(
      num_examples_a, example_bytes_a, first_access_time_ms_a);
  (*stats.mutable_dataset_stats())[uri_a] = dataset_stats_1;
  OperationalStats::DatasetStats dataset_stats_2 = CreateDatasetStats(
      num_examples_b, example_bytes_b, first_access_time_ms_b);
  (*stats.mutable_dataset_stats())[uri_b] = dataset_stats_2;

  // Set retry window
  int64_t min_delay_ms = 5000;
  int64_t max_delay_ms = 9000;
  RetryWindow retry;
  retry.set_retry_token("token");
  *retry.mutable_delay_min() = TimeUtil::MillisecondsToDuration(min_delay_ms);
  *retry.mutable_delay_max() = TimeUtil::MillisecondsToDuration(max_delay_ms);
  *stats.mutable_retry_window() = retry;

  OpStatsSequence opstats_sequence;
  ::google::protobuf::Timestamp currentTime = TimeUtil::GetCurrentTime();
  *opstats_sequence.mutable_earliest_trustworthy_time() = currentTime;
  *opstats_sequence.add_opstats() = std::move(stats);
  EXPECT_CALL(mock_db_, Read()).WillOnce(Return(opstats_sequence));

  ExampleSelector selector;
  selector.set_collection_uri(kOpStatsCollectionUri);
  absl::StatusOr<std::unique_ptr<ExampleIterator>> iterator_or =
      iterator_factory_.CreateExampleIterator(selector);

  ASSERT_TRUE(iterator_or.ok());
  std::unique_ptr<ExampleIterator> iterator = std::move(iterator_or.value());
  absl::StatusOr<std::string> example_or = iterator->Next();
  ASSERT_TRUE(example_or.ok());
  tensorflow::Example example;
  example.ParseFromString(example_or.value());

  // Verify the example contains all the correct information.
  absl::flat_hash_map<std::string, OperationalStats::DatasetStats>
      dataset_stats_map;
  dataset_stats_map[uri_a] = dataset_stats_1;
  dataset_stats_map[uri_b] = dataset_stats_2;

  VerifyExample(example, session, population, task_name, error,
                chunking_layer_bytes_downloaded, chunking_layer_bytes_uploaded,
                network_duration_ms,
                std::vector<int64_t>{event_kind_a, event_kind_b},
                std::vector<int64_t>{event_time_ms_a, event_time_ms_b},
                dataset_stats_map, min_delay_ms, max_delay_ms,
                TimeUtil::TimestampToMilliseconds(currentTime));
}

TEST_F(OpStatsExampleStoreTest, FullSerializationWithPhaseStats) {
  OperationalStats stats;

  // Eligibility checkin
  OperationalStats::PhaseStats eligibility_checkin;
  eligibility_checkin.set_phase(
      OperationalStats::PhaseStats::ELIGIBILITY_EVAL_CHECKIN);
  *eligibility_checkin.add_events() = CreateEvent(
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_CHECKIN_STARTED, 10000);
  *eligibility_checkin.add_events() = CreateEvent(
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_DISABLED, 10100);
  const int64_t eligibility_checkin_bytes_downloaded = 10;
  const int64_t eligibility_checkin_bytes_uploaded = 10;
  const int64_t eligibility_checkin_network_duration_ms = 100;
  eligibility_checkin.set_bytes_downloaded(
      eligibility_checkin_bytes_downloaded);
  eligibility_checkin.set_bytes_uploaded(eligibility_checkin_bytes_uploaded);
  *eligibility_checkin.mutable_network_duration() =
      TimeUtil::MillisecondsToDuration(eligibility_checkin_network_duration_ms);
  *stats.add_phase_stats() = eligibility_checkin;

  // Multiple task assignments
  OperationalStats::PhaseStats multiple_task_assignments;
  multiple_task_assignments.set_phase(
      OperationalStats::PhaseStats::MULTIPLE_TASK_ASSIGNMENTS);
  *multiple_task_assignments.add_events() = CreateEvent(
      OperationalStats::Event::EVENT_KIND_MULTIPLE_TASK_ASSIGNMENTS_STARTED,
      11000);
  *multiple_task_assignments.add_events() = CreateEvent(
      OperationalStats::Event::EVENT_KIND_MULTIPLE_TASK_ASSIGNMENTS_COMPLETED,
      12000);
  const int64_t multiple_task_assignments_bytes_downloaded = 500;
  const int64_t multiple_task_assignments_bytes_uploaded = 250;
  const int64_t multiple_task_assignments_network_duration_ms = 500;
  multiple_task_assignments.set_bytes_downloaded(
      multiple_task_assignments_bytes_downloaded);
  multiple_task_assignments.set_bytes_uploaded(
      multiple_task_assignments_bytes_uploaded);
  *multiple_task_assignments.mutable_network_duration() =
      TimeUtil::MillisecondsToDuration(
          multiple_task_assignments_network_duration_ms);
  *stats.add_phase_stats() = multiple_task_assignments;

  // Computation for task 1
  OperationalStats::PhaseStats computation_1;
  computation_1.set_phase(OperationalStats::PhaseStats::COMPUTATION);
  *computation_1.add_events() = CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED, 13000);
  *computation_1.add_events() = CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED, 14000);
  const std::string task_name_1 = "task_1";
  computation_1.set_task_name(task_name_1);
  const std::string uri_1 = "app://collection_1";
  OperationalStats::DatasetStats dataset_stats_1 =
      CreateDatasetStats(10, 1000, 100);
  (*computation_1.mutable_dataset_stats())[uri_1] = dataset_stats_1;
  *stats.add_phase_stats() = computation_1;

  // Upload for task 1
  OperationalStats::PhaseStats upload_1;
  upload_1.set_phase(OperationalStats::PhaseStats::UPLOAD);
  *upload_1.add_events() = CreateEvent(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_STARTED, 15000);
  *upload_1.add_events() = CreateEvent(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_FINISHED, 16000);
  const int64_t upload_1_bytes_downloaded = 10;
  const int64_t upload_1_bytes_uploaded = 1000;
  const int64_t upload_1_network_duration_ms = 200;
  upload_1.set_bytes_downloaded(upload_1_bytes_downloaded);
  upload_1.set_bytes_uploaded(upload_1_bytes_uploaded);
  *upload_1.mutable_network_duration() =
      TimeUtil::MillisecondsToDuration(upload_1_network_duration_ms);
  *stats.add_phase_stats() = upload_1;

  // Computation for task 2
  OperationalStats::PhaseStats computation_2;
  computation_2.set_phase(OperationalStats::PhaseStats::COMPUTATION);
  *computation_2.add_events() = CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED, 17000);
  *computation_2.add_events() = CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_ERROR_TENSORFLOW, 17050);
  const std::string task_name_2 = "task_2";
  computation_2.set_task_name(task_name_2);
  const std::string error_message = "Missing op kernel: DO_NOT_EXIST";
  computation_2.set_error_message(error_message);
  *stats.add_phase_stats() = computation_2;

  // Regular Check in
  OperationalStats::PhaseStats checkin;
  checkin.set_phase(OperationalStats::PhaseStats::CHECKIN);
  *checkin.add_events() =
      CreateEvent(OperationalStats::Event::EVENT_KIND_CHECKIN_STARTED, 18000);
  *checkin.add_events() =
      CreateEvent(OperationalStats::Event::EVENT_KIND_CHECKIN_ACCEPTED, 19000);
  const int64_t checkin_bytes_downloaded = 500;
  const int64_t checkin_bytes_uploaded = 50;
  const int64_t checkin_network_duration_ms = 300;
  checkin.set_bytes_downloaded(checkin_bytes_downloaded);
  checkin.set_bytes_uploaded(checkin_bytes_uploaded);
  *checkin.mutable_network_duration() =
      TimeUtil::MillisecondsToDuration(checkin_network_duration_ms);
  *stats.add_phase_stats() = checkin;

  // Computation for task 3
  OperationalStats::PhaseStats computation_3;
  computation_3.set_phase(OperationalStats::PhaseStats::COMPUTATION);
  *computation_3.add_events() = CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED, 20000);
  *computation_3.add_events() = CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED, 21000);
  const std::string task_name_3 = "task_3";
  computation_3.set_task_name(task_name_3);
  const std::string uri_3 = "app://collection_2";
  OperationalStats::DatasetStats dataset_stats_3 =
      CreateDatasetStats(20, 500, 150);
  (*computation_3.mutable_dataset_stats())[uri_3] = dataset_stats_3;
  *stats.add_phase_stats() = computation_3;

  // Upload for task 3
  OperationalStats::PhaseStats upload_3;
  upload_3.set_phase(OperationalStats::PhaseStats::UPLOAD);
  *upload_3.add_events() = CreateEvent(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_STARTED, 22000);
  *upload_3.add_events() = CreateEvent(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_ERROR_IO, 23000);
  const int64_t upload_3_bytes_downloaded = 20;
  const int64_t upload_3_bytes_uploaded = 20;
  const int64_t upload_3_network_duration_ms = 10;
  upload_3.set_bytes_downloaded(upload_3_bytes_downloaded);
  upload_3.set_bytes_uploaded(upload_3_bytes_uploaded);
  *upload_3.mutable_network_duration() =
      TimeUtil::MillisecondsToDuration(upload_3_network_duration_ms);
  const std::string error_3 = "Network error.";
  upload_3.set_error_message(error_3);
  *stats.add_phase_stats() = upload_3;

  // Set singular fields
  std::string session = "session";
  std::string population = "population";
  stats.set_session_name(session);
  stats.set_population_name(population);

  // Set retry window
  int64_t min_delay_ms = 5000;
  int64_t max_delay_ms = 9000;
  RetryWindow retry;
  retry.set_retry_token("token");
  *retry.mutable_delay_min() = TimeUtil::MillisecondsToDuration(min_delay_ms);
  *retry.mutable_delay_max() = TimeUtil::MillisecondsToDuration(max_delay_ms);
  *stats.mutable_retry_window() = retry;

  OpStatsSequence opstats_sequence;
  ::google::protobuf::Timestamp currentTime = TimeUtil::GetCurrentTime();
  *opstats_sequence.mutable_earliest_trustworthy_time() = currentTime;
  *opstats_sequence.add_opstats() = std::move(stats);
  EXPECT_CALL(mock_db_, Read()).WillOnce(Return(opstats_sequence));

  ExampleSelector selector;
  selector.set_collection_uri(kOpStatsCollectionUri);
  absl::StatusOr<std::unique_ptr<ExampleIterator>> iterator_or =
      iterator_factory_.CreateExampleIterator(selector);

  ASSERT_TRUE(iterator_or.ok());
  std::unique_ptr<ExampleIterator> iterator = std::move(iterator_or.value());
  // We are expecting 3 examples, each representing a single task.
  // These examples are returned in reverse order.
  absl::StatusOr<std::string> serialized_example_1 = iterator->Next();
  ASSERT_TRUE(serialized_example_1.ok());
  tensorflow::Example example_1;
  example_1.ParseFromString(*serialized_example_1);

  int64_t bytes_downloaded_task_3 = eligibility_checkin_bytes_downloaded +
                                    checkin_bytes_downloaded +
                                    upload_3_bytes_downloaded;
  int64_t bytes_uploaded_task_3 = eligibility_checkin_bytes_uploaded +
                                  checkin_bytes_uploaded +
                                  upload_3_bytes_uploaded;
  int64_t network_duration_ms_task_3 = eligibility_checkin_network_duration_ms +
                                       checkin_network_duration_ms +
                                       upload_3_network_duration_ms;
  std::vector<int64_t> event_types_task_3{
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_CHECKIN_STARTED,
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_DISABLED,
      OperationalStats::Event::EVENT_KIND_CHECKIN_STARTED,
      OperationalStats::Event::EVENT_KIND_CHECKIN_ACCEPTED,
      OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED,
      OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED,
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_STARTED,
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_ERROR_IO};
  std::vector<int64_t> event_times_task_3{10000, 10100, 18000, 19000,
                                          20000, 21000, 22000, 23000};
  absl::flat_hash_map<std::string, OperationalStats::DatasetStats>
      dataset_stats_task_3;
  dataset_stats_task_3[uri_3] = dataset_stats_3;
  VerifyExample(example_1, session, population, task_name_3, error_3,
                bytes_downloaded_task_3, bytes_uploaded_task_3,
                network_duration_ms_task_3, event_types_task_3,
                event_times_task_3, dataset_stats_task_3, min_delay_ms,
                max_delay_ms, TimeUtil::TimestampToMilliseconds(currentTime));

  absl::StatusOr<std::string> serialized_example_2 = iterator->Next();
  ASSERT_TRUE(serialized_example_2.ok());
  tensorflow::Example example_2;
  example_2.ParseFromString(*serialized_example_2);

  int64_t bytes_downloaded_task_2 = eligibility_checkin_bytes_downloaded +
                                    multiple_task_assignments_bytes_downloaded;
  int64_t bytes_uploaded_task_2 = eligibility_checkin_bytes_uploaded +
                                  multiple_task_assignments_bytes_uploaded;
  int64_t network_duration_ms_task_2 =
      eligibility_checkin_network_duration_ms +
      multiple_task_assignments_network_duration_ms;
  std::vector<int64_t> event_types_task_2{
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_CHECKIN_STARTED,
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_DISABLED,
      OperationalStats::Event::EVENT_KIND_MULTIPLE_TASK_ASSIGNMENTS_STARTED,
      OperationalStats::Event::EVENT_KIND_MULTIPLE_TASK_ASSIGNMENTS_COMPLETED,
      OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED,
      OperationalStats::Event::EVENT_KIND_COMPUTATION_ERROR_TENSORFLOW};
  std::vector<int64_t> event_times_task_2{10000, 10100, 11000,
                                          12000, 17000, 17050};
  absl::flat_hash_map<std::string, OperationalStats::DatasetStats>
      dataset_stats_task_2;
  VerifyExample(example_2, session, population, task_name_2, error_message,
                bytes_downloaded_task_2, bytes_uploaded_task_2,
                network_duration_ms_task_2, event_types_task_2,
                event_times_task_2, dataset_stats_task_2, min_delay_ms,
                max_delay_ms, TimeUtil::TimestampToMilliseconds(currentTime));

  absl::StatusOr<std::string> serialized_example_3 = iterator->Next();
  ASSERT_TRUE(serialized_example_3.ok());
  tensorflow::Example example_3;
  example_3.ParseFromString(*serialized_example_3);

  int64_t bytes_downloaded_task_1 = eligibility_checkin_bytes_downloaded +
                                    multiple_task_assignments_bytes_downloaded +
                                    upload_1_bytes_downloaded;
  int64_t bytes_uploaded_task_1 = eligibility_checkin_bytes_uploaded +
                                  multiple_task_assignments_bytes_uploaded +
                                  upload_1_bytes_uploaded;
  int64_t network_duration_ms_task_1 =
      eligibility_checkin_network_duration_ms +
      multiple_task_assignments_network_duration_ms +
      upload_1_network_duration_ms;
  std::vector<int64_t> event_types_task_1{
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_CHECKIN_STARTED,
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_DISABLED,
      OperationalStats::Event::EVENT_KIND_MULTIPLE_TASK_ASSIGNMENTS_STARTED,
      OperationalStats::Event::EVENT_KIND_MULTIPLE_TASK_ASSIGNMENTS_COMPLETED,
      OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED,
      OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED,
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_STARTED,
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_FINISHED};
  std::vector<int64_t> event_times_task_1{10000, 10100, 11000, 12000,
                                          13000, 14000, 15000, 16000};
  absl::flat_hash_map<std::string, OperationalStats::DatasetStats>
      dataset_stats_task_1;
  dataset_stats_task_1[uri_1] = dataset_stats_1;
  VerifyExample(example_3, session, population, task_name_1, /*error=*/"",
                bytes_downloaded_task_1, bytes_uploaded_task_1,
                network_duration_ms_task_1, event_types_task_1,
                event_times_task_1, dataset_stats_task_1, min_delay_ms,
                max_delay_ms, TimeUtil::TimestampToMilliseconds(currentTime));

  ASSERT_THAT(iterator->Next(), IsCode(absl::StatusCode::kOutOfRange));
}

}  // anonymous namespace
}  // namespace opstats
}  // namespace client
}  // namespace fcp
