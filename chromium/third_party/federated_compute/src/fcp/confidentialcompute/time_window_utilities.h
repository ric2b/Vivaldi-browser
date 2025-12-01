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
#ifndef FCP_CONFIDENTIALCOMPUTE_TIME_WINDOW_UTILITIES_H_
#define FCP_CONFIDENTIALCOMPUTE_TIME_WINDOW_UTILITIES_H_

#include "absl/status/statusor.h"
#include "absl/time/civil_time.h"
#include "fcp/protos/confidentialcompute/windowing_schedule.pb.h"

namespace fcp {
namespace confidentialcompute {

// Get the start of the time window that the event time falls into, given a time
// window schedule. Only supports IGNORE time zone scheme and civil time
// tumbling windows.
absl::StatusOr<absl::CivilSecond> GetTimeWindowStart(
    ::fcp::confidentialcompute::WindowingSchedule::CivilTimeWindowSchedule
        schedule,
    absl::CivilSecond event_civil_second);

// Validates the CivilTimeWindowSchedule, returns an error if it is invalid.
absl::StatusOr<
    ::fcp::confidentialcompute::WindowingSchedule::CivilTimeWindowSchedule>
ValidateCivilTimeWindowSchedule(
    ::fcp::confidentialcompute::WindowingSchedule::CivilTimeWindowSchedule
        schedule);

}  // namespace confidentialcompute
}  // namespace fcp

#endif  // FCP_CONFIDENTIALCOMPUTE_TIME_WINDOW_UTILITIES_H_
