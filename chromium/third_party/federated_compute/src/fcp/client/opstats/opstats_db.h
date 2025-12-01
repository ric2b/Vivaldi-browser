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
#ifndef FCP_CLIENT_OPSTATS_OPSTATS_DB_H_
#define FCP_CLIENT_OPSTATS_OPSTATS_DB_H_

#include <functional>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "fcp/protos/opstats.pb.h"

namespace fcp {
namespace client {
namespace opstats {

// Base no-op class for the OpStats database that always contains an empty
// OpStatsSequence and performs no file i/o.
class OpStatsDb {
 public:
  virtual ~OpStatsDb() = default;
  // The returned OpStatsSequence message should contain the operational stats
  // for all runs.  The operational stats for each run is wrapped inside a
  // OperationalStats message, and the OperationalStats messages are ordered
  // sequentially (first run to last run) within OpStatsSequence.
  virtual absl::StatusOr<OpStatsSequence> Read() { return OpStatsSequence(); }

  // OpStatsDb has a Transform method instead of a Write method because
  // OpStatsSequence message already contains the operational stats for every
  // run, and the user only need to update the existing OpStatsSequence message
  // to add/remove/update data. In addition, by having a Transform method allows
  // the implementations to perform atomic read-update-write operations.
  virtual absl::Status Transform(std::function<void(OpStatsSequence&)> func) {
    return absl::OkStatus();
  }
};

}  // namespace opstats
}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_OPSTATS_OPSTATS_DB_H_
