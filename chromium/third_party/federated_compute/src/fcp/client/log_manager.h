/*
 * Copyright 2019 Google LLC
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
#ifndef FCP_CLIENT_LOG_MANAGER_H_
#define FCP_CLIENT_LOG_MANAGER_H_

#include <cstdint>
#include <string>

#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/engine/engine.pb.h"
#include "fcp/client/histogram_counters.pb.h"

namespace fcp {
namespace client {

// An interface used to log "diag codes" - numeric enum values representing some
// state of the code, e.g. a specific source code location being reached, or a
// certain condition being met - to a monitoring backend in the cloud.
class LogManager {
 public:
  virtual ~LogManager() = default;

  // These functions log the given diag code.
  virtual void LogDiag(ProdDiagCode diagCode) = 0;
  virtual void LogDiag(DebugDiagCode diagCode) = 0;
  // This function logs the given value to a long histogram identified by
  // histogram_counter, annotated with the indexes and data_source_type.
  virtual void LogToLongHistogram(HistogramCounters histogram_counter,
                                  int execution_index, int epoch_index,
                                  engine::DataSourceType data_source_type,
                                  int64_t value) = 0;
  void LogToLongHistogram(HistogramCounters histogram_counter, int64_t value) {
    return LogToLongHistogram(histogram_counter, /*execution_index=*/0,
                              /*epoch_index=*/0,
                              engine::DataSourceType::DATASET, value);
  }
  // After calling this function, all subsequently published histogram events
  // will be annotated with the specified model_identifier. This value is
  // typically provided by the federated server.
  //
  // Note that this method may be called multiple times with different values,
  // if over the course of a training session multiple models are executed.
  virtual void SetModelIdentifier(const std::string& model_identifier) = 0;
};

}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_LOG_MANAGER_H_
