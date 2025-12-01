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

#include "fcp/client/opstats/opstats_utils.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "google/protobuf/timestamp.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/time/time.h"
#include "fcp/protos/opstats.pb.h"
#include "fcp/testing/testing.h"
#include "re2/re2.h"

namespace fcp {
namespace client {
namespace opstats {
namespace {

using ::fcp::EqualsProto;
using ::testing::ElementsAre;

constexpr char kTaskName[] = "task";
constexpr char kCollectionUri[] = "collection_uri";
OperationalStats::Event::EventKind kComputationFinishedEvent =
    OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED;
OperationalStats::Event::EventKind kUploadStartedEvent =
    OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_STARTED;
OperationalStats::Event::EventKind kUploadServerAbortedEvent =
    OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_SERVER_ABORTED;

OperationalStats::Event CreateEvent(
    OperationalStats::Event::EventKind event_kind, int64_t event_time_seconds) {
  OperationalStats::Event event;
  event.set_event_type(event_kind);
  google::protobuf::Timestamp t;
  t.set_seconds(event_time_seconds);
  *event.mutable_timestamp() = t;
  return event;
}

OperationalStats::DatasetStats CreateDatasetStats(
    int64_t first_access_timestamp_seconds) {
  OperationalStats::DatasetStats dataset_stats;
  google::protobuf::Timestamp t;
  t.set_seconds(first_access_timestamp_seconds);
  *(dataset_stats.mutable_first_access_timestamp()) = t;
  return dataset_stats;
}

TEST(OpStatsUtils, GetLastSuccessfulContributionTimeMixedLegacyAndNewOpStats) {
  OperationalStats first_success_run;
  first_success_run.set_task_name(kTaskName);
  *first_success_run.add_events() = CreateEvent(kUploadStartedEvent, 10000);
  OperationalStats second_success_run;
  OperationalStats::PhaseStats upload;
  upload.set_phase(OperationalStats::PhaseStats::UPLOAD);
  upload.set_task_name(kTaskName);
  *upload.add_events() = CreateEvent(kUploadStartedEvent, 20000);
  *second_success_run.add_phase_stats() = upload;
  OperationalStats third_failure_run;
  OperationalStats::PhaseStats failed_upload;
  upload.set_phase(OperationalStats::PhaseStats::UPLOAD);
  failed_upload.set_task_name(kTaskName);
  *failed_upload.add_events() = CreateEvent(kUploadStartedEvent, 30000);
  *failed_upload.add_events() = CreateEvent(kUploadServerAbortedEvent, 30001);
  *third_failure_run.add_phase_stats() = failed_upload;
  OpStatsSequence data;
  *data.add_opstats() = first_success_run;
  *data.add_opstats() = second_success_run;
  *data.add_opstats() = third_failure_run;

  std::optional<OperationalStats> returned_stats =
      GetLastSuccessfulContribution(data, kTaskName);
  ASSERT_TRUE(returned_stats.has_value());
  std::optional<google::protobuf::Timestamp> last_contribution_time =
      GetLastSuccessfulContributionTime(data, kTaskName);
  ASSERT_EQ(last_contribution_time->seconds(), 20000);
}

TEST(OpStatsUtils,
     GetLastSuccessfulContributionTimeNotFoundMixedLegacyAndNewOpStats) {
  OperationalStats first_success_run;
  first_success_run.set_task_name("unrelated_task");
  *first_success_run.add_events() = CreateEvent(kUploadStartedEvent, 10000);
  OperationalStats second_success_run;
  OperationalStats::PhaseStats upload;
  upload.set_phase(OperationalStats::PhaseStats::UPLOAD);
  upload.set_task_name("unrelated_task_2");
  *upload.add_events() = CreateEvent(kUploadStartedEvent, 20000);
  *second_success_run.add_phase_stats() = upload;
  OperationalStats third_failure_run;
  OperationalStats::PhaseStats failed_upload;
  upload.set_phase(OperationalStats::PhaseStats::UPLOAD);
  failed_upload.set_task_name(kTaskName);
  *failed_upload.add_events() = CreateEvent(kUploadStartedEvent, 30000);
  *failed_upload.add_events() = CreateEvent(kUploadServerAbortedEvent, 30001);
  *third_failure_run.add_phase_stats() = failed_upload;
  OpStatsSequence data;
  *data.add_opstats() = first_success_run;
  *data.add_opstats() = second_success_run;
  *data.add_opstats() = third_failure_run;

  ASSERT_EQ(GetLastSuccessfulContributionTime(data, kTaskName), std::nullopt);
}

TEST(OpStatsUtils,
     GetLastSuccessfulContributionTimeReturnsUploadStartedTimestamp) {
  OperationalStats stats;
  stats.set_task_name(kTaskName);

  int64_t upload_started_time_sec = 1000;
  stats.mutable_events()->Add(
      CreateEvent(kUploadStartedEvent, upload_started_time_sec));

  OpStatsSequence opstats_sequence;
  *opstats_sequence.add_opstats() = std::move(stats);

  auto last_time =
      GetLastSuccessfulContributionTime(opstats_sequence, kTaskName);
  EXPECT_EQ(last_time->seconds(), upload_started_time_sec);
}

TEST(OpStatsUtils,
     GetLastSuccessfulContributionTimeForPatternReturnsUploadStartedTimestamp) {
  OperationalStats stats;
  stats.set_task_name("my_task.group_swor");

  int64_t upload_started_time_sec = 1000;
  stats.mutable_events()->Add(
      CreateEvent(kUploadStartedEvent, upload_started_time_sec));

  OpStatsSequence opstats_sequence;
  *opstats_sequence.add_opstats() = std::move(stats);

  RE2 pattern("(.*\\b)group_swor(\\b.*)");
  auto last_time =
      GetLastSuccessfulContributionTimeForPattern(opstats_sequence, pattern);
  EXPECT_EQ(last_time->seconds(), upload_started_time_sec);
}

TEST(OpStatsUtils,
     GetLastSuccessfulContributionTimeForPatternMixedLegacyAndNewOpStats) {
  std::string task_name = "my_task.group_swor";
  OperationalStats first_success_run;
  first_success_run.set_task_name(task_name);
  *first_success_run.add_events() = CreateEvent(kUploadStartedEvent, 10000);
  OperationalStats second_success_run;
  OperationalStats::PhaseStats upload;
  upload.set_phase(OperationalStats::PhaseStats::UPLOAD);
  upload.set_task_name(task_name);
  *upload.add_events() = CreateEvent(kUploadStartedEvent, 20000);
  *second_success_run.add_phase_stats() = upload;
  OperationalStats third_failure_run;
  OperationalStats::PhaseStats failed_upload;
  upload.set_phase(OperationalStats::PhaseStats::UPLOAD);
  failed_upload.set_task_name(task_name);
  *failed_upload.add_events() = CreateEvent(kUploadStartedEvent, 30000);
  *failed_upload.add_events() = CreateEvent(kUploadServerAbortedEvent, 30001);
  *third_failure_run.add_phase_stats() = failed_upload;
  OpStatsSequence data;
  *data.add_opstats() = first_success_run;
  *data.add_opstats() = second_success_run;
  *data.add_opstats() = third_failure_run;

  RE2 pattern("(.*\\b)group_swor(\\b.*)");
  auto last_time = GetLastSuccessfulContributionTimeForPattern(data, pattern);
  EXPECT_EQ(last_time->seconds(), 20000);
}

TEST(OpStatsUtils,
     GetLastSuccessfulContributionTimeReturnNotFoundForUnknownTask) {
  OperationalStats stats;
  stats.set_task_name(kTaskName);

  int64_t upload_started_time_sec = 1000;
  stats.mutable_events()->Add(
      CreateEvent(kUploadStartedEvent, upload_started_time_sec));

  OpStatsSequence opstats_sequence;
  *opstats_sequence.add_opstats() = std::move(stats);
  EXPECT_FALSE(
      GetLastSuccessfulContributionTime(opstats_sequence, "task_name_not_found")
          .has_value());
}

TEST(OpStatsUtils,
     GetLastSuccessfulContributionTimeReturnsMostRecentUploadStartedTimestamp) {
  OpStatsSequence opstats_sequence;

  OperationalStats old_stats;
  old_stats.set_task_name(kTaskName);
  old_stats.mutable_events()->Add(CreateEvent(kUploadStartedEvent, 1000));
  *opstats_sequence.add_opstats() = std::move(old_stats);

  OperationalStats new_stats;
  new_stats.set_task_name(kTaskName);
  int64_t new_upload_started_sec = 2000;
  new_stats.mutable_events()->Add(
      CreateEvent(kUploadStartedEvent, new_upload_started_sec));
  *opstats_sequence.add_opstats() = std::move(new_stats);

  auto last_time =
      GetLastSuccessfulContributionTime(opstats_sequence, kTaskName);
  EXPECT_EQ(last_time->seconds(), new_upload_started_sec);
}

TEST(OpStatsUtils,
     GetLastSuccessfulContributionTimeReturnNotFoundIfAbortedByServer) {
  OperationalStats stats;
  stats.set_task_name(kTaskName);
  stats.mutable_events()->Add(CreateEvent(kUploadStartedEvent, 1000));
  stats.mutable_events()->Add(CreateEvent(kUploadServerAbortedEvent, 1001));

  OpStatsSequence opstats_sequence;
  *opstats_sequence.add_opstats() = std::move(stats);

  EXPECT_FALSE(GetLastSuccessfulContributionTime(opstats_sequence, kTaskName)
                   .has_value());
}

TEST(OpStatsUtils,
     GetLastSuccessfulContributionTimeReturnOlderIfNewerAbortedByServer) {
  OpStatsSequence opstats_sequence;

  OperationalStats old_stats;
  old_stats.set_task_name(kTaskName);
  int64_t expected_time_sec = 1000;
  old_stats.mutable_events()->Add(
      CreateEvent(kUploadStartedEvent, expected_time_sec));
  *opstats_sequence.add_opstats() = std::move(old_stats);

  OperationalStats new_stats;
  new_stats.set_task_name(kTaskName);
  new_stats.mutable_events()->Add(CreateEvent(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_STARTED, 2000));
  new_stats.mutable_events()->Add(CreateEvent(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_SERVER_ABORTED, 2001));
  *opstats_sequence.add_opstats() = std::move(new_stats);

  auto last_time =
      GetLastSuccessfulContributionTime(opstats_sequence, kTaskName);
  EXPECT_EQ(last_time->seconds(), expected_time_sec);
}

TEST(OpStatsUtils,
     GetLastSuccessfulContributionTimeReturnNotFoundIfNoUploadStarted) {
  OperationalStats stats;
  stats.set_task_name(kTaskName);
  stats.mutable_events()->Add(
      CreateEvent(OperationalStats::Event::EVENT_KIND_CHECKIN_STARTED, 1000));

  OpStatsSequence opstats_sequence;
  *opstats_sequence.add_opstats() = std::move(stats);
  EXPECT_FALSE(GetLastSuccessfulContributionTime(opstats_sequence, kTaskName)
                   .has_value());
}

TEST(OpStatsUtils, GetOperationalStatsForTimeRange) {
  // Successful contribution before the start of the time range.
  OperationalStats first_run;
  first_run.set_task_name("task_1");
  *first_run.add_events() = CreateEvent(kUploadStartedEvent, 9999);

  // Successful contribution within the time range.
  OperationalStats second_run;
  OperationalStats::PhaseStats* upload = second_run.add_phase_stats();
  upload->set_task_name("task_2");
  upload->set_phase(OperationalStats::PhaseStats::UPLOAD);
  OperationalStats::Event upload_event =
      CreateEvent(kUploadStartedEvent, 11000);
  *upload->add_events() = upload_event;

  // Failed contribution within the time range.
  OperationalStats third_run;
  OperationalStats::PhaseStats* upload_2 = third_run.add_phase_stats();
  upload_2->set_task_name("task_2");
  upload_2->set_phase(OperationalStats::PhaseStats::UPLOAD);
  OperationalStats::Event upload_event_2 =
      CreateEvent(kUploadStartedEvent, 12000);
  *upload_2->add_events() = upload_event_2;
  OperationalStats::Event upload_failed_event =
      CreateEvent(kUploadServerAbortedEvent, 12001);
  *upload_2->add_events() = upload_failed_event;

  // Successful contribution after the end of the time range.
  OperationalStats fourth_run;
  OperationalStats::PhaseStats* upload_3 = fourth_run.add_phase_stats();
  upload_3->set_task_name("task_1");
  upload_3->set_phase(OperationalStats::PhaseStats::UPLOAD);
  *upload_3->add_events() = CreateEvent(kUploadStartedEvent, 14000);

  OpStatsSequence data;
  *data.add_opstats() = first_run;
  *data.add_opstats() = second_run;
  *data.add_opstats() = third_run;
  *data.add_opstats() = fourth_run;

  auto stats = GetOperationalStatsForTimeRange(
      data, absl::FromUnixSeconds(10000), absl::FromUnixSeconds(13000));
  // The returned OperationalStats should be in legacy format without
  // PhaseStats.
  OperationalStats expected_proto_1;
  expected_proto_1.set_task_name("task_2");
  *expected_proto_1.add_events() = upload_event;
  OperationalStats expected_proto_2;
  expected_proto_2.set_task_name("task_2");
  *expected_proto_2.add_events() = upload_event_2;
  *expected_proto_2.add_events() = upload_failed_event;
  EXPECT_THAT(stats, ElementsAre(EqualsProto(expected_proto_2),
                                 EqualsProto(expected_proto_1)));
}

TEST(OpStatsUtils, GetOperationalStatsForTimeRangeMultipleTasksInSameRun) {
  OperationalStats run_with_3_tasks;

  // First task is before the lower bound of time range.
  OperationalStats::PhaseStats* computation_1 =
      run_with_3_tasks.add_phase_stats();
  computation_1->set_task_name("task_1");
  computation_1->set_phase(OperationalStats::PhaseStats::COMPUTATION);
  OperationalStats::Event computation_event_1 = CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED, 10000);
  *computation_1->add_events() = computation_event_1;
  OperationalStats::PhaseStats* upload_1 = run_with_3_tasks.add_phase_stats();
  upload_1->set_phase(OperationalStats::PhaseStats::UPLOAD);
  OperationalStats::Event upload_event_1 =
      CreateEvent(kUploadStartedEvent, 10001);
  *upload_1->add_events() = upload_event_1;

