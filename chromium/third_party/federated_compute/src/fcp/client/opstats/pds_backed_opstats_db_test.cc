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

#include "fcp/client/opstats/pds_backed_opstats_db.h"

#include <cstdint>
#include <filesystem>  // NOLINT(build/c++17)
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <thread>  // NOLINT(build/c++11)
#include <utility>
#include <vector>

#include "google/protobuf/util/time_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/engine/engine.pb.h"
#include "fcp/client/histogram_counters.pb.h"
#include "fcp/client/opstats/opstats_db.h"
#include "fcp/client/test_helpers.h"
#include "fcp/protos/opstats.pb.h"
#include "fcp/testing/testing.h"
#include "protostore/file-storage.h"

namespace fcp {
namespace client {
namespace opstats {
namespace {

using ::google::protobuf::util::TimeUtil;
using ::testing::Ge;
using ::testing::Gt;

const absl::Duration ttl = absl::Hours(24);
const absl::Time benchmark_time = absl::Now();
const int64_t benchmark_time_sec = absl::ToUnixSeconds(absl::Now());
const int64_t size_limit = 1 * 1024 * 1024;

class BasePdsBackedOpStatsDbTest {
 protected:
  void SetUpBaseDir() { base_dir_ = testing::TempDir(); }

  void TearDownBaseDir() {
    std::filesystem::remove(std::filesystem::path(base_dir_) /
                            PdsBackedOpStatsDb::kParentDir /
                            PdsBackedOpStatsDb::kDbFileName);
  }

  static OperationalStats_Event CreateEvent(
      OperationalStats::Event::EventKind kind, int64_t time_sec) {
    OperationalStats_Event event;
    event.set_event_type(kind);
    *event.mutable_timestamp() = TimeUtil::SecondsToTimestamp(time_sec);
    return event;
  }

  static OperationalStats CreateOperationalStatsWithSingleEvent(
      OperationalStats::Event::EventKind kind, int64_t time_sec) {
    OperationalStats op_stats;
    op_stats.mutable_events()->Add(CreateEvent(kind, time_sec));
    return op_stats;
  }

  static OperationalStats CreateOperationalStatsWithSingleEventInPhaseStats(
      OperationalStats::Event::EventKind kind, int64_t time_sec) {
    OperationalStats op_stats;
    auto phase_stats = op_stats.add_phase_stats();
    phase_stats->mutable_events()->Add(CreateEvent(kind, time_sec));
    return op_stats;
  }

