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

#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/civil_time.h"
#include "fcp/protos/confidentialcompute/windowing_schedule.pb.h"

namespace fcp {
namespace confidentialcompute {

using ::fcp::confidentialcompute::WindowingSchedule;

namespace {
// Template helper function to calculate the window start for any CivilTime
// type.
template <typename CivilTimeType>
absl::StatusOr<absl::CivilSecond> CalculateWindowStart(
    const WindowingSchedule::CivilTimeWindowSchedule& schedule,
    absl::CivilSecond event_civil_second) {
  const auto& start_date = schedule.start_date();
  const auto& start_time = schedule.start_time();
  CivilTimeType schedule_start(start_date.year(), start_date.month(),
                               start_date.day(), start_time.hours(),
                               start_time.minutes(), start_time.seconds());
  CivilTimeType event_civil_time(event_civil_second);

  if (event_civil_time < schedule_start) {
    return absl::InvalidArgumentError(
        "Event time is before the schedule start");
  }
  // Calculate the number of windows that elapsed between the schedule start
  // and the event time.
  absl::civil_diff_t diff = event_civil_time - schedule_start;
  int64_t window_size = schedule.size().size();
  absl::civil_diff_t window_index = diff / window_size;
  CivilTimeType window_start = schedule_start + window_index * window_size;
  return absl::CivilSecond(window_start);
}

}  // namespace

absl::StatusOr<WindowingSchedule::CivilTimeWindowSchedule>
ValidateCivilTimeWindowSchedule(
    WindowingSchedule::CivilTimeWindowSchedule schedule) {
  if (schedule.time_zone_scheme() !=
      WindowingSchedule::CivilTimeWindowSchedule::IGNORE) {
    return absl::UnimplementedError(
        "Only IGNORE time zone scheme is supported");
  }
  if (schedule.size().size() != schedule.shift().size() ||
      schedule.size().unit() != schedule.shift().unit()) {
    return absl::UnimplementedError("Only tumbling windows are supported");
  }
  if (schedule.size().size() <= 0) {
    return absl::InvalidArgumentError("Window size must be positive");
  }
  return schedule;
}

absl::StatusOr<absl::CivilSecond> GetTimeWindowStart(
    WindowingSchedule::CivilTimeWindowSchedule schedule,
    absl::CivilSecond event_civil_second) {
  // Validate the schedule and return an error if it is invalid.
  absl::StatusOr<WindowingSchedule::CivilTimeWindowSchedule> schedule_new =
      ValidateCivilTimeWindowSchedule(schedule);
  if (!schedule_new.ok()) {
    return schedule_new.status();
  }
  schedule = schedule_new.value();

  const auto& start_date = schedule.start_date();

  switch (schedule.size().unit()) {
    case WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::HOURS: {
      const auto& start_time = schedule.start_time();
      if (start_time.minutes() != 0 || start_time.seconds() != 0 ||
          start_time.nanos() != 0) {
        return absl::InvalidArgumentError(
            "Start time minutes, seconds, and nanos must be 0 for HOUR "
            "windows");
      }
      return CalculateWindowStart<absl::CivilHour>(schedule,
                                                   event_civil_second);
    }
    case WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::DAYS:
      if (schedule.has_start_time()) {
        return absl::InvalidArgumentError(
            "Start time must be unset for DAY windows");
      }
      return CalculateWindowStart<absl::CivilDay>(schedule, event_civil_second);
    case WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::MONTHS:
      if (start_date.day() != 0 || schedule.has_start_time()) {
        return absl::InvalidArgumentError(
            "Start date day and start time must be unset for MONTH windows");
      }
      return CalculateWindowStart<absl::CivilMonth>(schedule,
                                                    event_civil_second);
    case WindowingSchedule::CivilTimeWindowSchedule::TimePeriod::YEARS:
      if (start_date.month() != 0 || start_date.day() != 0 ||
          schedule.has_start_time()) {
        return absl::InvalidArgumentError(
            "Start date month, day, and time must be unset for YEAR windows");
      }
      return CalculateWindowStart<absl::CivilYear>(schedule,
                                                   event_civil_second);
    default:
      return absl::UnimplementedError("Unsupported windowing schedule unit");
  }
}

}  //  namespace confidentialcompute
}  //  namespace fcp
