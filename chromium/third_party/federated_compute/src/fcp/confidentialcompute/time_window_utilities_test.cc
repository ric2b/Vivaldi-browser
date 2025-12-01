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
#include "fcp/confidentialcompute/time_window_utilities.h"

#include <optional>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/time/civil_time.h"
#include "fcp/protos/confidentialcompute/windowing_schedule.pb.h"
#include "fcp/testing/testing.h"

namespace fcp {
namespace confidentialcompute {
namespace {

using ::fcp::confidentialcompute::WindowingSchedule;
using ::testing::ExplainMatchResult;
using ::testing::HasSubstr;

MATCHER_P2(StatusIs, expected_code, message_matcher, "") {
  return ExplainMatchResult(IsCode(expected_code), arg, result_listener) &&
         ExplainMatchResult(message_matcher, arg.status().message(),
                            result_listener);
}

MATCHER_P(IsOkAndHolds, m, "") {
  return testing::ExplainMatchResult(IsOk(), arg, result_listener) &&
         testing::ExplainMatchResult(m, arg.value(), result_listener);
}

WindowingSchedule::CivilTimeWindowSchedule CreateSchedule(
    int window_size,
    WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::Unit unit,
    int start_year, int start_month, int start_day,
    std::optional<int> start_hours = std::nullopt,
    std::optional<int> start_minutes = std::nullopt,
    std::optional<int> start_seconds = std::nullopt) {
  WindowingSchedule::CivilTimeWindowSchedule schedule;
  schedule.mutable_size()->set_size(window_size);
  schedule.mutable_size()->set_unit(unit);
  schedule.mutable_shift()->set_size(window_size);
  schedule.mutable_shift()->set_unit(unit);
  if (start_year != 0) schedule.mutable_start_date()->set_year(start_year);
  if (start_month != 0) schedule.mutable_start_date()->set_month(start_month);
  if (start_day != 0) schedule.mutable_start_date()->set_day(start_day);
  if (start_hours.has_value())
    schedule.mutable_start_time()->set_hours(*start_hours);
  if (start_minutes.has_value())
    schedule.mutable_start_time()->set_minutes(*start_minutes);
  if (start_seconds.has_value())
    schedule.mutable_start_time()->set_seconds(*start_seconds);
  schedule.set_time_zone_scheme(
      WindowingSchedule::CivilTimeWindowSchedule::IGNORE);
  return schedule;
}

TEST(TimeWindowUtilitiesTest, GetWindowStartHourly) {
  WindowingSchedule::CivilTimeWindowSchedule schedule = CreateSchedule(
      /*window_size=*/6,
      WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::HOURS,
      /*start_year=*/2024, /*start_month=*/1,
      /*start_day=*/1, /*start_hours=*/0);
  EXPECT_THAT(
      GetTimeWindowStart(schedule, absl::CivilSecond(2024, 1, 1, 13, 5, 0)),
      IsOkAndHolds(absl::CivilSecond(2024, 1, 1, 12, 0, 0)));
  EXPECT_THAT(
      GetTimeWindowStart(schedule, absl::CivilSecond(2024, 1, 1, 0, 0, 0)),
      IsOkAndHolds(absl::CivilSecond(2024, 1, 1, 0, 0, 0)));
}

TEST(TimeWindowUtilitiesTest, GetWindowStartDaily) {
  WindowingSchedule::CivilTimeWindowSchedule schedule = CreateSchedule(
      /*window_size=*/1,
      WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::DAYS,
      /*start_year=*/2024, /*start_month=*/1, /*start_day=*/1);
  EXPECT_THAT(
      GetTimeWindowStart(schedule, absl::CivilSecond(2024, 1, 5, 13, 5, 0)),
      IsOkAndHolds(absl::CivilSecond(2024, 1, 5, 0, 0, 0)));
}

TEST(TimeWindowUtilitiesTest, GetWindowStartMonthly) {
  WindowingSchedule::CivilTimeWindowSchedule schedule = CreateSchedule(
      /*window_size=*/1,
      WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::MONTHS,
      /*start_year=*/2024, /*start_month=*/1, /*start_day=*/0);
  EXPECT_THAT(
      GetTimeWindowStart(schedule, absl::CivilSecond(2024, 3, 5, 13, 5, 0)),
      IsOkAndHolds(absl::CivilSecond(2024, 3, 1, 0, 0, 0)));
}

TEST(TimeWindowUtilitiesTest, GetWindowStartYearly) {
  WindowingSchedule::CivilTimeWindowSchedule schedule = CreateSchedule(
      /*window_size=*/1,
      WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::YEARS,
      /*start_year=*/2024, /*start_month=*/0, /*start_day=*/0);
  EXPECT_THAT(
      GetTimeWindowStart(schedule, absl::CivilSecond(2025, 3, 5, 13, 5, 0)),
      IsOkAndHolds(absl::CivilSecond(2025, 1, 1, 0, 0, 0)));
}

TEST(TimeWindowUtilitiesTest, GetWindowStartEventBeforeStart) {
  WindowingSchedule::CivilTimeWindowSchedule schedule = CreateSchedule(
      /*window_size=*/1,
      WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::DAYS,
      /*start_year=*/2024, /*start_month=*/1, /*start_day=*/10);
  EXPECT_THAT(
      GetTimeWindowStart(schedule, absl::CivilSecond(2024, 1, 5, 13, 5, 0)),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("Event time is before the schedule start")));
}

TEST(TimeWindowUtilitiesTest, InvalidStartTimeForHourWindow) {
  WindowingSchedule::CivilTimeWindowSchedule schedule = CreateSchedule(
      /*window_size=*/1,
      WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::HOURS,
      /*start_year=*/2024, /*start_month=*/1, /*start_day=*/1,
      /*start_hours=*/0,
      /*start_minutes=*/1, /*start_seconds=*/0);  // Minutes not 0
  EXPECT_THAT(
      GetTimeWindowStart(schedule, absl::CivilSecond(2024, 1, 1, 13, 5, 0)),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("Start time minutes, seconds, and nanos must "
                         "be 0 for HOUR windows")));
}

TEST(TimeWindowUtilitiesTest, InvalidStartTimeForDayWindow) {
  WindowingSchedule::CivilTimeWindowSchedule schedule = CreateSchedule(
      /*window_size=*/1,
      WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::DAYS,
      /*start_year=*/2024, /*start_month=*/1, /*start_day=*/1,
      /*start_hours=*/1,
      /*start_minutes=*/0, /*start_seconds=*/0);  // Hours not 0
  EXPECT_THAT(
      GetTimeWindowStart(schedule, absl::CivilSecond(2024, 1, 1, 13, 5, 0)),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("Start time must be unset for DAY windows")));
}