  std::string base_dir_;
  testing::StrictMock<MockLogManager> log_manager_;
  absl::Mutex mu_;
};

class PdsBackedOpStatsDbTest : public BasePdsBackedOpStatsDbTest,
                               public testing::Test {
  void SetUp() override { SetUpBaseDir(); }

  void TearDown() override { TearDownBaseDir(); }
};

TEST_F(PdsBackedOpStatsDbTest, FailToCreateParentDirectory) {
  EXPECT_CALL(log_manager_,
              LogDiag(ProdDiagCode::OPSTATS_PARENT_DIR_CREATION_FAILED));
  ASSERT_THAT(
      PdsBackedOpStatsDb::Create("/proc/0", ttl, log_manager_, size_limit),
      IsCode(INTERNAL));
}

TEST_F(PdsBackedOpStatsDbTest, InvalidRelativePath) {
  EXPECT_CALL(log_manager_, LogDiag(ProdDiagCode::OPSTATS_INVALID_FILE_PATH));
  ASSERT_THAT(PdsBackedOpStatsDb::Create("relative/opstats", ttl, log_manager_,
                                         size_limit),
              IsCode(INVALID_ARGUMENT));
}

TEST_F(PdsBackedOpStatsDbTest, AddOpStats) {
  auto db =
      PdsBackedOpStatsDb::Create(base_dir_, ttl, log_manager_, size_limit);
  ASSERT_THAT(db, IsOk());
  OperationalStats op_stats = CreateOperationalStatsWithSingleEvent(
      OperationalStats::Event::EVENT_KIND_CHECKIN_STARTED, benchmark_time_sec);
  auto func = [op_stats](OpStatsSequence& data) {
    *data.add_opstats() = op_stats;
  };
  EXPECT_CALL(
      log_manager_,
      LogToLongHistogram(HistogramCounters::OPSTATS_DB_SIZE_BYTES,
                         /*execution_index=*/0, /*epoch_index=*/0,
                         engine::DataSourceType::DATASET, /*value=*/Gt(0)));
  EXPECT_CALL(log_manager_,
              LogToLongHistogram(HistogramCounters::OPSTATS_DB_NUM_ENTRIES,
                                 /*execution_index=*/0, /*epoch_index=*/0,
                                 engine::DataSourceType::DATASET, /*value=*/1));
  ASSERT_OK((*db)->Transform(func));
  OpStatsSequence expected;
  *expected.add_opstats() = op_stats;
  absl::StatusOr<OpStatsSequence> data = (*db)->Read();
  ASSERT_THAT(data, IsOk());
  ASSERT_TRUE(data->has_earliest_trustworthy_time());
  data->clear_earliest_trustworthy_time();
  EXPECT_THAT(*data, EqualsProto(expected));
}

TEST_F(PdsBackedOpStatsDbTest, MutateOpStats) {
  auto db =
      PdsBackedOpStatsDb::Create(base_dir_, ttl, log_manager_, size_limit);
  ASSERT_THAT(db, IsOk());
  auto initialCommit = [](OpStatsSequence& data) {
    *data.add_opstats() = CreateOperationalStatsWithSingleEvent(
        OperationalStats::Event::EVENT_KIND_CHECKIN_STARTED,
        benchmark_time_sec);
  };
  EXPECT_CALL(
      log_manager_,
      LogToLongHistogram(HistogramCounters::OPSTATS_DB_SIZE_BYTES,
                         /*execution_index=*/0, /*epoch_index=*/0,
                         engine::DataSourceType::DATASET, /*value=*/Gt(0)))
      .Times(2);
  EXPECT_CALL(log_manager_,
              LogToLongHistogram(HistogramCounters::OPSTATS_DB_NUM_ENTRIES,
                                 /*execution_index=*/0, /*epoch_index=*/0,
                                 engine::DataSourceType::DATASET, /*value=*/1))
      .Times(2);
  ASSERT_OK((*db)->Transform(initialCommit));
  auto mutate = [](OpStatsSequence& data) {
    data.mutable_opstats(0)->mutable_events()->Add(
        CreateEvent(OperationalStats::Event::EVENT_KIND_CHECKIN_ACCEPTED,
                    benchmark_time_sec));
  };
  ASSERT_OK((*db)->Transform(mutate));
  OperationalStats expected_op_stats;
  expected_op_stats.mutable_events()->Add(CreateEvent(
      OperationalStats::Event::EVENT_KIND_CHECKIN_STARTED, benchmark_time_sec));
  expected_op_stats.mutable_events()->Add(
      CreateEvent(OperationalStats::Event::EVENT_KIND_CHECKIN_ACCEPTED,
                  benchmark_time_sec));
  OpStatsSequence expected;
  *expected.add_opstats() = expected_op_stats;
  absl::StatusOr<OpStatsSequence> data = (*db)->Read();
  ASSERT_THAT(data, IsOk());
  ASSERT_TRUE(data->has_earliest_trustworthy_time());
  data->clear_earliest_trustworthy_time();
  EXPECT_THAT(*data, EqualsProto(expected));
}

TEST_F(PdsBackedOpStatsDbTest, LastUpdateTimeIsCorrectlyUsed) {
  auto db =
      PdsBackedOpStatsDb::Create(base_dir_, ttl, log_manager_, size_limit);
  ASSERT_THAT(db, IsOk());
  OperationalStats op_stats;
  op_stats.mutable_events()->Add(
      CreateEvent(OperationalStats::Event::EVENT_KIND_CHECKIN_STARTED,
                  absl::ToUnixSeconds(benchmark_time - absl::Hours(48))));
  op_stats.mutable_events()->Add(
      CreateEvent(OperationalStats::Event::EVENT_KIND_CHECKIN_ACCEPTED,
                  absl::ToUnixSeconds(benchmark_time - absl::Hours(12))));
  auto initialCommit = [op_stats](OpStatsSequence& data) {
    *data.add_opstats() = op_stats;
  };
  EXPECT_CALL(
      log_manager_,
      LogToLongHistogram(HistogramCounters::OPSTATS_DB_SIZE_BYTES,
                         /*execution_index=*/0, /*epoch_index=*/0,
                         engine::DataSourceType::DATASET, /*value=*/Gt(0)))
      .Times(2);
  EXPECT_CALL(log_manager_,
              LogToLongHistogram(HistogramCounters::OPSTATS_DB_NUM_ENTRIES,
                                 /*execution_index=*/0, /*epoch_index=*/0,
                                 engine::DataSourceType::DATASET, /*value=*/1))
      .Times(2);
  ASSERT_OK((*db)->Transform(initialCommit));

  // We do a second unity commit to trigger the ttl cleanup.
  auto unityCommit = [](OpStatsSequence& data) {};
  ASSERT_OK((*db)->Transform(unityCommit));
  OpStatsSequence expected;
  *expected.add_opstats() = op_stats;
  absl::StatusOr<OpStatsSequence> data = (*db)->Read();
  ASSERT_THAT(data, IsOk());
  ASSERT_TRUE(data->has_earliest_trustworthy_time());
  data->clear_earliest_trustworthy_time();
  EXPECT_THAT(*data, EqualsProto(expected));
}

TEST_F(PdsBackedOpStatsDbTest, NoEventsOpStatsGotRemoved) {
  auto db =
      PdsBackedOpStatsDb::Create(base_dir_, ttl, log_manager_, size_limit);
  ASSERT_THAT(db, IsOk());
  OperationalStats op_stats;
  op_stats.set_population_name("population");
  auto initialCommit = [op_stats](OpStatsSequence& data) {
    *data.add_opstats() = op_stats;
  };
  EXPECT_CALL(
      log_manager_,
      LogToLongHistogram(HistogramCounters::OPSTATS_DB_SIZE_BYTES,
                         /*execution_index=*/0, /*epoch_index=*/0,
                         engine::DataSourceType::DATASET, /*value=*/Ge(0)))
      .Times(2);
  EXPECT_CALL(log_manager_,
              LogToLongHistogram(HistogramCounters::OPSTATS_DB_NUM_ENTRIES,
                                 /*execution_index=*/0, /*epoch_index=*/0,
                                 engine::DataSourceType::DATASET, /*value=*/1));
  EXPECT_CALL(log_manager_,
              LogToLongHistogram(HistogramCounters::OPSTATS_DB_NUM_ENTRIES,
                                 /*execution_index=*/0, /*epoch_index=*/0,
                                 engine::DataSourceType::DATASET, /*value=*/0));
  ASSERT_OK((*db)->Transform(initialCommit));

  // We do a second unity commit to trigger the ttl cleanup.
  auto unityCommit = [](OpStatsSequence& data) {};
  ASSERT_OK((*db)->Transform(unityCommit));
  absl::StatusOr<OpStatsSequence> data = (*db)->Read();
  ASSERT_THAT(data, IsOk());
  ASSERT_TRUE(data->has_earliest_trustworthy_time());
  data->clear_earliest_trustworthy_time();
  EXPECT_THAT(*data, EqualsProto(OpStatsSequence::default_instance()));
}

TEST_F(PdsBackedOpStatsDbTest, TwoInstanceOnTwoThreadsAccessSameFile) {
  EXPECT_CALL(log_manager_,
              LogDiag(ProdDiagCode::OPSTATS_MULTIPLE_DB_INSTANCE_DETECTED));
  std::vector<absl::StatusOr<std::unique_ptr<OpStatsDb>>> results;
  std::function<void()> init = [&]() {
    absl::WriterMutexLock lock(&mu_);
    results.push_back(
        PdsBackedOpStatsDb::Create(base_dir_, ttl, log_manager_, size_limit));
  };
  std::thread first_thread(init);
  std::thread second_thread(init);
  first_thread.join();
  second_thread.join();
  std::set<absl::StatusCode> expected{absl::StatusCode::kOk,
                                      absl::StatusCode::kInternal};
  std::set<absl::StatusCode> status_codes;
  for (const auto& result : results) {
    status_codes.insert(result.status().code());
  }
  ASSERT_EQ(status_codes, expected);
}

TEST_F(PdsBackedOpStatsDbTest, TwoInstanceOnTwoThreadsAccessDifferentFile) {
  std::vector<absl::StatusOr<std::unique_ptr<OpStatsDb>>> results;
  std::function<void(std::string)> init = [&](std::string thread_id) {
    absl::WriterMutexLock lock(&mu_);
    results.push_back(
        PdsBackedOpStatsDb::Create(absl::StrCat(base_dir_, "/", thread_id), ttl,
                                   log_manager_, size_limit));
  };
  std::thread first_thread(init, "1");
  std::thread second_thread(init, "2");
  first_thread.join();
  second_thread.join();
  for (const auto& result : results) {
    ASSERT_OK(result.status());
  }
}

TEST_F(PdsBackedOpStatsDbTest, BackfillEarliestTrustWorthyTime) {
  OperationalStats first_op_stats = CreateOperationalStatsWithSingleEvent(
      OperationalStats::Event::EVENT_KIND_TRAIN_NOT_STARTED,
      benchmark_time_sec);
  OperationalStats second_op_stats = CreateOperationalStatsWithSingleEvent(
      OperationalStats::Event::EVENT_KIND_TRAIN_NOT_STARTED,
      benchmark_time_sec);
  {
    absl::StatusOr<std::unique_ptr<OpStatsDb>> db =
        PdsBackedOpStatsDb::Create(base_dir_, ttl, log_manager_, size_limit);
    ASSERT_OK(db);
    auto add = [first_op_stats, second_op_stats](OpStatsSequence& data) {
      *data.add_opstats() = first_op_stats;
      *data.add_opstats() = second_op_stats;
    };
    auto remove_earliest_trustworthy_time = [](OpStatsSequence& data) {
      data.clear_earliest_trustworthy_time();
    };
    EXPECT_CALL(
        log_manager_,
        LogToLongHistogram(HistogramCounters::OPSTATS_DB_SIZE_BYTES,
                           /*execution_index=*/0, /*epoch_index=*/0,
                           engine::DataSourceType::DATASET, /*value=*/Gt(0)))
        .Times(2);
    EXPECT_CALL(log_manager_, LogToLongHistogram(
                                  HistogramCounters::OPSTATS_DB_NUM_ENTRIES,
                                  /*execution_index=*/0, /*epoch_index=*/0,
                                  engine::DataSourceType::DATASET, /*value=*/2))
        .Times(2);
    ASSERT_OK((*db)->Transform(add));
    ASSERT_OK((*db)->Transform(remove_earliest_trustworthy_time));
  }

  absl::StatusOr<std::unique_ptr<OpStatsDb>> db =
      PdsBackedOpStatsDb::Create(base_dir_, ttl, log_manager_, size_limit);
  ASSERT_OK(db);
  OperationalStats third_op_stats = CreateOperationalStatsWithSingleEvent(
      OperationalStats::Event::EVENT_KIND_CHECKIN_STARTED,
      benchmark_time_sec + 10);
  auto add_another = [third_op_stats](OpStatsSequence& data) {
    *data.add_opstats() = third_op_stats;
  };
  EXPECT_CALL(
      log_manager_,
      LogToLongHistogram(HistogramCounters::OPSTATS_DB_SIZE_BYTES,
                         /*execution_index=*/0, /*epoch_index=*/0,
                         engine::DataSourceType::DATASET, /*value=*/Gt(0)));
  EXPECT_CALL(log_manager_,
              LogToLongHistogram(HistogramCounters::OPSTATS_DB_NUM_ENTRIES,
                                 /*execution_index=*/0, /*epoch_index=*/0,
                                 engine::DataSourceType::DATASET, /*value=*/3));
  ASSERT_OK((*db)->Transform(add_another));
  absl::StatusOr<OpStatsSequence> data = (*db)->Read();
  ASSERT_OK(data);
  OpStatsSequence expected;
  *expected.mutable_earliest_trustworthy_time() =
      TimeUtil::SecondsToTimestamp(benchmark_time_sec);
  *expected.add_opstats() = first_op_stats;
  *expected.add_opstats() = second_op_stats;
  *expected.add_opstats() = third_op_stats;
  EXPECT_THAT((*data), EqualsProto(expected));
}

TEST_F(PdsBackedOpStatsDbTest, ReadEmpty) {
  ::google::protobuf::Timestamp before_creation_time =
      TimeUtil::GetCurrentTime();
  auto db =
      PdsBackedOpStatsDb::Create(base_dir_, ttl, log_manager_, size_limit);
  ::google::protobuf::Timestamp after_creation_time =
      TimeUtil::GetCurrentTime();
  ASSERT_THAT(db, IsOk());
  absl::StatusOr<OpStatsSequence> data = (*db)->Read();
  ASSERT_THAT(data, IsOk());
  EXPECT_TRUE(data->opstats().empty());
  EXPECT_TRUE(data->earliest_trustworthy_time() >= before_creation_time);
  EXPECT_TRUE(data->earliest_trustworthy_time() <= after_creation_time);
}

TEST_F(PdsBackedOpStatsDbTest, RemoveOpstatsDueToTtl) {
  auto db =
      PdsBackedOpStatsDb::Create(base_dir_, ttl, log_manager_, size_limit);
  ASSERT_THAT(db, IsOk());
  OperationalStats op_stats_remove = CreateOperationalStatsWithSingleEvent(
      OperationalStats::Event::EVENT_KIND_CHECKIN_STARTED,
      absl::ToUnixSeconds(benchmark_time - absl::Hours(25)));
  OperationalStats op_stats_keep = CreateOperationalStatsWithSingleEvent(
      OperationalStats::Event::EVENT_KIND_CHECKIN_STARTED,
      absl::ToUnixSeconds(benchmark_time - absl::Hours(23)));
  auto initialCommit = [op_stats_remove, op_stats_keep](OpStatsSequence& data) {
    *data.add_opstats() = op_stats_remove;
    *data.add_opstats() = op_stats_keep;
  };
  EXPECT_CALL(
      log_manager_,
      LogToLongHistogram(HistogramCounters::OPSTATS_DB_SIZE_BYTES,
                         /*execution_index=*/0, /*epoch_index=*/0,
                         engine::DataSourceType::DATASET, /*value=*/Gt(0)))
      .Times(2);
  EXPECT_CALL(log_manager_,
              LogToLongHistogram(HistogramCounters::OPSTATS_DB_NUM_ENTRIES,
                                 /*execution_index=*/0, /*epoch_index=*/0,
                                 engine::DataSourceType::DATASET, /*value=*/2));
  EXPECT_CALL(log_manager_,
              LogToLongHistogram(HistogramCounters::OPSTATS_DB_NUM_ENTRIES,
                                 /*execution_index=*/0, /*epoch_index=*/0,
                                 engine::DataSourceType::DATASET, /*value=*/1));
  ASSERT_OK((*db)->Transform(initialCommit));

  // We do a second unity commit to trigger the ttl cleanup.
  auto unityCommit = [](OpStatsSequence& data) {};
  ASSERT_OK((*db)->Transform(unityCommit));

  absl::StatusOr<OpStatsSequence> data = (*db)->Read();
  ASSERT_THAT(data, IsOk());
  ASSERT_EQ(data->opstats().size(), 1);
  ASSERT_THAT(data->opstats()[0], EqualsProto(op_stats_keep));
  // The TTL is 24 hours, the timestamp should be set to the time when the db
  // got purged - 24 hours.  It should be smaller than the kept
  // OperationalStats, but larger than benchmark time - 24 hours.
  google::protobuf::Timestamp lower_bound = TimeUtil::SecondsToTimestamp(
      absl::ToUnixSeconds(benchmark_time - absl::Hours(24)));
  google::protobuf::Timestamp upper_bound = TimeUtil::SecondsToTimestamp(
      absl::ToUnixSeconds(benchmark_time - absl::Hours(23)));
  EXPECT_TRUE(data->earliest_trustworthy_time() >= lower_bound);
  EXPECT_TRUE(data->earliest_trustworthy_time() <= upper_bound);
}

TEST_F(PdsBackedOpStatsDbTest, RemoveOpstatsDueToTtlPhaseStats) {
  auto db =
      PdsBackedOpStatsDb::Create(base_dir_, ttl, log_manager_, size_limit);
  ASSERT_THAT(db, IsOk());
  OperationalStats op_stats_remove =
      CreateOperationalStatsWithSingleEventInPhaseStats(
          OperationalStats::Event::EVENT_KIND_CHECKIN_STARTED,
          absl::ToUnixSeconds(benchmark_time - absl::Hours(25)));
  OperationalStats op_stats_keep =
      CreateOperationalStatsWithSingleEventInPhaseStats(
          OperationalStats::Event::EVENT_KIND_CHECKIN_STARTED,
          absl::ToUnixSeconds(benchmark_time - absl::Hours(23)));
  auto initialCommit = [op_stats_remove, op_stats_keep](OpStatsSequence& data) {
    *data.add_opstats() = op_stats_remove;
    *data.add_opstats() = op_stats_keep;
  };
  EXPECT_CALL(
      log_manager_,
      LogToLongHistogram(HistogramCounters::OPSTATS_DB_SIZE_BYTES,
                         /*execution_index=*/0, /*epoch_index=*/0,
                         engine::DataSourceType::DATASET, /*value=*/Gt(0)))
      .Times(2);
  EXPECT_CALL(log_manager_,
              LogToLongHistogram(HistogramCounters::OPSTATS_DB_NUM_ENTRIES,
                                 /*execution_index=*/0, /*epoch_index=*/0,
                                 engine::DataSourceType::DATASET, /*value=*/2));
  EXPECT_CALL(log_manager_,
              LogToLongHistogram(HistogramCounters::OPSTATS_DB_NUM_ENTRIES,
                                 /*execution_index=*/0, /*epoch_index=*/0,
                                 engine::DataSourceType::DATASET, /*value=*/1));
  ASSERT_OK((*db)->Transform(initialCommit));

  // We do a second unity commit to trigger the ttl cleanup.
  auto unityCommit = [](OpStatsSequence& data) {};
  ASSERT_OK((*db)->Transform(unityCommit));

  absl::StatusOr<OpStatsSequence> data = (*db)->Read();
  ASSERT_THAT(data, IsOk());
  ASSERT_EQ(data->opstats().size(), 1);
  ASSERT_THAT(data->opstats()[0], EqualsProto(op_stats_keep));
  // The TTL is 24 hours, the timestamp should be set to the time when the db
  // got purged - 24 hours.  It should be smaller than the kept
  // OperationalStats, but larger than benchmark time - 24 hours.
  google::protobuf::Timestamp lower_bound = TimeUtil::SecondsToTimestamp(
      absl::ToUnixSeconds(benchmark_time - absl::Hours(24)));
  google::protobuf::Timestamp upper_bound = TimeUtil::SecondsToTimestamp(
      absl::ToUnixSeconds(benchmark_time - absl::Hours(23)));
  EXPECT_TRUE(data->earliest_trustworthy_time() >= lower_bound);
  EXPECT_TRUE(data->earliest_trustworthy_time() <= upper_bound);
}

TEST_F(PdsBackedOpStatsDbTest, RemoveOpstatsDueToTtlLegacyAndPhaseStats) {
  auto db =
      PdsBackedOpStatsDb::Create(base_dir_, ttl, log_manager_, size_limit);
  ASSERT_THAT(db, IsOk());
  OperationalStats op_stats_remove = CreateOperationalStatsWithSingleEvent(
      OperationalStats::Event::EVENT_KIND_CHECKIN_STARTED,
      absl::ToUnixSeconds(benchmark_time - absl::Hours(25)));
  OperationalStats op_stats_keep =
      CreateOperationalStatsWithSingleEventInPhaseStats(
          OperationalStats::Event::EVENT_KIND_CHECKIN_STARTED,
          absl::ToUnixSeconds(benchmark_time - absl::Hours(23)));
  auto initialCommit = [op_stats_remove, op_stats_keep](OpStatsSequence& data) {
    *data.add_opstats() = op_stats_remove;
    *data.add_opstats() = op_stats_keep;
  };
  EXPECT_CALL(
      log_manager_,
      LogToLongHistogram(HistogramCounters::OPSTATS_DB_SIZE_BYTES,
                         /*execution_index=*/0, /*epoch_index=*/0,
                         engine::DataSourceType::DATASET, /*value=*/Gt(0)))
      .Times(2);
  EXPECT_CALL(log_manager_,
              LogToLongHistogram(HistogramCounters::OPSTATS_DB_NUM_ENTRIES,
                                 /*execution_index=*/0, /*epoch_index=*/0,
                                 engine::DataSourceType::DATASET, /*value=*/2));
  EXPECT_CALL(log_manager_,
              LogToLongHistogram(HistogramCounters::OPSTATS_DB_NUM_ENTRIES,
                                 /*execution_index=*/0, /*epoch_index=*/0,
                                 engine::DataSourceType::DATASET, /*value=*/1));
  ASSERT_OK((*db)->Transform(initialCommit));

  // We do a second unity commit to trigger the ttl cleanup.
  auto unityCommit = [](OpStatsSequence& data) {};
  ASSERT_OK((*db)->Transform(unityCommit));

  absl::StatusOr<OpStatsSequence> data = (*db)->Read();
  ASSERT_THAT(data, IsOk());
  ASSERT_EQ(data->opstats().size(), 1);
  ASSERT_THAT(data->opstats()[0], EqualsProto(op_stats_keep));
  // The TTL is 24 hours, the timestamp should be set to the time when the db
  // got purged - 24 hours.  It should be smaller than the kept
  // OperationalStats, but larger than benchmark time - 24 hours.
  google::protobuf::Timestamp lower_bound = TimeUtil::SecondsToTimestamp(
      absl::ToUnixSeconds(benchmark_time - absl::Hours(24)));
  google::protobuf::Timestamp upper_bound = TimeUtil::SecondsToTimestamp(
      absl::ToUnixSeconds(benchmark_time - absl::Hours(23)));
  EXPECT_TRUE(data->earliest_trustworthy_time() >= lower_bound);
  EXPECT_TRUE(data->earliest_trustworthy_time() <= upper_bound);
}

TEST_F(PdsBackedOpStatsDbTest, CorruptedFile) {
  {
    std::unique_ptr<OpStatsDb> db =
        PdsBackedOpStatsDb::Create(base_dir_, ttl, log_manager_, size_limit)
            .value();
    auto func = [](OpStatsSequence& data) {
      *data.add_opstats() = CreateOperationalStatsWithSingleEvent(
          OperationalStats::Event::EVENT_KIND_CHECKIN_STARTED,
          benchmark_time_sec);
    };
    EXPECT_CALL(
        log_manager_,
        LogToLongHistogram(HistogramCounters::OPSTATS_DB_SIZE_BYTES,
                           /*execution_index=*/0, /*epoch_index=*/0,
                           engine::DataSourceType::DATASET, /*value=*/Gt(0)));
    EXPECT_CALL(
        log_manager_,
        LogToLongHistogram(HistogramCounters::OPSTATS_DB_NUM_ENTRIES,
                           /*execution_index=*/0, /*epoch_index=*/0,
                           engine::DataSourceType::DATASET, /*value=*/1));
    ASSERT_OK(db->Transform(func));
  }

  {
    std::filesystem::path db_path(base_dir_);
    db_path /= PdsBackedOpStatsDb::kParentDir;
    db_path /= PdsBackedOpStatsDb::kDbFileName;
    protostore::FileStorage file_storage;
    std::unique_ptr<protostore::OutputStream> ostream =
        file_storage.OpenForWrite(db_path).value();
    ASSERT_OK(ostream->Append("not a proto"));
    ASSERT_OK(ostream->Close());
  }

  std::unique_ptr<OpStatsDb> db =
      PdsBackedOpStatsDb::Create(base_dir_, ttl, log_manager_, size_limit)
          .value();
  EXPECT_CALL(log_manager_, LogDiag(ProdDiagCode::OPSTATS_READ_FAILED));
  ::google::protobuf::Timestamp before_read_time = TimeUtil::GetCurrentTime();
  ASSERT_THAT(db->Read(), IsCode(INTERNAL));
  ::google::protobuf::Timestamp after_read_time = TimeUtil::GetCurrentTime();

  // Second read should succeed, and return empty data.
  absl::StatusOr<OpStatsSequence> data = db->Read();
  ASSERT_THAT(data, IsOk());
  EXPECT_TRUE(data->opstats().empty());
  EXPECT_TRUE(data->earliest_trustworthy_time() >= before_read_time);
  EXPECT_TRUE(data->earliest_trustworthy_time() <= after_read_time);
}

TEST_F(PdsBackedOpStatsDbTest, OpStatsRemovedDueToSizeLimit) {
  // Set size limit to 18, which allow a single OperationalStats with a single
  // event (12 bytes for OperationalStats, 14 bytes when it is wrapped inside
  // an OpStatsSequence). If record_earliest_trustworthy_time is true, we'll
  // increase the size limit to 30 bytes to accommodate the timestamp.
  int64_t max_size_bytes = 30;
  absl::StatusOr<std::unique_ptr<OpStatsDb>> db_status =
      PdsBackedOpStatsDb::Create(base_dir_, ttl, log_manager_, max_size_bytes);
  ASSERT_THAT(db_status, IsOk());
  std::unique_ptr<OpStatsDb> db = std::move(db_status.value());
  EXPECT_CALL(
      log_manager_,
      LogToLongHistogram(HistogramCounters::OPSTATS_DB_SIZE_BYTES,
                         /*execution_index=*/0, /*epoch_index=*/0,
                         engine::DataSourceType::DATASET, /*value=*/Gt(0)))
      .Times(2);
  EXPECT_CALL(log_manager_,
              LogToLongHistogram(HistogramCounters::OPSTATS_DB_NUM_ENTRIES,
                                 /*execution_index=*/0, /*epoch_index=*/0,
                                 engine::DataSourceType::DATASET, /*value=*/1))
      .Times(2);
  OperationalStats op_stats = CreateOperationalStatsWithSingleEvent(
      OperationalStats::Event::EVENT_KIND_CHECKIN_STARTED, benchmark_time_sec);
  auto initial_commit = [op_stats](OpStatsSequence& data) {
    *data.add_opstats() = op_stats;
  };
  ASSERT_OK(db->Transform(initial_commit));

  // Add the second event, which will pushes the database size over the limit.
  EXPECT_CALL(log_manager_,
              LogToLongHistogram(HistogramCounters::OPSTATS_NUM_PRUNED_ENTRIES,
                                 /*execution_index=*/0, /*epoch_index=*/0,
                                 engine::DataSourceType::DATASET, /*value=*/1));
  EXPECT_CALL(log_manager_,
              LogToLongHistogram(
                  HistogramCounters::OPSTATS_OLDEST_PRUNED_ENTRY_TENURE_HOURS,
                  /*execution_index=*/0, /*epoch_index=*/0,
                  engine::DataSourceType::DATASET, /*value=*/Ge(0)));
  OperationalStats another_op_stats = CreateOperationalStatsWithSingleEvent(
      OperationalStats::Event::EVENT_KIND_CHECKIN_STARTED,
      benchmark_time_sec + 5);
  auto add = [another_op_stats](OpStatsSequence& data) {
    *data.add_opstats() = another_op_stats;
  };
  ASSERT_OK(db->Transform(add));

  // Verify the first event doesn't exist in the database.
  OpStatsSequence expected;
  *expected.add_opstats() = another_op_stats;
  *expected.mutable_earliest_trustworthy_time() =
      TimeUtil::SecondsToTimestamp(benchmark_time_sec + 5);

  absl::StatusOr<OpStatsSequence> data = db->Read();
  ASSERT_THAT(data, IsOk());
  EXPECT_THAT(*data, EqualsProto(expected));
}

}  // anonymous namespace
}  // namespace opstats
}  // namespace client
}  // namespace fcp
