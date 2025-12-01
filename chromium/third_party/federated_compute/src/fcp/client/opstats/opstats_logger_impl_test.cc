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

#include "fcp/client/opstats/opstats_logger_impl.h"

#include <cstdint>
#include <filesystem>  // NOLINT(build/c++17)
#include <memory>
#include <string>
#include <utility>

#include "google/protobuf/util/time_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "fcp/base/time_util.h"
#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/engine/engine.pb.h"
#include "fcp/client/histogram_counters.pb.h"
#include "fcp/client/opstats/opstats_logger.h"
#include "fcp/client/opstats/pds_backed_opstats_db.h"
#include "fcp/client/stats.h"
#include "fcp/client/test_helpers.h"
#include "fcp/protos/opstats.pb.h"
#include "fcp/testing/testing.h"
#include "protostore/file-storage.h"

namespace fcp {
namespace client {
namespace opstats {
namespace {

using google::internal::federatedml::v2::RetryWindow;
using google::protobuf::Timestamp;
using google::protobuf::util::TimeUtil;
using testing::Ge;
using testing::Return;
using testing::StrictMock;

constexpr char kSessionName[] = "SESSION";
constexpr char kPopulationName[] = "POPULATION";
constexpr char kTaskName[] = "TASK";

class OpStatsLoggerImplTest : public testing::Test {
 protected:
  void SetUp() override {
    ON_CALL(mock_flags_, opstats_ttl_days()).WillByDefault(Return(1));
    ON_CALL(mock_flags_, opstats_db_size_limit_bytes())
        .WillByDefault(Return(1 * 1024 * 1024));
    base_dir_ = testing::TempDir();
  }

  void TearDown() override {
    auto db = PdsBackedOpStatsDb::Create(
        base_dir_, mock_flags_.opstats_ttl_days() * absl::Hours(24),
        mock_log_manager_, mock_flags_.opstats_db_size_limit_bytes());
    ASSERT_OK(db);
    EXPECT_CALL(
        mock_log_manager_,
        LogToLongHistogram(OPSTATS_DB_SIZE_BYTES, /*execution_index=*/0,
                           /*epoch_index=*/0, engine::DataSourceType::DATASET,
                           /*value=*/0));
    EXPECT_CALL(
        mock_log_manager_,
        LogToLongHistogram(HistogramCounters::OPSTATS_DB_NUM_ENTRIES,
                           /*execution_index=*/0, /*epoch_index=*/0,
                           engine::DataSourceType::DATASET, /*value=*/0));
    EXPECT_THAT((*db)->Transform([](OpStatsSequence& data) { data.Clear(); }),
                IsOk());
  }

  // Checks that the expected and actual protos are equivalent, ignoring the
  // timestamps in the actual proto, which must also be increasing.
  void CheckEqualProtosAndIncreasingTimestamps(const Timestamp& start_time,
                                               const OpStatsSequence& expected,
                                               OpStatsSequence actual) {
    auto previous_timestamp = start_time;
    for (auto& opstats : *actual.mutable_opstats()) {
      for (auto& event : *opstats.mutable_events()) {
        EXPECT_GE(event.timestamp(), previous_timestamp);
        previous_timestamp = event.timestamp();
        // Remove the timestamp
        event.clear_timestamp();
      }
      for (auto& phase_stats : *opstats.mutable_phase_stats()) {
        for (auto& event : *phase_stats.mutable_events()) {
          EXPECT_GE(event.timestamp(), previous_timestamp);
          previous_timestamp = event.timestamp();
          event.clear_timestamp();
        }
      }
    }
    actual.clear_earliest_trustworthy_time();
    EXPECT_THAT(actual, EqualsProto(expected));
  }

  void ExpectOpstatsEnabledEvents(int num_opstats_loggers) {
    ExpectOpstatsEnabledEvents(num_opstats_loggers, num_opstats_loggers);
  }

  void ExpectOpstatsEnabledEvents(int num_opstats_loggers,
                                  int num_opstats_commits) {
    EXPECT_CALL(mock_log_manager_,
                LogDiag(DebugDiagCode::TRAINING_OPSTATS_ENABLED))
        .Times(num_opstats_loggers);
    // Logged when the class is initialized.
    EXPECT_CALL(mock_log_manager_,
                LogDiag(ProdDiagCode::OPSTATS_DB_COMMIT_EXPECTED))
        .Times(num_opstats_loggers);
    EXPECT_CALL(mock_log_manager_,
                LogDiag(ProdDiagCode::OPSTATS_DB_COMMIT_ATTEMPTED))
        .Times(num_opstats_commits);
    EXPECT_CALL(
        mock_log_manager_,
        LogToLongHistogram(TRAINING_OPSTATS_COMMIT_LATENCY,
                           /*execution_index=*/0, /*epoch_index=*/0,
                           engine::DataSourceType::DATASET, /*value=*/Ge(0)))
        .Times(num_opstats_commits);
    EXPECT_CALL(
        mock_log_manager_,
        LogToLongHistogram(OPSTATS_DB_SIZE_BYTES, /*execution_index=*/0,
                           /*epoch_index=*/0, engine::DataSourceType::DATASET,
                           /*value=*/Ge(0)))
        .Times(num_opstats_commits);
    EXPECT_CALL(
        mock_log_manager_,
        LogToLongHistogram(OPSTATS_DB_NUM_ENTRIES, /*execution_index=*/0,
                           /*epoch_index=*/0, engine::DataSourceType::DATASET,
                           /*value=*/Ge(0)))
        .Times(num_opstats_commits);
  }

  RetryWindow CreateRetryWindow(const std::string& retry_token,
                                int64_t delay_min_seconds,
                                int64_t delay_max_seconds) {
    RetryWindow retry_window;
    retry_window.set_retry_token(retry_token);
    retry_window.mutable_delay_min()->set_seconds(delay_min_seconds);
    retry_window.mutable_delay_max()->set_seconds(delay_max_seconds);
    return retry_window;
  }

  OperationalStats::DatasetStats CreateDatasetStats(int32_t example_cnt,
                                                    int64_t example_bytes) {
    OperationalStats::DatasetStats dataset_stats;
    dataset_stats.set_num_examples_read(example_cnt);
    dataset_stats.set_num_bytes_read(example_bytes);
    return dataset_stats;
  }

  NetworkStats CreateNetworkStats(int64_t bytes_downloaded,
                                  int64_t bytes_uploaded,
                                  absl::Duration network_duration) {
    NetworkStats network_stats;
    network_stats.bytes_downloaded = bytes_downloaded;
    network_stats.bytes_uploaded = bytes_uploaded;
    network_stats.network_duration = network_duration;
    return network_stats;
  }