TEST(TimeWindowUtilitiesTest, InvalidStartTimeForMonthWindow) {
  WindowingSchedule::CivilTimeWindowSchedule schedule = CreateSchedule(
      /*window_size=*/1,
      WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::MONTHS,
      /*start_year=*/2024, /*start_month=*/1,
      /*start_day=*/2);  // Day not 0
  EXPECT_THAT(
      GetTimeWindowStart(schedule, absl::CivilSecond(2024, 1, 1, 13, 5, 0)),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("Start date day and start time must be unset "
                         "for MONTH windows")));
}

TEST(TimeWindowUtilitiesTest, InvalidStartTimeForYearWindow) {
  WindowingSchedule::CivilTimeWindowSchedule schedule = CreateSchedule(
      /*window_size=*/1,
      WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::YEARS,
      /*start_year=*/2024, /*start_month=*/2,
      /*start_day=*/0);  // Month not 0
  EXPECT_THAT(
      GetTimeWindowStart(schedule, absl::CivilSecond(2024, 1, 1, 13, 5, 0)),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("Start date month, day, and time must be "
                         "unset for YEAR windows")));
}

TEST(TimeWindowUtilitiesTest, NonTumblingWindow) {
  WindowingSchedule::CivilTimeWindowSchedule schedule = CreateSchedule(
      /*window_size=*/1,
      WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::DAYS,
      /*start_year=*/2024, /*start_month=*/1, /*start_day=*/1);
  schedule.mutable_shift()->set_size(2);
  EXPECT_THAT(
      GetTimeWindowStart(schedule, absl::CivilSecond(2024, 1, 1, 13, 5, 0)),
      StatusIs(absl::StatusCode::kUnimplemented,
               HasSubstr("Only tumbling windows are supported")));
}

