/*
 * Copyright 2020 Google LLC
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

#ifndef FCP_CLIENT_FAKE_LOG_MANAGER_H_
#define FCP_CLIENT_FAKE_LOG_MANAGER_H_

#include <cstdint>
#include <string>

#include "fcp/base/monitoring.h"
#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/engine/engine.pb.h"
#include "fcp/client/histogram_counters.pb.h"
#include "fcp/client/log_manager.h"

namespace fcp {
namespace client {

class FakeLogManager : public LogManager {
 public:
  void LogDiag(ProdDiagCode diagCode) override { FCP_LOG(ERROR) << diagCode; }

  void LogDiag(DebugDiagCode diagCode) override { FCP_LOG(ERROR) << diagCode; }

  void LogToLongHistogram(HistogramCounters histogram_counter,
                          int execution_index, int epoch_index,
                          engine::DataSourceType data_source_type,
                          int64_t value) override {}

  void SetModelIdentifier(const std::string& model_identifier) override {}
};

}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_FAKE_LOG_MANAGER_H_