  std::string base_dir_;
  MockFlags mock_flags_;
  StrictMock<MockLogManager> mock_log_manager_;
};

TEST_F(OpStatsLoggerImplTest, SetTaskNameWithoutStartPhaseLogging) {
  EXPECT_DEATH(
      {
        auto opstats_logger =
            CreateOpStatsLogger(base_dir_, &mock_flags_, &mock_log_manager_,
                                kSessionName, kPopulationName);
        opstats_logger->AddEventAndSetTaskName(
            kTaskName, OperationalStats::Event::EVENT_KIND_CHECKIN_ACCEPTED);
      },
      testing::HasSubstr(
          "AddEventAndSetTaskName called before StartLoggingForPhase"));
}

TEST_F(OpStatsLoggerImplTest, NewRunAfterCorruption) {
  auto start_time = TimeUtil::GetCurrentTime();
  ExpectOpstatsEnabledEvents(/*num_opstats_loggers=*/2);

  auto opstats_logger =
      CreateOpStatsLogger(base_dir_, &mock_flags_, &mock_log_manager_,
                          kSessionName, kPopulationName);
  opstats_logger->StartLoggingForPhase(OperationalStats::PhaseStats::CHECKIN);
  opstats_logger->AddEventAndSetTaskName(
      kTaskName, OperationalStats::Event::EVENT_KIND_CHECKIN_ACCEPTED);
  opstats_logger.reset();

  // Make the db file corrupt
  {
    std::filesystem::path db_path(base_dir_);
    db_path /= PdsBackedOpStatsDb::kParentDir;
    db_path /= PdsBackedOpStatsDb::kDbFileName;
    protostore::FileStorage file_storage;
    std::unique_ptr<protostore::OutputStream> ostream =
        file_storage.OpenForWrite(db_path).value();
    EXPECT_THAT(ostream->Append("not a proto"), IsOk());
    EXPECT_THAT(ostream->Close(), IsOk());
  }

  EXPECT_CALL(mock_log_manager_, LogDiag(ProdDiagCode::OPSTATS_READ_FAILED));
  auto opstats_logger_no_population = CreateOpStatsLogger(
      base_dir_, &mock_flags_, &mock_log_manager_, kSessionName,
      /*population_name=*/"");
  opstats_logger_no_population->StartLoggingForPhase(
      OperationalStats::PhaseStats::CHECKIN);
  opstats_logger_no_population->AddEventAndSetTaskName(
      kTaskName, OperationalStats::Event::EVENT_KIND_CHECKIN_ACCEPTED);

  opstats_logger_no_population.reset();

  auto db = PdsBackedOpStatsDb::Create(
      base_dir_, mock_flags_.opstats_ttl_days() * absl::Hours(24),
      mock_log_manager_, mock_flags_.opstats_db_size_limit_bytes());
  ASSERT_OK(db);
  auto data = (*db)->Read();
  ASSERT_OK(data);

  // Expect only the second run to be represented in the db.
  OpStatsSequence expected;
  auto new_opstats = expected.add_opstats();
  new_opstats->set_session_name(kSessionName);
  auto phase_stats = new_opstats->add_phase_stats();
  phase_stats->set_phase(OperationalStats::PhaseStats::CHECKIN);
  phase_stats->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_CHECKIN_ACCEPTED);
  phase_stats->set_task_name(kTaskName);
  CheckEqualProtosAndIncreasingTimestamps(start_time, expected, *data);
}

TEST_F(OpStatsLoggerImplTest, AddEventWithoutStartPhaseLogging) {
  EXPECT_DEATH(
      {
        auto opstats_logger =
            CreateOpStatsLogger(base_dir_, &mock_flags_, &mock_log_manager_,
                                kSessionName, kPopulationName);
        opstats_logger->AddEvent(
            OperationalStats::Event::EVENT_KIND_CHECKIN_STARTED);
      },
      testing::HasSubstr("AddEvent called before StartLoggingForPhase"));
}

TEST_F(OpStatsLoggerImplTest, AddInitializationEventWithoutStartPhaseLogging) {
  auto start_time = TimeUtil::GetCurrentTime();

  ExpectOpstatsEnabledEvents(/*num_opstats_loggers=*/1);
  auto opstats_logger =
      CreateOpStatsLogger(base_dir_, &mock_flags_, &mock_log_manager_,
                          kSessionName, kPopulationName);
  opstats_logger->AddEvent(
      OperationalStats::Event::EVENT_KIND_INITIALIZATION_ERROR_NONFATAL);
  opstats_logger.reset();

  auto db = PdsBackedOpStatsDb::Create(
      base_dir_, mock_flags_.opstats_ttl_days() * absl::Hours(24),
      mock_log_manager_, mock_flags_.opstats_db_size_limit_bytes());
  ASSERT_OK(db);
  auto data = (*db)->Read();
  ASSERT_OK(data);

  OpStatsSequence expected;
  auto new_opstats = expected.add_opstats();
  new_opstats->set_session_name(kSessionName);
  new_opstats->set_population_name(kPopulationName);
  new_opstats->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_INITIALIZATION_ERROR_NONFATAL);

  CheckEqualProtosAndIncreasingTimestamps(start_time, expected, *data);
}

TEST_F(OpStatsLoggerImplTest, AddEventAfterTtl) {
  auto start_time = TimeUtil::GetCurrentTime();
  ExpectOpstatsEnabledEvents(/*num_opstats_loggers=*/2);

  // Set the ttl to 0 so that previous data will be wiped out each time the
  // logger tries to commit new data.
  EXPECT_CALL(mock_flags_, opstats_ttl_days()).WillRepeatedly(Return(0));
  auto opstats_logger =
      CreateOpStatsLogger(base_dir_, &mock_flags_, &mock_log_manager_,
                          kSessionName, kPopulationName);
  opstats_logger->StartLoggingForPhase(OperationalStats::PhaseStats::CHECKIN);
  opstats_logger->AddEvent(OperationalStats::Event::EVENT_KIND_CHECKIN_STARTED);
  opstats_logger.reset();

  auto opstats_logger_no_population = CreateOpStatsLogger(
      base_dir_, &mock_flags_, &mock_log_manager_, kSessionName,
      /*population_name=*/"");
  opstats_logger_no_population->StartLoggingForPhase(
      OperationalStats::PhaseStats::CHECKIN);
  opstats_logger_no_population->AddEvent(
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_CHECKIN_STARTED);
  opstats_logger_no_population->AddEvent(
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_REJECTED);
  opstats_logger_no_population.reset();

  auto db = PdsBackedOpStatsDb::Create(
      base_dir_, mock_flags_.opstats_ttl_days() * absl::Hours(24),
      mock_log_manager_, mock_flags_.opstats_db_size_limit_bytes());
  auto data = (*db)->Read();
  ASSERT_OK(data);

  // Expect the db to contain only data associated with the second run. The
  // second run should be complete, however.
  OpStatsSequence expected;
  auto new_opstats = expected.add_opstats();
  new_opstats->set_session_name(kSessionName);
  auto phase_stats = new_opstats->add_phase_stats();
  phase_stats->set_phase(OperationalStats::PhaseStats::CHECKIN);
  phase_stats->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_CHECKIN_STARTED);
  phase_stats->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_REJECTED);

  CheckEqualProtosAndIncreasingTimestamps(start_time, expected, *data);
}

TEST_F(OpStatsLoggerImplTest,
       AddEventWithErrorMessageWithoutStartPhaseLogging) {
  EXPECT_DEATH(
      {
        auto opstats_logger =
            CreateOpStatsLogger(base_dir_, &mock_flags_, &mock_log_manager_,
                                kSessionName, kPopulationName);
        opstats_logger->AddEventWithErrorMessage(
            OperationalStats::Event::EVENT_KIND_ERROR_IO, "error");
      },
      testing::HasSubstr(
          "AddEventWithErrorMessage called before StartLoggingForPhase"));
}