  // Second task is within the time range.
  OperationalStats::PhaseStats* computation_2 =
      run_with_3_tasks.add_phase_stats();
  computation_2->set_task_name("task_2");
  computation_2->set_phase(OperationalStats::PhaseStats::COMPUTATION);
  OperationalStats::Event computation_event_2 = CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED, 11000);
  *computation_2->add_events() = computation_event_2;
  OperationalStats::PhaseStats* upload_2 = run_with_3_tasks.add_phase_stats();
  upload_2->set_phase(OperationalStats::PhaseStats::UPLOAD);
  OperationalStats::Event upload_event_2 =
      CreateEvent(kUploadStartedEvent, 11001);
  *upload_2->add_events() = upload_event_2;

  // Third task is also within the time range.
  OperationalStats::PhaseStats* computation_3 =
      run_with_3_tasks.add_phase_stats();
  computation_3->set_task_name("task_3");
  computation_3->set_phase(OperationalStats::PhaseStats::COMPUTATION);
  OperationalStats::Event computation_event_3 = CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED, 12000);
  *computation_3->add_events() = computation_event_3;
  OperationalStats::PhaseStats* upload_3 = run_with_3_tasks.add_phase_stats();
  upload_3->set_phase(OperationalStats::PhaseStats::UPLOAD);
  OperationalStats::Event upload_event_3 =
      CreateEvent(kUploadStartedEvent, 12001);
  *upload_3->add_events() = upload_event_3;