TEST(TimeWindowUtilitiesTest, InvalidWindowSize) {
  WindowingSchedule::CivilTimeWindowSchedule schedule = CreateSchedule(
      /*window_size=*/0,
      WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::DAYS,
      /*start_year=*/2024, /*start_month=*/1, /*start_day=*/1);
  EXPECT_THAT(
      GetTimeWindowStart(schedule, absl::CivilSecond(2024, 1, 1, 13, 5, 0)),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("Window size must be positive")));

  schedule = CreateSchedule(
      /*window_size=*/-1,
      WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::DAYS,
      /*start_year=*/2024, /*start_month=*/1, /*start_day=*/1);
  EXPECT_THAT(
      GetTimeWindowStart(schedule, absl::CivilSecond(2024, 1, 1, 13, 5, 0)),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("Window size must be positive")));
}

TEST(TimeWindowUtilitiesTest, ValidateCivilTimeWindowScheduleValid) {
  WindowingSchedule::CivilTimeWindowSchedule schedule = CreateSchedule(
      /*window_size=*/1,
      WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::DAYS,
      /*start_year=*/2024, /*start_month=*/1, /*start_day=*/1);
  EXPECT_OK(ValidateCivilTimeWindowSchedule(schedule));
}

TEST(TimeWindowUtilitiesTest, ValidateCivilTimeWindowScheduleInvalidTimeZone) {
  WindowingSchedule::CivilTimeWindowSchedule schedule = CreateSchedule(
      /*window_size=*/1,
      WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::DAYS,
      /*start_year=*/2024, /*start_month=*/1, /*start_day=*/1);
  // Setting an invalid enum value to test the case where time_zone_scheme is
  // not IGNORE.
  schedule.set_time_zone_scheme(
      static_cast<WindowingSchedule::CivilTimeWindowSchedule::TimeZoneScheme>(
          -1));
  EXPECT_THAT(ValidateCivilTimeWindowSchedule(schedule),
              StatusIs(absl::StatusCode::kUnimplemented,
                       HasSubstr("Only IGNORE time zone scheme is supported")));
}

TEST(TimeWindowUtilitiesTest, ValidateCivilTimeWindowScheduleNonTumbling) {
  WindowingSchedule::CivilTimeWindowSchedule schedule = CreateSchedule(
      /*window_size=*/1,
      WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::DAYS,
      /*start_year=*/2024, /*start_month=*/1, /*start_day=*/1);
  schedule.mutable_shift()->set_size(2);
  EXPECT_THAT(ValidateCivilTimeWindowSchedule(schedule),
              StatusIs(absl::StatusCode::kUnimplemented,
                       HasSubstr("Only tumbling windows are supported")));
}

TEST(TimeWindowUtilitiesTest, ValidateCivilTimeWindowScheduleInvalidSize) {
  WindowingSchedule::CivilTimeWindowSchedule schedule = CreateSchedule(
      /*window_size=*/0,
      WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::DAYS,
      /*start_year=*/2024, /*start_month=*/1, /*start_day=*/1);
  EXPECT_THAT(ValidateCivilTimeWindowSchedule(schedule),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Window size must be positive")));

  schedule = CreateSchedule(
      /*window_size=*/-1,
      WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::DAYS,
      /*start_year=*/2024, /*start_month=*/1, /*start_day=*/1);
  EXPECT_THAT(ValidateCivilTimeWindowSchedule(schedule),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Window size must be positive")));
}

}  //  namespace
}  //  namespace confidentialcompute
}  //  namespace fcp