TEST_F(OpStatsLoggerImplTest, SetMinSepPolicyIndex) {
  EXPECT_CALL(mock_flags_, log_min_sep_index_to_phase_stats())
      .WillRepeatedly(Return(false));

  auto start_time = TimeUtil::GetCurrentTime();
  ExpectOpstatsEnabledEvents(/*num_opstats_loggers=*/1);

  auto opstats_logger =
      CreateOpStatsLogger(base_dir_, &mock_flags_, &mock_log_manager_,
                          kSessionName, kPopulationName);
  opstats_logger->StartLoggingForPhase(OperationalStats::PhaseStats::CHECKIN);
  opstats_logger->AddEvent(OperationalStats::Event::EVENT_KIND_CHECKIN_STARTED);
  opstats_logger->SetMinSepPolicyIndex(1);

  opstats_logger.reset();

  auto db = PdsBackedOpStatsDb::Create(
      base_dir_, mock_flags_.opstats_ttl_days() * absl::Hours(24),
      mock_log_manager_, mock_flags_.opstats_db_size_limit_bytes());
  ASSERT_OK(db);
  auto data = (*db)->Read();
  ASSERT_OK(data);

  OpStatsSequence expected;
  auto new_opstats = expected.add_opstats();
  new_opstats->set_session_name(kSessionName);
  new_opstats->set_population_name(kPopulationName);
  new_opstats->set_min_sep_policy_index(1);
  auto phase_stats = new_opstats->add_phase_stats();
  phase_stats->set_phase(OperationalStats::PhaseStats::CHECKIN);
  phase_stats->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_CHECKIN_STARTED);

  CheckEqualProtosAndIncreasingTimestamps(start_time, expected, *data);
}

TEST_F(OpStatsLoggerImplTest, SetMinSepPolicyIndexWithoutStartPhaseLogging) {
  EXPECT_CALL(mock_flags_, log_min_sep_index_to_phase_stats())
      .WillRepeatedly(Return(true));

  EXPECT_DEATH(
      {
        auto opstats_logger =
            CreateOpStatsLogger(base_dir_, &mock_flags_, &mock_log_manager_,
                                kSessionName, kPopulationName);
        opstats_logger->SetMinSepPolicyIndex(1);
      },
      "SetMinSepPolicyIndex called before StartLoggingForPhase");
}

TEST_F(OpStatsLoggerImplTest, UpdateDatasetStatsWithoutStartPhaseLogging) {
  const std::string kCollectionUri = "app:/collection_uri";
  EXPECT_DEATH(
      {
        auto opstats_logger =
            CreateOpStatsLogger(base_dir_, &mock_flags_, &mock_log_manager_,
                                kSessionName, kPopulationName);
        opstats_logger->UpdateDatasetStats(kCollectionUri, 100, 1000);
      },
      "UpdateDatasetStats called before StartLoggingForPhase");
}

TEST_F(OpStatsLoggerImplTest, RecordCollectionFirstAccessTime) {
  ExpectOpstatsEnabledEvents(/*num_opstats_loggers=*/1);

  auto opstats_logger =
      CreateOpStatsLogger(base_dir_, &mock_flags_, &mock_log_manager_,
                          kSessionName, kPopulationName);
  const std::string kCollectionUri = "app:/collection_uri";
  const std::string kCollectionUriOther = "app:/collection_uri_other";
  absl::Time collection_first_access_time = absl::Now();
  opstats_logger->StartLoggingForPhase(
      OperationalStats::PhaseStats::COMPUTATION);
  opstats_logger->RecordCollectionFirstAccessTime(kCollectionUri,
                                                  collection_first_access_time);
  opstats_logger->UpdateDatasetStats(kCollectionUri,
                                     /*additional_example_count=*/100,
                                     /*additional_example_size_bytes=*/1000);
  opstats_logger->UpdateDatasetStats(kCollectionUriOther,
                                     /*additional_example_count=*/200,
                                     /*additional_example_size_bytes=*/2000);
  opstats_logger.reset();

  auto db = PdsBackedOpStatsDb::Create(
      base_dir_, mock_flags_.opstats_ttl_days() * absl::Hours(24),
      mock_log_manager_, mock_flags_.opstats_db_size_limit_bytes());
  auto data = (*db)->Read();
  ASSERT_OK(data);

  OpStatsSequence expected;
  auto new_opstats = expected.add_opstats();
  new_opstats->set_session_name(kSessionName);
  new_opstats->set_population_name(kPopulationName);
  auto phase_stats = new_opstats->add_phase_stats();
  phase_stats->set_phase(OperationalStats::PhaseStats::COMPUTATION);
  OperationalStats::DatasetStats dataset_stats;
  dataset_stats.set_num_examples_read(100);
  dataset_stats.set_num_bytes_read(1000);
  *dataset_stats.mutable_first_access_timestamp() =
      ::fcp::TimeUtil::ConvertAbslToProtoTimestamp(
          collection_first_access_time);
  (*phase_stats->mutable_dataset_stats())[kCollectionUri] =
      std::move(dataset_stats);
  OperationalStats::DatasetStats dataset_stats_other;
  dataset_stats_other.set_num_examples_read(200);
  dataset_stats_other.set_num_bytes_read(2000);
  (*phase_stats->mutable_dataset_stats())[kCollectionUriOther] =
      std::move(dataset_stats_other);

  (*data).clear_earliest_trustworthy_time();
  EXPECT_THAT(*data, EqualsProto(expected));
}

TEST_F(OpStatsLoggerImplTest, SetRetryWindow) {
  ExpectOpstatsEnabledEvents(/*num_opstats_loggers=*/1);

  auto opstats_logger =
      CreateOpStatsLogger(base_dir_, &mock_flags_, &mock_log_manager_,
                          kSessionName, kPopulationName);
  opstats_logger->SetRetryWindow(CreateRetryWindow("retry_token", 100, 200));
  opstats_logger->SetRetryWindow(CreateRetryWindow("retry_token", 300, 400));
  opstats_logger.reset();

  auto db = PdsBackedOpStatsDb::Create(
      base_dir_, mock_flags_.opstats_ttl_days() * absl::Hours(24),
      mock_log_manager_, mock_flags_.opstats_db_size_limit_bytes());
  ASSERT_OK(db);
  auto data = (*db)->Read();
  ASSERT_OK(data);

  OpStatsSequence expected;
  auto new_opstats = expected.add_opstats();
  new_opstats->set_session_name(kSessionName);
  new_opstats->set_population_name(kPopulationName);
  *new_opstats->mutable_retry_window() =
      CreateRetryWindow(/*retry_token=*/"", 300, 400);

  (*data).clear_earliest_trustworthy_time();
  EXPECT_THAT(*data, EqualsProto(expected));
}

