/*
 * Copyright 2025 Google LLC
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
#include "fcp/client/engine/privacy_id_utils.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "google/type/datetime.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/civil_time.h"
#include "fcp/client/event_time_range.pb.h"
#include "fcp/client/example_query_result.pb.h"
#include "fcp/confidentialcompute/constants.h"
#include "fcp/protos/confidentialcompute/windowing_schedule.pb.h"
#include "fcp/protos/plan.pb.h"
#include "fcp/testing/testing.h"

namespace fcp {
namespace client {
namespace engine {
namespace {

using ::fcp::EqualsProto;
using ::fcp::confidentialcompute::WindowingSchedule;
using ::google::internal::federated::plan::ExampleQuerySpec;
using ::google::internal::federated::plan::PrivacyIdConfig;
using ::testing::Eq;
using ::testing::ExplainMatchResult;
using ::testing::HasSubstr;
using ::testing::UnorderedElementsAre;

MATCHER_P2(StatusIs, expected_code, message_matcher, "") {
  return ExplainMatchResult(IsCode(expected_code), arg, result_listener) &&
         ExplainMatchResult(message_matcher, arg.status().message(),
                            result_listener);
}

MATCHER_P(IsOkAndHolds, m, "") {
  return testing::ExplainMatchResult(IsOk(), arg, result_listener) &&
         testing::ExplainMatchResult(m, arg.value(), result_listener);
}

MATCHER_P2(PerPrivacyIdResultIs, privacy_id, example_query_result, "") {
  return ExplainMatchResult(Eq(privacy_id), arg.privacy_id, result_listener) &&
         ExplainMatchResult(EqualsProto(example_query_result),
                            arg.example_query_result, result_listener);
}

PrivacyIdConfig CreatePrivacyIdConfig(
    int window_size,
    WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::Unit unit,
    int start_year, int start_month, int start_day,
    std::optional<int> start_hours = std::nullopt,
    std::optional<int> start_minutes = std::nullopt,
    std::optional<int> start_seconds = std::nullopt) {
  PrivacyIdConfig config;
  auto* schedule =
      config.mutable_windowing_schedule()->mutable_civil_time_window_schedule();
  schedule->mutable_size()->set_size(window_size);
  schedule->mutable_size()->set_unit(unit);
  schedule->mutable_shift()->set_size(window_size);
  schedule->mutable_shift()->set_unit(unit);
  if (start_year != 0) schedule->mutable_start_date()->set_year(start_year);
  if (start_month != 0) schedule->mutable_start_date()->set_month(start_month);
  if (start_day != 0) schedule->mutable_start_date()->set_day(start_day);
  if (start_hours.has_value())
    schedule->mutable_start_time()->set_hours(*start_hours);
  if (start_minutes.has_value())
    schedule->mutable_start_time()->set_minutes(*start_minutes);
  if (start_seconds.has_value())
    schedule->mutable_start_time()->set_seconds(*start_seconds);
  schedule->set_time_zone_scheme(
      WindowingSchedule::CivilTimeWindowSchedule::IGNORE);
  return config;
}


ExampleQuerySpec::ExampleQuery CreateExampleQuery() {
  return PARSE_TEXT_PROTO(R"pb(
    example_selector { collection_uri: "app:/test_collection" }
  )pb");
}

ExampleQueryResult CreateExampleQueryResult(
    const std::vector<std::string>& event_times,
    const std::vector<int64_t>& values) {
  ExampleQueryResult result;
  auto& vectors = *result.mutable_vector_data()->mutable_vectors();

  ExampleQueryResult::VectorData::Values event_time_values;
  event_time_values.mutable_string_values()->mutable_value()->Add(
      event_times.begin(), event_times.end());
  vectors[absl::StrCat("prefix/", confidential_compute::kEventTimeColumnName)] =
      event_time_values;

  ExampleQueryResult::VectorData::Values data_values;
  data_values.mutable_int64_values()->mutable_value()->Add(values.begin(),
                                                           values.end());
  vectors["test_data"] = data_values;
  result.mutable_stats()->set_output_rows_count(
      static_cast<int32_t>(values.size()));
  return result;
}

ExampleQueryResult AddEventTimeRange(ExampleQueryResult result,
                                     const google::type::DateTime& start_time,
                                     const google::type::DateTime& end_time) {
  *result.mutable_stats()
       ->mutable_cross_query_event_time_range()
       ->mutable_start_event_time() = start_time;
  *result.mutable_stats()
       ->mutable_cross_query_event_time_range()
       ->mutable_end_event_time() = end_time;
  return result;
}

google::type::DateTime CreateDateTime(int year, int month, int day, int hours,
                                      int minutes, int seconds) {
  google::type::DateTime date_time;
  date_time.set_year(year);
  date_time.set_month(month);
  date_time.set_day(day);
  date_time.set_hours(hours);
  date_time.set_minutes(minutes);
  date_time.set_seconds(seconds);
  return date_time;
}

ExampleQueryResult AddPrivacyId(ExampleQueryResult result,
                                const std::string privacy_id) {
  ExampleQueryResult::VectorData::Values privacy_id_values;
  privacy_id_values.mutable_bytes_values()->add_value(privacy_id);
  (*result.mutable_vector_data()
        ->mutable_vectors())[confidential_compute::kPrivacyIdColumnName] =
      privacy_id_values;
  return result;
}

TEST(PrivacyIdUtilsTest, GetPrivacyId) {
  absl::string_view source_id = "test_source";
  absl::CivilSecond window_start(2024, 5, 15, 10, 0, 0);

  absl::StatusOr<std::string> id1 = GetPrivacyId(source_id, window_start);
  ASSERT_OK(id1);
  absl::StatusOr<std::string> id2 = GetPrivacyId(source_id, window_start);
  EXPECT_EQ(*id1, *id2);

  absl::CivilSecond different_window(2024, 5, 15, 11, 0, 0);
  absl::StatusOr<std::string> id3 = GetPrivacyId(source_id, different_window);
  EXPECT_NE(*id1, *id3);

  absl::StatusOr<std::string> id4 = GetPrivacyId("other_source", window_start);
  EXPECT_NE(*id1, *id4);
}

TEST(PrivacyIdUtilsTest, MissingWindowSchedule) {
  PrivacyIdConfig config = CreatePrivacyIdConfig(
      /*window_size=*/1,
      WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::DAYS,
      /*start_year=*/2024, /*start_month=*/1, /*start_day=*/1);
  config.mutable_windowing_schedule()->clear_civil_time_window_schedule();
  ExampleQueryResult query_result = CreateExampleQueryResult(
      {"2024-01-01T10:15:00+00:00", "2024-01-02T12:00:00+00:00",
       "2024-01-01T08:00:00-09:00"},
      {1, 2, 3});
  EXPECT_THAT(SplitResultsByPrivacyId(CreateExampleQuery(), query_result,
                                      config, "test_source"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Missing CivilTimeWindowSchedule")));
}

TEST(PrivacyIdUtilsTest, NonTumblingWindow) {
  PrivacyIdConfig config = CreatePrivacyIdConfig(
      /*window_size=*/1,
      WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::DAYS,
      /*start_year=*/2024, /*start_month=*/1, /*start_day=*/1);
  config.mutable_windowing_schedule()
      ->mutable_civil_time_window_schedule()
      ->mutable_shift()
      ->set_size(2);
  ExampleQueryResult query_result = CreateExampleQueryResult(
      {"2024-01-01T10:15:00+00:00", "2024-01-02T12:00:00+00:00",
       "2024-01-01T08:00:00-09:00"},
      {1, 2, 3});
  EXPECT_THAT(SplitResultsByPrivacyId(CreateExampleQuery(), query_result,
                                      config, "test_source"),
              StatusIs(absl::StatusCode::kUnimplemented,
                       HasSubstr("Only tumbling windows are supported")));
}

TEST(PrivacyIdUtilsTest, InvalidWindowSize) {
  ExampleQueryResult query_result = CreateExampleQueryResult(
      {"2024-01-01T10:15:00+00:00", "2024-01-02T12:00:00+00:00",
       "2024-01-01T08:00:00-09:00"},
      {1, 2, 3});
  PrivacyIdConfig config = CreatePrivacyIdConfig(
      /*window_size=*/0,
      WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::DAYS,
      /*start_year=*/2024, /*start_month=*/1, /*start_day=*/1);
  EXPECT_THAT(SplitResultsByPrivacyId(CreateExampleQuery(), query_result,
                                      config, "test_source"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Window size must be positive")));

  config = CreatePrivacyIdConfig(
      /*window_size=*/-1,
      WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::DAYS,
      /*start_year=*/2024, /*start_month=*/1, /*start_day=*/1);
  EXPECT_THAT(SplitResultsByPrivacyId(CreateExampleQuery(), query_result,
                                      config, "test_source"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Window size must be positive")));
}

TEST(PrivacyIdUtilsTest, SplitResultsByPrivacyIdSucceeds) {
  // With a window size of 1 day and rows with event times from Jan 1, 2024
  // to Jan 2, 2024, expect the rows to be split into two separate results,
  // one for Jan 1, 2024 and one for Jan 2, 2024.
  PrivacyIdConfig config = CreatePrivacyIdConfig(
      /*window_size=*/1,
      WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::DAYS,
      /*start_year=*/2024, /*start_month=*/1, /*start_day=*/1);
  ExampleQueryResult query_result = CreateExampleQueryResult(
      {"2024-01-01T10:15:00+00:00", "2024-01-02T12:00:00+00:00",
       "2024-01-01T08:00:00-09:00"},
      {1, 2, 3});

  absl::StatusOr<SplitResults> split_results = SplitResultsByPrivacyId(
      CreateExampleQuery(), query_result, config, "test_source");
  ASSERT_OK(split_results);

  absl::StatusOr<std::string> privacy_id_1 =
      GetPrivacyId("test_source", absl::CivilDay(2024, 1, 1));
  ASSERT_OK(privacy_id_1);
  absl::StatusOr<std::string> privacy_id_2 =
      GetPrivacyId("test_source", absl::CivilDay(2024, 1, 2));
  ASSERT_OK(privacy_id_2);

  EXPECT_THAT(
      split_results->per_privacy_id_results,
      UnorderedElementsAre(
          PerPrivacyIdResultIs(
              *privacy_id_1,
              AddEventTimeRange(
                  CreateExampleQueryResult({"2024-01-01T10:15:00+00:00",
                                            "2024-01-01T08:00:00-09:00"},
                                           {1, 3}),
                  CreateDateTime(2024, 1, 1, 8, 0, 0),
                  CreateDateTime(2024, 1, 1, 10, 0, 0))),
          PerPrivacyIdResultIs(
              *privacy_id_2,
              AddEventTimeRange(
                  CreateExampleQueryResult({"2024-01-02T12:00:00+00:00"}, {2}),
                  CreateDateTime(2024, 1, 2, 12, 0, 0),
                  CreateDateTime(2024, 1, 2, 12, 0, 0)))));
}

TEST(PrivacyIdUtilsTest, SplitResultsByPrivacyIdNoEventTimeColumn) {
  PrivacyIdConfig config = CreatePrivacyIdConfig(
      /*window_size=*/1,
      WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::DAYS,
      /*start_year=*/2024, /*start_month=*/1, /*start_day=*/1);
  ExampleQueryResult query_result =
      CreateExampleQueryResult({"2024-01-01T12:00:00+00:00"}, {1});
  query_result.mutable_vector_data()->mutable_vectors()->erase(
      absl::StrCat("prefix/", confidential_compute::kEventTimeColumnName));

  EXPECT_THAT(SplitResultsByPrivacyId(CreateExampleQuery(), query_result,
                                      config, "test_source"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Required column ending with")));
}

TEST(PrivacyIdUtilsTest, SplitResultsByPrivacyIdMultipleEventTimeColumns) {
  PrivacyIdConfig config = CreatePrivacyIdConfig(
      /*window_size=*/1,
      WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::DAYS,
      /*start_year=*/2024, /*start_month=*/1, /*start_day=*/1);
  ExampleQueryResult query_result =
      CreateExampleQueryResult({"2024-01-01T12:00:00+00:00"}, {1});
  query_result.mutable_vector_data()->mutable_vectors()->insert(
      {absl::StrCat("different_prefix/",
                    confidential_compute::kEventTimeColumnName),
       ExampleQueryResult::VectorData::Values()});

  EXPECT_THAT(SplitResultsByPrivacyId(CreateExampleQuery(), query_result,
                                      config, "test_source"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Multiple columns found ending with")));
}

TEST(PrivacyIdUtilsTest, SplitResultsByPrivacyIdWrongEventTimeType) {
  PrivacyIdConfig config = CreatePrivacyIdConfig(
      /*window_size=*/1,
      WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::DAYS,
      /*start_year=*/2024, /*start_month=*/1, /*start_day=*/1);
  ExampleQueryResult query_result =
      CreateExampleQueryResult({"to be replaced"}, {1});
  // Replace string event time with invalid event time type int64
  (*query_result.mutable_vector_data()->mutable_vectors())
      [absl::StrCat("prefix/", confidential_compute::kEventTimeColumnName)]
          .mutable_int64_values()
          ->add_value(12345);

  EXPECT_THAT(
      SplitResultsByPrivacyId(CreateExampleQuery(), query_result, config,
                              "test_source"),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          HasSubstr(
              "Unexpected data type for event time column, expected string")));
}

TEST(PrivacyIdUtilsTest, SplitResultsByPrivacyIdInvalidEventTimeFormat) {
  PrivacyIdConfig config = CreatePrivacyIdConfig(
      /*window_size=*/1,
      WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::DAYS,
      /*start_year=*/2024, /*start_month=*/1, /*start_day=*/1);
  ExampleQueryResult query_result =
      CreateExampleQueryResult({"invalid event time"}, {1});

  EXPECT_THAT(SplitResultsByPrivacyId(CreateExampleQuery(), query_result,
                                      config, "test_source"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid event time format")));
}

TEST(PrivacyIdUtilsTest, SplitResultsByPrivacyIdEventTimeWithoutTimezone) {
  PrivacyIdConfig config = CreatePrivacyIdConfig(
      /*window_size=*/1,
      WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::DAYS,
      /*start_year=*/2024, /*start_month=*/1, /*start_day=*/1);
  ExampleQueryResult query_result =
      CreateExampleQueryResult({"2024-01-01T10:00:00"}, {1});

  EXPECT_THAT(SplitResultsByPrivacyId(CreateExampleQuery(), query_result,
                                      config, "test_source"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid event time format")));
}

TEST(PrivacyIdUtilsTest, SplitResultsByPrivacyIdAlreadyHasPrivacyId) {
  PrivacyIdConfig config = CreatePrivacyIdConfig(
      /*window_size=*/1,
      WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::DAYS,
      /*start_year=*/2024, /*start_month=*/1, /*start_day=*/1);
  ExampleQueryResult query_result =
      AddPrivacyId(CreateExampleQueryResult({"2024-01-01T10:00:00+00:00"}, {1}),
                   "existing_id");
  EXPECT_THAT(
      SplitResultsByPrivacyId(CreateExampleQuery(), query_result, config,
                              "test_source"),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("Privacy ID column cannot already exist in the "
                         "example query result")));
}

TEST(PrivacyIdUtilsTest, SplitResultsByPrivacyIdEmptyResult) {
  PrivacyIdConfig config = CreatePrivacyIdConfig(
      /*window_size=*/1,
      WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::DAYS,
      /*start_year=*/2024, /*start_month=*/1, /*start_day=*/1);
  ExampleQueryResult query_result = CreateExampleQueryResult({}, {});

  absl::StatusOr<SplitResults> split_results = SplitResultsByPrivacyId(
      CreateExampleQuery(), query_result, config, "test_source");
  ASSERT_OK(split_results);
  EXPECT_THAT(split_results->per_privacy_id_results, testing::IsEmpty());
}

TEST(PrivacyIdUtilsTest, SplitResultsByPrivacyIdNoWindowingSchedule) {
  PrivacyIdConfig config;

  ExampleQueryResult query_result = CreateExampleQueryResult(
      {"2024-01-01T10:15:00+00:00", "2024-01-02T12:00:00+00:00"}, {1, 2});

  absl::StatusOr<SplitResults> split_results = SplitResultsByPrivacyId(
      CreateExampleQuery(), query_result, config, "test_source");

  // Windowing schedule is required, so this should fail.
  EXPECT_THAT(split_results,
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Missing CivilTimeWindowSchedule")));
}

TEST(PrivacyIdUtilsTest, GetNumPrefixBitsValid) {
  EXPECT_THAT(GetNumPrefixBits(1), IsOkAndHolds(0));
  EXPECT_THAT(GetNumPrefixBits(2), IsOkAndHolds(1));
  EXPECT_THAT(GetNumPrefixBits(4), IsOkAndHolds(2));
  EXPECT_THAT(GetNumPrefixBits(8), IsOkAndHolds(3));
  EXPECT_THAT(GetNumPrefixBits(10), IsOkAndHolds(4));
  EXPECT_THAT(GetNumPrefixBits(16), IsOkAndHolds(4));
  EXPECT_THAT(GetNumPrefixBits(256), IsOkAndHolds(8));
  EXPECT_THAT(GetNumPrefixBits(1 << 30), IsOkAndHolds(30));
}

TEST(PrivacyIdUtilsTest, GetNumPrefixBitsInvalid) {
  EXPECT_THAT(GetNumPrefixBits(0), StatusIs(absl::StatusCode::kInvalidArgument,
                                            HasSubstr("must be positive")));
  EXPECT_THAT(GetNumPrefixBits(-1), StatusIs(absl::StatusCode::kInvalidArgument,
                                             HasSubstr("must be positive")));
}

TEST(PrivacyIdUtilsTest, GetPartitionKeyNumPrefixBits0) {
  std::string privacy_id = "0123456789abcdef";
  // All privacy IDs are mapped to the same partition key of all zeros.
  absl::StatusOr<int64_t> partition_key = GetPartitionKey(0, privacy_id);
  ASSERT_OK(partition_key);
  EXPECT_EQ(*partition_key, 0);
}

TEST(PrivacyIdUtilsTest, GetPartitionKeyNumPrefixBits1) {
  std::string privacy_id = "0123456789abcdef";
  absl::StatusOr<int64_t> partition_key = GetPartitionKey(1, privacy_id);
  ASSERT_OK(partition_key);
  // Only the most significant bit is preserved.
  EXPECT_EQ(*partition_key & (0xFFFFFFFFFFFFFFFFULL << 63), *partition_key);
  // Ensure the key is not zero, meaning some bit was set.
  EXPECT_NE(*partition_key, 0);
}

TEST(PrivacyIdUtilsTest, GetPartitionKeyNumPrefixBits4) {
  std::string privacy_id = "0123456789abcdef";
  absl::StatusOr<int64_t> partition_key = GetPartitionKey(4, privacy_id);
  ASSERT_OK(partition_key);
  // The first 4 bits are preserved.
  EXPECT_EQ(*partition_key & (0xFFFFFFFFFFFFFFFFULL << 60), *partition_key);
  EXPECT_NE(*partition_key, 0);
}

TEST(PrivacyIdUtilsTest, GetPartitionKeyNumPrefixBits8) {
  std::string privacy_id = "0123456789abcdef";
  absl::StatusOr<int64_t> partition_key = GetPartitionKey(8, privacy_id);
  ASSERT_OK(partition_key);
  // The first byte is preserved.
  EXPECT_EQ(*partition_key & (0xFFFFFFFFFFFFFFFFULL << 56), *partition_key);
  EXPECT_NE(*partition_key, 0);
}

TEST(PrivacyIdUtilsTest, GetPartitionKeyNumPrefixBits64) {
  std::string privacy_id = "0123456789abcdef";
  absl::StatusOr<int64_t> partition_key = GetPartitionKey(64, privacy_id);
  ASSERT_OK(partition_key);
  // All bits are preserved.
  EXPECT_EQ(*partition_key & 0xFFFFFFFFFFFFFFFFULL, *partition_key);
  EXPECT_NE(*partition_key, 0);
}

TEST(PrivacyIdUtilsTest, GetPartitionKeyDifferentIdDifferentKey) {
  std::string privacy_id1 = "0123456789abcdef";
  std::string privacy_id2 = "fedcba9876543210";
  absl::StatusOr<int64_t> partition_key1 = GetPartitionKey(17, privacy_id1);
  ASSERT_OK(partition_key1);
  absl::StatusOr<int64_t> partition_key2 = GetPartitionKey(17, privacy_id2);
  ASSERT_OK(partition_key2);
  EXPECT_NE(*partition_key1, *partition_key2);
}

TEST(PrivacyIdUtilsTest, GetPartitionKeyNumPrefixBitsNegative) {
  EXPECT_THAT(GetPartitionKey(-1, "0123456789abcdef"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("num_prefix_bits must be non-negative")));
}

TEST(PrivacyIdUtilsTest, GetPartitionKeyNumPrefixBitsTooLarge) {
  EXPECT_THAT(GetPartitionKey(65, "0123456789abcdef"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("num_prefix_bits must be <= 64")));
}

}  // namespace
}  // namespace engine
}  // namespace client
}  // namespace fcp