  OpStatsSequence data;
  *data.add_opstats() = run_with_3_tasks;

  auto stats = GetOperationalStatsForTimeRange(
      data, absl::FromUnixSeconds(11000), absl::FromUnixSeconds(13000));

  // The returned OperationalStats should be in legacy format without
  // PhaseStats.
  OperationalStats expected_proto_1;
  expected_proto_1.set_task_name("task_2");
  *expected_proto_1.add_events() = computation_event_2;
  *expected_proto_1.add_events() = upload_event_2;
  OperationalStats expected_proto_2;
  expected_proto_2.set_task_name("task_3");
  *expected_proto_2.add_events() = computation_event_3;
  *expected_proto_2.add_events() = upload_event_3;
  EXPECT_THAT(stats, ElementsAre(EqualsProto(expected_proto_2),
                                 EqualsProto(expected_proto_1)));
}

TEST(OpStatsUtils, GetPreviousCollectionFirstAccessTimeReturnsFirstAccessTime) {
  OperationalStats stats;
  stats.set_task_name(kTaskName);

  int64_t first_access_time_sec = 2000;
  stats.mutable_events()->Add(CreateEvent(kUploadStartedEvent, 1000));
  stats.mutable_dataset_stats()->insert(
      {kCollectionUri, CreateDatasetStats(first_access_time_sec)});

  OpStatsSequence opstats_sequence;
  *opstats_sequence.add_opstats() = std::move(stats);

  auto first_access_time_map =
      GetPreviousCollectionFirstAccessTimeMap(opstats_sequence, kTaskName);
  EXPECT_TRUE(first_access_time_map.value().contains(kCollectionUri));
  EXPECT_EQ(first_access_time_map.value()[kCollectionUri].seconds(),
            first_access_time_sec);
}

TEST(OpStatsUtils,
     GetPreviousCollectionFirstAccessTimeReturnsMostRecentAccessTime) {
  OpStatsSequence opstats_sequence;

  OperationalStats old_stats;
  old_stats.set_task_name(kTaskName);
  old_stats.mutable_events()->Add(CreateEvent(kUploadStartedEvent, 1000));
  old_stats.mutable_dataset_stats()->insert(
      {kCollectionUri, CreateDatasetStats(1500)});
  *opstats_sequence.add_opstats() = std::move(old_stats);

  OperationalStats new_stats;
  new_stats.set_task_name(kTaskName);
  int64_t first_access_time_sec = 2000;
  new_stats.mutable_events()->Add(CreateEvent(kUploadStartedEvent, 2500));
  new_stats.mutable_dataset_stats()->insert(
      {kCollectionUri, CreateDatasetStats(first_access_time_sec)});
  *opstats_sequence.add_opstats() = std::move(new_stats);

  auto first_access_time_map =
      GetPreviousCollectionFirstAccessTimeMap(opstats_sequence, kTaskName);
  EXPECT_TRUE(first_access_time_map.value().contains(kCollectionUri));
  EXPECT_EQ(first_access_time_map.value()[kCollectionUri].seconds(),
            first_access_time_sec);
}

TEST(OpStatsUtils,
     GetPreviousCollectionFirstAccessTimeReturnNotFoundAbortedByServer) {
  OperationalStats stats;
  stats.set_task_name(kTaskName);
  stats.mutable_events()->Add(CreateEvent(kUploadStartedEvent, 1000));
  stats.mutable_events()->Add(CreateEvent(kUploadServerAbortedEvent, 1001));
  stats.mutable_dataset_stats()->insert(
      {kCollectionUri, CreateDatasetStats(1002)});

  OpStatsSequence opstats_sequence;
  *opstats_sequence.add_opstats() = std::move(stats);

  EXPECT_FALSE(
      GetPreviousCollectionFirstAccessTimeMap(opstats_sequence, kTaskName)
          .has_value());
}

TEST(OpStatsUtils,
     GetPreviousCollectionFirstAccessTimeMixedLegacyAndNewOpStats) {
  OperationalStats first_success_run;
  first_success_run.set_task_name(kTaskName);
  *first_success_run.add_events() = CreateEvent(kUploadStartedEvent, 10000);
  first_success_run.mutable_dataset_stats()->insert(
      {kCollectionUri, CreateDatasetStats(10001)});
  OperationalStats second_success_run;
  OperationalStats::PhaseStats upload;
  int first_access_time_sec = 20001;
  upload.set_phase(OperationalStats::PhaseStats::UPLOAD);
  upload.set_task_name(kTaskName);
  *upload.add_events() = CreateEvent(kUploadStartedEvent, 20000);
  upload.mutable_dataset_stats()->insert(
      {kCollectionUri, CreateDatasetStats(first_access_time_sec)});
  *second_success_run.add_phase_stats() = upload;
  OperationalStats third_failure_run;
  OperationalStats::PhaseStats failed_upload;
  upload.set_phase(OperationalStats::PhaseStats::UPLOAD);
  failed_upload.set_task_name(kTaskName);
  *failed_upload.add_events() = CreateEvent(kUploadStartedEvent, 30000);
  *failed_upload.add_events() = CreateEvent(kUploadServerAbortedEvent, 30001);
  failed_upload.mutable_dataset_stats()->insert(
      {kCollectionUri, CreateDatasetStats(2500)});
  *third_failure_run.add_phase_stats() = failed_upload;
  OpStatsSequence data;
  *data.add_opstats() = first_success_run;
  *data.add_opstats() = second_success_run;
  *data.add_opstats() = third_failure_run;

  std::optional<absl::flat_hash_map<std::string, google::protobuf::Timestamp>>
      first_access_time_map =
          GetPreviousCollectionFirstAccessTimeMap(data, kTaskName);
  EXPECT_TRUE(first_access_time_map.value().contains(kCollectionUri));
  EXPECT_EQ(first_access_time_map.value()[kCollectionUri].seconds(),
            first_access_time_sec);
}

TEST(OpStatsUtils,
     GetLastSuccessfulContributionMinSepPolicyIndexReturnsNullForUnkownTask) {
  OperationalStats stats;
  stats.set_task_name(kTaskName);
  stats.set_min_sep_policy_index(1);
  stats.mutable_events()->Add(CreateEvent(kUploadStartedEvent, 1000));

  OpStatsSequence opstats_sequence;
  *opstats_sequence.add_opstats() = std::move(stats);
  EXPECT_FALSE(GetLastSuccessfulContributionMinSepPolicyIndex(
                   opstats_sequence, /*task_name=*/"unknown_task_name")
                   .has_value());
}

TEST(OpStatsUtils,
     GetLastSuccessfulContributionMinSepPolicyIndexReturnsMostRecentIndex) {
  OpStatsSequence opstats_sequence;

  OperationalStats old_stats;
  old_stats.set_task_name(kTaskName);
  old_stats.set_min_sep_policy_index(1);
  *opstats_sequence.add_opstats() = std::move(old_stats);

  OperationalStats new_stats;
  new_stats.set_task_name(kTaskName);
  new_stats.set_min_sep_policy_index(2);
  new_stats.mutable_events()->Add(CreateEvent(kUploadStartedEvent, 1000));
  *opstats_sequence.add_opstats() = std::move(new_stats);

  auto last_index = GetLastSuccessfulContributionMinSepPolicyIndex(
      opstats_sequence, kTaskName);
  EXPECT_EQ(last_index.value(), 2);
}

TEST(OpStatsUtils,
     GetLastSuccessfulContributionMinSepPolicyIndexReturnsNullIfAborted) {
  OperationalStats stats;
  stats.set_task_name(kTaskName);
  stats.mutable_events()->Add(CreateEvent(kUploadStartedEvent, 1000));
  stats.mutable_events()->Add(CreateEvent(kUploadServerAbortedEvent, 1001));
  stats.set_min_sep_policy_index(1);

  OpStatsSequence opstats_sequence;
  *opstats_sequence.add_opstats() = std::move(stats);

  EXPECT_FALSE(GetLastSuccessfulContributionMinSepPolicyIndex(opstats_sequence,
                                                              kTaskName)
                   .has_value());
}

TEST(OpStatsUtils,
     GetLastSuccessfulContributionMinSepPolicyIndexReturnsOlderIfNewerAborted) {
  OpStatsSequence opstats_sequence;

  OperationalStats old_stats;
  old_stats.set_task_name(kTaskName);
  old_stats.mutable_events()->Add(CreateEvent(kUploadStartedEvent, 1000));
  old_stats.set_min_sep_policy_index(1);
  *opstats_sequence.add_opstats() = std::move(old_stats);

  OperationalStats new_stats;
  new_stats.set_task_name(kTaskName);
  new_stats.mutable_events()->Add(CreateEvent(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_STARTED, 2000));
  new_stats.mutable_events()->Add(CreateEvent(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_SERVER_ABORTED, 2001));
  new_stats.set_min_sep_policy_index(2);
  *opstats_sequence.add_opstats() = std::move(new_stats);

  auto last_index = GetLastSuccessfulContributionMinSepPolicyIndex(
      opstats_sequence, kTaskName);
  EXPECT_EQ(last_index.value(), 1);
}

TEST(OpStatsUtils,
     GetLastSuccessfulContributionMinSepPolicyIndexReturnsNullIfNoUpload) {
  OperationalStats stats;
  stats.set_task_name(kTaskName);
  stats.mutable_events()->Add(
      CreateEvent(OperationalStats::Event::EVENT_KIND_CHECKIN_STARTED, 1000));
  stats.set_min_sep_policy_index(1);

  OpStatsSequence opstats_sequence;
  *opstats_sequence.add_opstats() = std::move(stats);
  EXPECT_FALSE(GetLastSuccessfulContributionMinSepPolicyIndex(opstats_sequence,
                                                              kTaskName)
                   .has_value());
}

TEST(OpStatsUtils,
     GetLastSuccessfulContributionMinSepPolicyIndexWithPhaseStats) {
  OperationalStats first_success_run;
  OperationalStats::PhaseStats compute;
  compute.set_phase(OperationalStats::PhaseStats::COMPUTATION);
  compute.set_task_name(kTaskName);
  *compute.add_events() = CreateEvent(kComputationFinishedEvent, 10000);
  compute.set_min_sep_policy_index(1);
  OperationalStats::PhaseStats upload;
  upload.set_phase(OperationalStats::PhaseStats::UPLOAD);
  *upload.add_events() = CreateEvent(kUploadStartedEvent, 15000);
  upload.set_task_name(kTaskName);
  *first_success_run.add_phase_stats() = compute;
  *first_success_run.add_phase_stats() = upload;

  OperationalStats second_success_run;
  compute.set_phase(OperationalStats::PhaseStats::COMPUTATION);
  compute.set_task_name(kTaskName);
  *compute.add_events() = CreateEvent(kComputationFinishedEvent, 20000);
  compute.set_min_sep_policy_index(2);
  compute.set_task_name(kTaskName);
  upload.set_phase(OperationalStats::PhaseStats::UPLOAD);
  *upload.add_events() = CreateEvent(kUploadStartedEvent, 25000);
  upload.set_task_name(kTaskName);
  *second_success_run.add_phase_stats() = compute;
  *second_success_run.add_phase_stats() = upload;

  OperationalStats third_failure_run;
  OperationalStats::PhaseStats failed_upload;
  failed_upload.set_phase(OperationalStats::PhaseStats::UPLOAD);
  failed_upload.set_task_name(kTaskName);
  *failed_upload.add_events() = CreateEvent(kUploadStartedEvent, 30000);
  *failed_upload.add_events() = CreateEvent(kUploadServerAbortedEvent, 30001);
  *third_failure_run.add_phase_stats() = failed_upload;
  third_failure_run.set_min_sep_policy_index(3);

  OpStatsSequence data;
  *data.add_opstats() = first_success_run;
  *data.add_opstats() = second_success_run;
  *data.add_opstats() = third_failure_run;

  auto last_index =
      GetLastSuccessfulContributionMinSepPolicyIndex(data, kTaskName);
  EXPECT_EQ(last_index.value(), 2);
}

TEST(OpStatsUtils,
     GetLastSuccessfulContributionMinSepPolicyIndexMixedLegacyAndNewOpStats) {
  OperationalStats first_success_run;
  first_success_run.set_task_name(kTaskName);
  *first_success_run.add_events() = CreateEvent(kUploadStartedEvent, 10000);
  first_success_run.set_min_sep_policy_index(1);

  OperationalStats second_success_run;
  OperationalStats::PhaseStats compute;
  compute.set_phase(OperationalStats::PhaseStats::COMPUTATION);
  compute.set_task_name(kTaskName);
  *compute.add_events() = CreateEvent(kComputationFinishedEvent, 20000);
  compute.set_min_sep_policy_index(2);
  compute.set_task_name(kTaskName);
  OperationalStats::PhaseStats upload;
  upload.set_phase(OperationalStats::PhaseStats::UPLOAD);
  *upload.add_events() = CreateEvent(kUploadStartedEvent, 25000);
  upload.set_task_name(kTaskName);
  *second_success_run.add_phase_stats() = compute;
  *second_success_run.add_phase_stats() = upload;

  OperationalStats third_failure_run;
  OperationalStats::PhaseStats failed_upload;
  failed_upload.set_phase(OperationalStats::PhaseStats::UPLOAD);
  failed_upload.set_task_name(kTaskName);
  *failed_upload.add_events() = CreateEvent(kUploadStartedEvent, 30000);
  *failed_upload.add_events() = CreateEvent(kUploadServerAbortedEvent, 30001);
  *third_failure_run.add_phase_stats() = failed_upload;
  third_failure_run.set_min_sep_policy_index(3);

  OpStatsSequence data;
  *data.add_opstats() = first_success_run;
  *data.add_opstats() = second_success_run;
  *data.add_opstats() = third_failure_run;

  auto last_index =
      GetLastSuccessfulContributionMinSepPolicyIndex(data, kTaskName);
  EXPECT_TRUE(last_index.has_value());
  EXPECT_EQ(last_index.value(), 2);
}

TEST(OpStatsUtils,
     GetLastSuccessfulContributionMinSepPolicyIndexMultipleTasksInSameRun) {
  OperationalStats run_with_3_tasks;

  OperationalStats::PhaseStats* computation_1 =
      run_with_3_tasks.add_phase_stats();
  computation_1->set_task_name("task_1");
  computation_1->set_phase(OperationalStats::PhaseStats::COMPUTATION);
  OperationalStats::Event computation_event_1 = CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED, 10000);
  *computation_1->add_events() = computation_event_1;
  computation_1->set_min_sep_policy_index(1);
  OperationalStats::PhaseStats* upload_1 = run_with_3_tasks.add_phase_stats();
  upload_1->set_phase(OperationalStats::PhaseStats::UPLOAD);
  OperationalStats::Event upload_event_1 =
      CreateEvent(kUploadStartedEvent, 10001);
  *upload_1->add_events() = upload_event_1;