TEST_F(OpStatsLoggerImplTest, AddEventCommitAddMoreEvents) {
  auto start_time = TimeUtil::GetCurrentTime();
  ExpectOpstatsEnabledEvents(
      /*num_opstats_loggers=*/2, /*num_opstats_commits=*/4);

  auto opstats_logger =
      CreateOpStatsLogger(base_dir_, &mock_flags_, &mock_log_manager_,
                          kSessionName, kPopulationName);
  opstats_logger->StartLoggingForPhase(OperationalStats::PhaseStats::CHECKIN);
  opstats_logger->AddEvent(OperationalStats::Event::EVENT_KIND_CHECKIN_STARTED);
  opstats_logger.reset();

  auto opstats_logger_no_population = CreateOpStatsLogger(
      base_dir_, &mock_flags_, &mock_log_manager_, kSessionName,
      /*population_name=*/"");
  opstats_logger_no_population->StartLoggingForPhase(
      OperationalStats::PhaseStats::ELIGIBILITY_EVAL_CHECKIN);
  opstats_logger_no_population->AddEvent(
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_CHECKIN_STARTED);
  ASSERT_OK(opstats_logger_no_population->CommitToStorage());
  opstats_logger_no_population->AddEvent(
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_REJECTED);
  opstats_logger_no_population->StopLoggingForTheCurrentPhase();
  ASSERT_OK(opstats_logger_no_population->CommitToStorage());
  opstats_logger_no_population->StartLoggingForPhase(
      OperationalStats::PhaseStats::COMPUTATION);
  opstats_logger_no_population->AddEvent(
      OperationalStats::Event::EVENT_KIND_TRAIN_NOT_STARTED);
  opstats_logger_no_population.reset();

  auto db = PdsBackedOpStatsDb::Create(
      base_dir_, mock_flags_.opstats_ttl_days() * absl::Hours(24),
      mock_log_manager_, mock_flags_.opstats_db_size_limit_bytes());
  ASSERT_OK(db);
  auto data = (*db)->Read();
  ASSERT_OK(data);

  OpStatsSequence expected;
  // Add the first run
  auto first_run = expected.add_opstats();
  first_run->set_session_name(kSessionName);
  first_run->set_population_name(kPopulationName);
  auto phase_stats = first_run->add_phase_stats();
  phase_stats->set_phase(OperationalStats::PhaseStats::CHECKIN);
  phase_stats->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_CHECKIN_STARTED);
  // Add the second run
  auto second_run = expected.add_opstats();
  second_run->set_session_name(kSessionName);
  auto second_phase_stats = second_run->add_phase_stats();
  second_phase_stats->set_phase(
      OperationalStats::PhaseStats::ELIGIBILITY_EVAL_CHECKIN);
  second_phase_stats->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_CHECKIN_STARTED);
  second_phase_stats->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_REJECTED);
  auto third_phase_stats = second_run->add_phase_stats();
  third_phase_stats->set_phase(OperationalStats::PhaseStats::COMPUTATION);
  third_phase_stats->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_TRAIN_NOT_STARTED);

  CheckEqualProtosAndIncreasingTimestamps(start_time, expected, *data);
}

TEST_F(OpStatsLoggerImplTest, MisconfiguredTtlMultipleCommit) {
  auto start_time = TimeUtil::GetCurrentTime();
  ExpectOpstatsEnabledEvents(/*num_opstats_loggers=*/1,
                             /*num_opstats_commits*/ 3);
  auto db_zero_ttl = PdsBackedOpStatsDb::Create(
                         base_dir_, absl::ZeroDuration(), mock_log_manager_,
                         mock_flags_.opstats_db_size_limit_bytes())
                         .value();
  auto opstats_logger = std::make_unique<OpStatsLoggerImpl>(
      std::move(db_zero_ttl), &mock_log_manager_, &mock_flags_, kSessionName,
      kPopulationName);
  opstats_logger->StartLoggingForPhase(
      OperationalStats::PhaseStats::ELIGIBILITY_EVAL_CHECKIN);
  opstats_logger->AddEvent(
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_CHECKIN_STARTED);
  ASSERT_OK(opstats_logger->CommitToStorage());
  opstats_logger->AddEvent(
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_REJECTED);
  ASSERT_OK(opstats_logger->CommitToStorage());
  opstats_logger->StopLoggingForTheCurrentPhase();
  opstats_logger->StartLoggingForPhase(
      OperationalStats::PhaseStats::COMPUTATION);
  opstats_logger->AddEvent(
      OperationalStats::Event::EVENT_KIND_TRAIN_NOT_STARTED);
  opstats_logger.reset();

  auto db = PdsBackedOpStatsDb::Create(
      base_dir_, mock_flags_.opstats_ttl_days() * absl::Hours(24),
      mock_log_manager_, mock_flags_.opstats_db_size_limit_bytes());
  ASSERT_OK(db);
  auto data = (*db)->Read();
  ASSERT_OK(data);

  // Even though we had corruption in the middle of the run, it should be ok
  // because we committed the entire history successfully at the end.
  OpStatsSequence expected;
  auto expected_stats = expected.add_opstats();
  expected_stats->set_population_name(kPopulationName);
  expected_stats->set_session_name(kSessionName);
  auto phase_stats = expected_stats->add_phase_stats();
  phase_stats->set_phase(
      OperationalStats::PhaseStats::ELIGIBILITY_EVAL_CHECKIN);
  phase_stats->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_CHECKIN_STARTED);
  phase_stats->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_REJECTED);
  auto another_phase_stats = expected_stats->add_phase_stats();
  another_phase_stats->set_phase(OperationalStats::PhaseStats::COMPUTATION);
  another_phase_stats->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_TRAIN_NOT_STARTED);

  CheckEqualProtosAndIncreasingTimestamps(start_time, expected, *data);
}

TEST_F(OpStatsLoggerImplTest, PhaseStatsAddEventAndSetTaskName) {
  auto start_time = TimeUtil::GetCurrentTime();
  ExpectOpstatsEnabledEvents(/*num_opstats_loggers=*/1,
                             /*num_opstats_commits*/ 1);
  auto opstats_logger =
      CreateOpStatsLogger(base_dir_, &mock_flags_, &mock_log_manager_,
                          kSessionName, kPopulationName);
  opstats_logger->StartLoggingForPhase(
      OperationalStats::PhaseStats::COMPUTATION);
  opstats_logger->AddEventAndSetTaskName(
      kTaskName, OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED);
  opstats_logger->StopLoggingForTheCurrentPhase();

  ASSERT_EQ(opstats_logger->GetCurrentTaskName(), kTaskName);

  opstats_logger.reset();

  auto db = PdsBackedOpStatsDb::Create(
      base_dir_, mock_flags_.opstats_ttl_days() * absl::Hours(24),
      mock_log_manager_, mock_flags_.opstats_db_size_limit_bytes());
  ASSERT_OK(db);
  auto data = (*db)->Read();
  ASSERT_OK(data);

  OpStatsSequence expected;
  auto expected_stats = expected.add_opstats();
  expected_stats->set_population_name(kPopulationName);
  expected_stats->set_session_name(kSessionName);
  OperationalStats::PhaseStats* phase_stats = expected_stats->add_phase_stats();
  phase_stats->set_phase(OperationalStats::PhaseStats::COMPUTATION);
  phase_stats->set_task_name(kTaskName);
  phase_stats->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED);

  CheckEqualProtosAndIncreasingTimestamps(start_time, expected, *data);
}

// For initialization events such as EVENT_KIND_TRAIN_NOT_STARTED, it'll be
// logged before we enter any Phases.
TEST_F(OpStatsLoggerImplTest, PhaseStatsAddEventStartLoggingNotCalled) {
  auto start_time = TimeUtil::GetCurrentTime();
  ExpectOpstatsEnabledEvents(/*num_opstats_loggers=*/1,
                             /*num_opstats_commits*/ 1);
  auto opstats_logger =
      CreateOpStatsLogger(base_dir_, &mock_flags_, &mock_log_manager_,
                          kSessionName, kPopulationName);
  opstats_logger->AddEvent(
      OperationalStats::Event::EVENT_KIND_TRAIN_NOT_STARTED);
  opstats_logger.reset();

  auto db = PdsBackedOpStatsDb::Create(
      base_dir_, mock_flags_.opstats_ttl_days() * absl::Hours(24),
      mock_log_manager_, mock_flags_.opstats_db_size_limit_bytes());
  ASSERT_OK(db);
  auto data = (*db)->Read();
  ASSERT_OK(data);

  OpStatsSequence expected;
  auto expected_stats = expected.add_opstats();
  expected_stats->set_population_name(kPopulationName);
  expected_stats->set_session_name(kSessionName);
  expected_stats->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_TRAIN_NOT_STARTED);

  CheckEqualProtosAndIncreasingTimestamps(start_time, expected, *data);
}

TEST_F(OpStatsLoggerImplTest, PhaseStatsAddEvent) {
  auto start_time = TimeUtil::GetCurrentTime();
  ExpectOpstatsEnabledEvents(/*num_opstats_loggers=*/1,
                             /*num_opstats_commits*/ 1);

  auto opstats_logger =
      CreateOpStatsLogger(base_dir_, &mock_flags_, &mock_log_manager_,
                          kSessionName, kPopulationName);
  opstats_logger->StartLoggingForPhase(
      OperationalStats::PhaseStats::COMPUTATION);
  const std::string task_1 = "task_1";
  opstats_logger->AddEventAndSetTaskName(
      task_1, OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED);
  opstats_logger->AddEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED);
  opstats_logger->StopLoggingForTheCurrentPhase();

  opstats_logger->StartLoggingForPhase(OperationalStats::PhaseStats::UPLOAD);
  opstats_logger->AddEvent(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_STARTED);
  opstats_logger->AddEvent(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_FINISHED);
  opstats_logger->StopLoggingForTheCurrentPhase();

  opstats_logger->StartLoggingForPhase(
      OperationalStats::PhaseStats::COMPUTATION);
  const std::string task_2 = "task_2";
  opstats_logger->AddEventAndSetTaskName(
      task_2, OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED);
  opstats_logger->AddEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_CLIENT_INTERRUPTED);
  opstats_logger->StopLoggingForTheCurrentPhase();

  ASSERT_EQ(opstats_logger->GetCurrentTaskName(), task_2);

  opstats_logger.reset();

  auto db = PdsBackedOpStatsDb::Create(
      base_dir_, mock_flags_.opstats_ttl_days() * absl::Hours(24),
      mock_log_manager_, mock_flags_.opstats_db_size_limit_bytes());
  ASSERT_OK(db);
  auto data = (*db)->Read();
  ASSERT_OK(data);

  OpStatsSequence expected;
  auto expected_stats = expected.add_opstats();
  expected_stats->set_population_name(kPopulationName);
  expected_stats->set_session_name(kSessionName);
  OperationalStats::PhaseStats* computation_phase_1 =
      expected_stats->add_phase_stats();
  computation_phase_1->set_phase(OperationalStats::PhaseStats::COMPUTATION);
  computation_phase_1->set_task_name(task_1);
  computation_phase_1->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED);
  computation_phase_1->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED);

  OperationalStats::PhaseStats* upload_phase_1 =
      expected_stats->add_phase_stats();
  upload_phase_1->set_phase(OperationalStats::PhaseStats::UPLOAD);
  upload_phase_1->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_STARTED);
  upload_phase_1->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_FINISHED);

  OperationalStats::PhaseStats* computation_phase_2 =
      expected_stats->add_phase_stats();
  computation_phase_2->set_phase(OperationalStats::PhaseStats::COMPUTATION);
  computation_phase_2->set_task_name(task_2);
  computation_phase_2->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED);
  computation_phase_2->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_CLIENT_INTERRUPTED);

  CheckEqualProtosAndIncreasingTimestamps(start_time, expected, *data);
}

TEST_F(OpStatsLoggerImplTest, PhaseStatsAddEventWithErrorMessage) {
  auto start_time = TimeUtil::GetCurrentTime();
  ExpectOpstatsEnabledEvents(/*num_opstats_loggers=*/1,
                             /*num_opstats_commits*/ 1);

  auto opstats_logger =
      CreateOpStatsLogger(base_dir_, &mock_flags_, &mock_log_manager_,
                          kSessionName, kPopulationName);
  opstats_logger->StartLoggingForPhase(
      OperationalStats::PhaseStats::COMPUTATION);
  opstats_logger->AddEventAndSetTaskName(
      kTaskName, OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED);
  const std::string error_message = "Missing op kernel.";
  opstats_logger->AddEventWithErrorMessage(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_ERROR_TENSORFLOW,
      error_message);
  opstats_logger->StopLoggingForTheCurrentPhase();

  ASSERT_EQ(opstats_logger->GetCurrentTaskName(), kTaskName);

  opstats_logger.reset();

  auto db = PdsBackedOpStatsDb::Create(
      base_dir_, mock_flags_.opstats_ttl_days() * absl::Hours(24),
      mock_log_manager_, mock_flags_.opstats_db_size_limit_bytes());
  ASSERT_OK(db);
  auto data = (*db)->Read();
  ASSERT_OK(data);

  OpStatsSequence expected;
  auto expected_stats = expected.add_opstats();
  expected_stats->set_population_name(kPopulationName);
  expected_stats->set_session_name(kSessionName);
  OperationalStats::PhaseStats* computation_phase =
      expected_stats->add_phase_stats();
  computation_phase->set_phase(OperationalStats::PhaseStats::COMPUTATION);
  computation_phase->set_task_name(kTaskName);
  computation_phase->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED);
  computation_phase->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_ERROR_TENSORFLOW);
  computation_phase->set_error_message(error_message);

  CheckEqualProtosAndIncreasingTimestamps(start_time, expected, *data);
}

// For initialization events such as EVENT_KIND_INITIALIZATION_ERROR_FATAL,
// it'll be logged before we enter any Phases.
TEST_F(OpStatsLoggerImplTest,
       PhaseStatsAddEventWithErrorMessageStartLoggingNotCalled) {
  auto start_time = TimeUtil::GetCurrentTime();
  ExpectOpstatsEnabledEvents(/*num_opstats_loggers=*/1,
                             /*num_opstats_commits*/ 1);
  auto opstats_logger =
      CreateOpStatsLogger(base_dir_, &mock_flags_, &mock_log_manager_,
                          kSessionName, kPopulationName);
  const std::string error_message = "Fatal initialization error.";
  opstats_logger->AddEventWithErrorMessage(
      OperationalStats::Event::EVENT_KIND_INITIALIZATION_ERROR_FATAL,
      error_message);
  opstats_logger.reset();

  auto db = PdsBackedOpStatsDb::Create(
      base_dir_, mock_flags_.opstats_ttl_days() * absl::Hours(24),
      mock_log_manager_, mock_flags_.opstats_db_size_limit_bytes());
  ASSERT_OK(db);
  auto data = (*db)->Read();
  ASSERT_OK(data);

  OpStatsSequence expected;
  auto expected_stats = expected.add_opstats();
  expected_stats->set_population_name(kPopulationName);
  expected_stats->set_session_name(kSessionName);
  expected_stats->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_INITIALIZATION_ERROR_FATAL);
  expected_stats->set_error_message(error_message);

  CheckEqualProtosAndIncreasingTimestamps(start_time, expected, *data);
}