  OperationalStats::PhaseStats* computation_2 =
      run_with_3_tasks.add_phase_stats();
  computation_2->set_task_name("task_2");
  computation_2->set_phase(OperationalStats::PhaseStats::COMPUTATION);
  OperationalStats::Event computation_event_2 = CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED, 11000);
  computation_2->set_min_sep_policy_index(2);
  *computation_2->add_events() = computation_event_2;
  OperationalStats::PhaseStats* upload_2 = run_with_3_tasks.add_phase_stats();
  upload_2->set_phase(OperationalStats::PhaseStats::UPLOAD);
  OperationalStats::Event upload_event_2 =
      CreateEvent(kUploadStartedEvent, 11001);
  *upload_2->add_events() = upload_event_2;

  OperationalStats::PhaseStats* computation_3 =
      run_with_3_tasks.add_phase_stats();
  computation_3->set_task_name("task_3");
  computation_3->set_phase(OperationalStats::PhaseStats::COMPUTATION);
  OperationalStats::Event computation_event_3 = CreateEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED, 12000);
  computation_3->set_min_sep_policy_index(3);
  *computation_3->add_events() = computation_event_3;
  OperationalStats::PhaseStats* upload_3 = run_with_3_tasks.add_phase_stats();
  upload_3->set_phase(OperationalStats::PhaseStats::UPLOAD);
  OperationalStats::Event upload_event_3 =
      CreateEvent(kUploadStartedEvent, 12001);
  *upload_3->add_events() = upload_event_3;

  OpStatsSequence data;
  *data.add_opstats() = run_with_3_tasks;

  auto stats = GetOperationalStatsForTimeRange(
      data, absl::FromUnixSeconds(11000), absl::FromUnixSeconds(13000));

  auto last_index_task_1 =
      GetLastSuccessfulContributionMinSepPolicyIndex(data, "task_1");
  EXPECT_TRUE(last_index_task_1.has_value());
  EXPECT_EQ(last_index_task_1.value(), 1);

  auto last_index_task_2 =
      GetLastSuccessfulContributionMinSepPolicyIndex(data, "task_2");
  EXPECT_TRUE(last_index_task_2.has_value());
  EXPECT_EQ(last_index_task_2.value(), 2);

  auto last_index_task_3 =
      GetLastSuccessfulContributionMinSepPolicyIndex(data, "task_3");
  EXPECT_TRUE(last_index_task_3.has_value());
  EXPECT_EQ(last_index_task_3.value(), 3);
}
}  // namespace
}  // namespace opstats
}  // namespace client
}  // namespace fcp