TEST_F(OpStatsLoggerImplTest, PhaseStatsUpdateDatasetStats) {
  auto start_time = TimeUtil::GetCurrentTime();
  ExpectOpstatsEnabledEvents(/*num_opstats_loggers=*/1,
                             /*num_opstats_commits*/ 1);

  auto opstats_logger =
      CreateOpStatsLogger(base_dir_, &mock_flags_, &mock_log_manager_,
                          kSessionName, kPopulationName);
  opstats_logger->StartLoggingForPhase(
      OperationalStats::PhaseStats::COMPUTATION);
  opstats_logger->AddEventAndSetTaskName(
      kTaskName, OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED);
  const std::string collection_url_1 = "app://collection_1";
  const int32_t example_cnt_1 = 5;
  const int64_t example_size_1 = 100;
  opstats_logger->UpdateDatasetStats(collection_url_1, example_cnt_1,
                                     example_size_1);

  const std::string collection_url_2 = "app://collection_2";
  const int32_t example_cnt_2 = 10;
  const int64_t example_size_2 = 2000;
  opstats_logger->UpdateDatasetStats(collection_url_2, example_cnt_2,
                                     example_size_2);

  const int32_t example_cnt_3 = 6;
  const int64_t example_size_3 = 120;
  opstats_logger->UpdateDatasetStats(collection_url_1, example_cnt_3,
                                     example_size_3);

  opstats_logger->StopLoggingForTheCurrentPhase();

  ASSERT_EQ(opstats_logger->GetCurrentTaskName(), kTaskName);

  opstats_logger.reset();

  auto db = PdsBackedOpStatsDb::Create(
      base_dir_, mock_flags_.opstats_ttl_days() * absl::Hours(24),
      mock_log_manager_, mock_flags_.opstats_db_size_limit_bytes());
  ASSERT_OK(db);
  auto data = (*db)->Read();
  ASSERT_OK(data);

  OpStatsSequence expected;
  auto expected_stats = expected.add_opstats();
  expected_stats->set_population_name(kPopulationName);
  expected_stats->set_session_name(kSessionName);
  OperationalStats::PhaseStats* computation_phase =
      expected_stats->add_phase_stats();
  computation_phase->set_phase(OperationalStats::PhaseStats::COMPUTATION);
  computation_phase->set_task_name(kTaskName);
  computation_phase->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED);
  auto* dataset_stats_map = computation_phase->mutable_dataset_stats();
  (*dataset_stats_map)[collection_url_1] = CreateDatasetStats(
      example_cnt_1 + example_cnt_3, example_size_1 + example_size_3);
  (*dataset_stats_map)[collection_url_2] =
      CreateDatasetStats(example_cnt_2, example_size_2);

  CheckEqualProtosAndIncreasingTimestamps(start_time, expected, *data);
}

TEST_F(OpStatsLoggerImplTest, PhaseStatsSetNetworkStats) {
  auto start_time = TimeUtil::GetCurrentTime();
  ExpectOpstatsEnabledEvents(/*num_opstats_loggers=*/1,
                             /*num_opstats_commits*/ 1);

  auto opstats_logger =
      CreateOpStatsLogger(base_dir_, &mock_flags_, &mock_log_manager_,
                          kSessionName, kPopulationName);
  opstats_logger->StartLoggingForPhase(
      OperationalStats::PhaseStats::ELIGIBILITY_EVAL_CHECKIN);
  const int64_t eet_checkin_bytes_downloaded = 15;
  const int64_t eet_checkin_bytes_uploaded = 5;
  const absl::Duration eet_checkin_duration = absl::Minutes(1);
  int64_t accumulated_bytes_downloaded = eet_checkin_bytes_downloaded;
  int64_t accumulated_bytes_uploaded = eet_checkin_bytes_uploaded;
  absl::Duration accumulated_network_duration = eet_checkin_duration;
  opstats_logger->SetNetworkStats(CreateNetworkStats(
      accumulated_bytes_downloaded, accumulated_bytes_uploaded,
      accumulated_network_duration));

  const int64_t eet_artifact_download_bytes_downloaded = 200;
  const int64_t eet_artifact_download_bytes_uploaded = 50;
  const absl::Duration eet_artifact_download_duration = absl::Minutes(2);
  accumulated_bytes_downloaded += eet_artifact_download_bytes_downloaded;
  accumulated_bytes_uploaded += eet_artifact_download_bytes_uploaded;
  accumulated_network_duration += eet_artifact_download_duration;
  opstats_logger->SetNetworkStats(CreateNetworkStats(
      accumulated_bytes_downloaded, accumulated_bytes_uploaded,
      accumulated_network_duration));
  opstats_logger->StopLoggingForTheCurrentPhase();

  opstats_logger->StartLoggingForPhase(OperationalStats::PhaseStats::CHECKIN);
  const int64_t checkin_bytes_downloaded = 1000;
  const int64_t checkin_bytes_uploaded = 80;
  const absl::Duration checkin_duration = absl::Minutes(5);
  accumulated_bytes_downloaded += checkin_bytes_downloaded;
  accumulated_bytes_uploaded += checkin_bytes_uploaded;
  accumulated_network_duration += checkin_duration;
  opstats_logger->SetNetworkStats(CreateNetworkStats(
      accumulated_bytes_downloaded, accumulated_bytes_uploaded,
      accumulated_network_duration));
  opstats_logger->StopLoggingForTheCurrentPhase();

  opstats_logger.reset();

  auto db = PdsBackedOpStatsDb::Create(
      base_dir_, mock_flags_.opstats_ttl_days() * absl::Hours(24),
      mock_log_manager_, mock_flags_.opstats_db_size_limit_bytes());
  ASSERT_OK(db);
  auto data = (*db)->Read();
  ASSERT_OK(data);

  OpStatsSequence expected;
  auto expected_stats = expected.add_opstats();
  expected_stats->set_population_name(kPopulationName);
  expected_stats->set_session_name(kSessionName);
  OperationalStats::PhaseStats* eet_checkin_phase =
      expected_stats->add_phase_stats();
  eet_checkin_phase->set_phase(
      OperationalStats::PhaseStats::ELIGIBILITY_EVAL_CHECKIN);
  eet_checkin_phase->set_bytes_downloaded(
      eet_checkin_bytes_downloaded + eet_artifact_download_bytes_downloaded);
  eet_checkin_phase->set_bytes_uploaded(eet_checkin_bytes_uploaded +
                                        eet_artifact_download_bytes_uploaded);
  *eet_checkin_phase->mutable_network_duration() =
      ::fcp::TimeUtil::ConvertAbslToProtoDuration(
          eet_checkin_duration + eet_artifact_download_duration);

  OperationalStats::PhaseStats* checkin_phase =
      expected_stats->add_phase_stats();

  checkin_phase->set_phase(OperationalStats::PhaseStats::CHECKIN);
  checkin_phase->set_bytes_downloaded(checkin_bytes_downloaded);
  checkin_phase->set_bytes_uploaded(checkin_bytes_uploaded);
  *checkin_phase->mutable_network_duration() =
      ::fcp::TimeUtil::ConvertAbslToProtoDuration(checkin_duration);

  CheckEqualProtosAndIncreasingTimestamps(start_time, expected, *data);
}

TEST_F(OpStatsLoggerImplTest, PhaseStatsCommitToStorage) {
  auto start_time = TimeUtil::GetCurrentTime();
  ExpectOpstatsEnabledEvents(/*num_opstats_loggers=*/1,
                             /*num_opstats_commits*/ 2);

  auto opstats_logger =
      CreateOpStatsLogger(base_dir_, &mock_flags_, &mock_log_manager_,
                          kSessionName, kPopulationName);
  opstats_logger->StartLoggingForPhase(
      OperationalStats::PhaseStats::COMPUTATION);
  opstats_logger->AddEventAndSetTaskName(
      kTaskName, OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED);
  opstats_logger->AddEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED);
  opstats_logger->StopLoggingForTheCurrentPhase();

  opstats_logger->StartLoggingForPhase(OperationalStats::PhaseStats::UPLOAD);
  opstats_logger->AddEvent(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_STARTED);
  ASSERT_OK(opstats_logger->CommitToStorage());
  opstats_logger->AddEvent(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_FINISHED);

  opstats_logger.reset();

  auto db = PdsBackedOpStatsDb::Create(
      base_dir_, mock_flags_.opstats_ttl_days() * absl::Hours(24),
      mock_log_manager_, mock_flags_.opstats_db_size_limit_bytes());
  ASSERT_OK(db);
  auto data = (*db)->Read();
  ASSERT_OK(data);

  OpStatsSequence expected;
  auto expected_stats = expected.add_opstats();
  expected_stats->set_population_name(kPopulationName);
  expected_stats->set_session_name(kSessionName);
  OperationalStats::PhaseStats* computation_phase =
      expected_stats->add_phase_stats();
  computation_phase->set_phase(OperationalStats::PhaseStats::COMPUTATION);
  computation_phase->set_task_name(kTaskName);
  computation_phase->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED);
  computation_phase->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED);
  OperationalStats::PhaseStats* upload_phase =
      expected_stats->add_phase_stats();
  upload_phase->set_phase(OperationalStats::PhaseStats::UPLOAD);
  upload_phase->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_STARTED);
  upload_phase->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_FINISHED);

  CheckEqualProtosAndIncreasingTimestamps(start_time, expected, *data);
}

TEST_F(OpStatsLoggerImplTest, PhaseStatsLogCommitLogCommitKeepsAllEntries) {
  auto start_time = TimeUtil::GetCurrentTime();
  ExpectOpstatsEnabledEvents(/*num_opstats_loggers=*/2,
                             /*num_opstats_commits*/ 4);

  auto opstats_logger =
      CreateOpStatsLogger(base_dir_, &mock_flags_, &mock_log_manager_,
                          kSessionName, kPopulationName);
  opstats_logger->StartLoggingForPhase(
      OperationalStats::PhaseStats::COMPUTATION);
  opstats_logger->AddEventAndSetTaskName(
      kTaskName, OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED);
  opstats_logger->AddEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED);
  opstats_logger->StopLoggingForTheCurrentPhase();

  opstats_logger->StartLoggingForPhase(OperationalStats::PhaseStats::UPLOAD);
  opstats_logger->AddEvent(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_STARTED);
  // FL runner always triggers a commit after upload started for hardened swor
  ASSERT_OK(opstats_logger->CommitToStorage());
  opstats_logger->AddEvent(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_FINISHED);
  opstats_logger->StopLoggingForTheCurrentPhase();
  opstats_logger.reset();

  // second run
  auto opstats_logger2 =
      CreateOpStatsLogger(base_dir_, &mock_flags_, &mock_log_manager_,
                          kSessionName, kPopulationName);
  opstats_logger2->StartLoggingForPhase(
      OperationalStats::PhaseStats::COMPUTATION);
  opstats_logger2->AddEventAndSetTaskName(
      kTaskName, OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED);
  opstats_logger2->AddEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED);
  opstats_logger2->StopLoggingForTheCurrentPhase();

  opstats_logger2->StartLoggingForPhase(OperationalStats::PhaseStats::UPLOAD);
  opstats_logger2->AddEvent(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_STARTED);
  // FL runner always triggers a commit after upload started for hardened swor
  ASSERT_OK(opstats_logger2->CommitToStorage());
  opstats_logger2->AddEvent(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_FINISHED);
  opstats_logger2.reset();

  auto db = PdsBackedOpStatsDb::Create(
      base_dir_, mock_flags_.opstats_ttl_days() * absl::Hours(24),
      mock_log_manager_, mock_flags_.opstats_db_size_limit_bytes());
  ASSERT_OK(db);
  auto data = (*db)->Read();
  ASSERT_OK(data);

  OpStatsSequence expected;
  auto expected_stats = expected.add_opstats();
  expected_stats->set_population_name(kPopulationName);
  expected_stats->set_session_name(kSessionName);
  OperationalStats::PhaseStats* computation_phase =
      expected_stats->add_phase_stats();
  computation_phase->set_phase(OperationalStats::PhaseStats::COMPUTATION);
  computation_phase->set_task_name(kTaskName);
  computation_phase->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED);
  computation_phase->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED);
  OperationalStats::PhaseStats* upload_phase =
      expected_stats->add_phase_stats();
  upload_phase->set_phase(OperationalStats::PhaseStats::UPLOAD);
  upload_phase->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_STARTED);
  upload_phase->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_FINISHED);

  auto second_run_stats = expected.add_opstats();
  second_run_stats->set_population_name(kPopulationName);
  second_run_stats->set_session_name(kSessionName);
  OperationalStats::PhaseStats* second_run_computation_phase =
      second_run_stats->add_phase_stats();
  second_run_computation_phase->set_phase(
      OperationalStats::PhaseStats::COMPUTATION);
  second_run_computation_phase->set_task_name(kTaskName);
  second_run_computation_phase->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED);
  second_run_computation_phase->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED);
  OperationalStats::PhaseStats* second_run_upload_phase =
      second_run_stats->add_phase_stats();
  second_run_upload_phase->set_phase(OperationalStats::PhaseStats::UPLOAD);
  second_run_upload_phase->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_STARTED);
  second_run_upload_phase->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_FINISHED);

  CheckEqualProtosAndIncreasingTimestamps(start_time, expected, *data);
}

TEST_F(OpStatsLoggerImplTest, PhaseStatsGetCurrentTaskName) {
  auto start_time = TimeUtil::GetCurrentTime();
  ExpectOpstatsEnabledEvents(/*num_opstats_loggers=*/1,
                             /*num_opstats_commits*/ 1);

  auto opstats_logger =
      CreateOpStatsLogger(base_dir_, &mock_flags_, &mock_log_manager_,
                          kSessionName, kPopulationName);
  opstats_logger->StartLoggingForPhase(
      OperationalStats::PhaseStats::COMPUTATION);
  opstats_logger->AddEventAndSetTaskName(
      kTaskName, OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED);
  opstats_logger->AddEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED);
  ASSERT_EQ(opstats_logger->GetCurrentTaskName(), kTaskName);
  opstats_logger->StopLoggingForTheCurrentPhase();
  ASSERT_EQ(opstats_logger->GetCurrentTaskName(), kTaskName);

  opstats_logger->StartLoggingForPhase(
      OperationalStats::PhaseStats::COMPUTATION);
  const std::string task_name_2 = "task_name_2";
  opstats_logger->AddEventAndSetTaskName(
      task_name_2, OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED);
  ASSERT_EQ(opstats_logger->GetCurrentTaskName(), task_name_2);
  opstats_logger->AddEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED);
  opstats_logger->StopLoggingForTheCurrentPhase();

  ASSERT_EQ(opstats_logger->GetCurrentTaskName(), task_name_2);
  opstats_logger.reset();
}

TEST_F(OpStatsLoggerImplTest, PhaseStatsStopCurrentPhaseLoggingNotCalled) {
  auto start_time = TimeUtil::GetCurrentTime();
  ExpectOpstatsEnabledEvents(/*num_opstats_loggers=*/1,
                             /*num_opstats_commits*/ 1);

  auto opstats_logger =
      CreateOpStatsLogger(base_dir_, &mock_flags_, &mock_log_manager_,
                          kSessionName, kPopulationName);
  opstats_logger->StartLoggingForPhase(
      OperationalStats::PhaseStats::COMPUTATION);
  opstats_logger->AddEventAndSetTaskName(
      kTaskName, OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED);

  // StopLoggingForCurrentPhase is not called, but it should still work.
  opstats_logger->StartLoggingForPhase(OperationalStats::PhaseStats::UPLOAD);
  opstats_logger->AddEvent(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_STARTED);

  opstats_logger.reset();

  auto db = PdsBackedOpStatsDb::Create(
      base_dir_, mock_flags_.opstats_ttl_days() * absl::Hours(24),
      mock_log_manager_, mock_flags_.opstats_db_size_limit_bytes());
  ASSERT_OK(db);
  auto data = (*db)->Read();
  ASSERT_OK(data);

  OpStatsSequence expected;
  auto expected_stats = expected.add_opstats();
  expected_stats->set_population_name(kPopulationName);
  expected_stats->set_session_name(kSessionName);
  OperationalStats::PhaseStats* computation_phase =
      expected_stats->add_phase_stats();
  computation_phase->set_phase(OperationalStats::PhaseStats::COMPUTATION);
  computation_phase->set_task_name(kTaskName);
  computation_phase->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED);
  OperationalStats::PhaseStats* upload_phase =
      expected_stats->add_phase_stats();
  upload_phase->set_phase(OperationalStats::PhaseStats::UPLOAD);
  upload_phase->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_STARTED);

  CheckEqualProtosAndIncreasingTimestamps(start_time, expected, *data);
}

TEST_F(OpStatsLoggerImplTest, PhaseStatsStopCurrentPhaseLoggingCalled) {
  auto start_time = TimeUtil::GetCurrentTime();
  ExpectOpstatsEnabledEvents(/*num_opstats_loggers=*/1,
                             /*num_opstats_commits*/ 1);

  auto opstats_logger =
      CreateOpStatsLogger(base_dir_, &mock_flags_, &mock_log_manager_,
                          kSessionName, kPopulationName);
  opstats_logger->StartLoggingForPhase(
      OperationalStats::PhaseStats::COMPUTATION);
  opstats_logger->AddEventAndSetTaskName(
      kTaskName, OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED);
  opstats_logger->StopLoggingForTheCurrentPhase();

  opstats_logger.reset();

  auto db = PdsBackedOpStatsDb::Create(
      base_dir_, mock_flags_.opstats_ttl_days() * absl::Hours(24),
      mock_log_manager_, mock_flags_.opstats_db_size_limit_bytes());
  ASSERT_OK(db);
  auto data = (*db)->Read();
  ASSERT_OK(data);

  OpStatsSequence expected;
  auto expected_stats = expected.add_opstats();
  expected_stats->set_population_name(kPopulationName);
  expected_stats->set_session_name(kSessionName);
  OperationalStats::PhaseStats* computation_phase =
      expected_stats->add_phase_stats();
  computation_phase->set_phase(OperationalStats::PhaseStats::COMPUTATION);
  computation_phase->set_task_name(kTaskName);
  computation_phase->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED);

  CheckEqualProtosAndIncreasingTimestamps(start_time, expected, *data);
}

TEST_F(OpStatsLoggerImplTest, PhaseStatsSetMinSepPolicyIndex) {
  EXPECT_CALL(mock_flags_, log_min_sep_index_to_phase_stats())
      .WillRepeatedly(Return(true));
  auto start_time = TimeUtil::GetCurrentTime();
  ExpectOpstatsEnabledEvents(/*num_opstats_loggers=*/1,
                             /*num_opstats_commits*/ 1);
  auto opstats_logger =
      CreateOpStatsLogger(base_dir_, &mock_flags_, &mock_log_manager_,
                          kSessionName, kPopulationName);
  opstats_logger->StartLoggingForPhase(OperationalStats::PhaseStats::CHECKIN);
  opstats_logger->AddEventAndSetTaskName(
      kTaskName, OperationalStats::Event::EVENT_KIND_CHECKIN_ACCEPTED);
  opstats_logger->SetMinSepPolicyIndex(1);
  opstats_logger->StopLoggingForTheCurrentPhase();

  ASSERT_EQ(opstats_logger->GetCurrentTaskName(), kTaskName);

  opstats_logger.reset();

  auto db = PdsBackedOpStatsDb::Create(
      base_dir_, mock_flags_.opstats_ttl_days() * absl::Hours(24),
      mock_log_manager_, mock_flags_.opstats_db_size_limit_bytes());
  ASSERT_OK(db);
  auto data = (*db)->Read();
  ASSERT_OK(data);

  OpStatsSequence expected;
  auto expected_stats = expected.add_opstats();
  expected_stats->set_population_name(kPopulationName);
  expected_stats->set_session_name(kSessionName);
  OperationalStats::PhaseStats* phase_stats = expected_stats->add_phase_stats();
  phase_stats->set_phase(OperationalStats::PhaseStats::CHECKIN);
  phase_stats->set_task_name(kTaskName);
  phase_stats->add_events()->set_event_type(
      OperationalStats::Event::EVENT_KIND_CHECKIN_ACCEPTED);
  phase_stats->set_min_sep_policy_index(1);

  CheckEqualProtosAndIncreasingTimestamps(start_time, expected, *data);
}

class CreateOpstatsTest : public testing::Test {
 protected:
  void SetUp() override {
    EXPECT_CALL(mock_flags_, opstats_ttl_days()).WillRepeatedly(Return(1));
    EXPECT_CALL(mock_flags_, opstats_db_size_limit_bytes())
        .WillRepeatedly(Return(1024));
    base_dir_ = testing::TempDir();
  }

  void TearDown() override {
    auto db = PdsBackedOpStatsDb::Create(
                  base_dir_, mock_flags_.opstats_ttl_days() * absl::Hours(24),
                  mock_log_manager_, mock_flags_.opstats_db_size_limit_bytes())
                  .value();
    EXPECT_THAT(db->Transform([](OpStatsSequence& data) { data.Clear(); }),
                IsOk());
  }

  std::string base_dir_;
  StrictMock<MockFlags> mock_flags_;
  testing::NiceMock<MockLogManager> mock_log_manager_;
};

TEST_F(CreateOpstatsTest, CreateOpStatsLoggerOpStatsEnabledDbFails) {
  std::string bad_base_dir = "/proc/0";

  auto opstats_logger =
      CreateOpStatsLogger(bad_base_dir, &mock_flags_, &mock_log_manager_,
                          kSessionName, kPopulationName);
  opstats_logger->SetNetworkStats(
      {.bytes_downloaded = 100, .bytes_uploaded = 101});

  // The database should initially be empty.
  auto data = opstats_logger->GetOpStatsDb()->Read();
  ASSERT_OK(data);
  EXPECT_THAT(data.value(), EqualsProto(OpStatsSequence()));
  opstats_logger.reset();

  // A second logger backed by the same database should not be able to produce
  // any info from the first run.
  EXPECT_CALL(mock_log_manager_,
              LogDiag(ProdDiagCode::OPSTATS_PARENT_DIR_CREATION_FAILED));
  auto opstats_logger_again =
      CreateOpStatsLogger(bad_base_dir, &mock_flags_, &mock_log_manager_,
                          kSessionName, kPopulationName);
  data = opstats_logger_again->GetOpStatsDb()->Read();
  ASSERT_OK(data);
  EXPECT_THAT(data.value(), EqualsProto(OpStatsSequence()));
}

}  // anonymous namespace
}  // namespace opstats
}  // namespace client
}  // namespace fcp
