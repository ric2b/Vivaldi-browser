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

#ifndef FCP_SECAGG_TESTING_SERVER_TEST_SECAGG_EXPERIMENTS_H_
#define FCP_SECAGG_TESTING_SERVER_TEST_SECAGG_EXPERIMENTS_H_

#include <set>
#include <string>

#include "fcp/secagg/server/experiments_interface.h"

namespace fcp {
namespace secagg {

// Defines an experiment class to set secagg experiments.
class TestSecAggExperiment : public ExperimentsInterface {
 public:
  explicit TestSecAggExperiment(
      const std::set<std::string>& enabled_experiment_names)
      : enabled_experiment_names_(enabled_experiment_names) {}
  explicit TestSecAggExperiment(std::string enabled_experiment_name) {
    enabled_experiment_names_ =
        std::set<std::string>({enabled_experiment_name});
  }
  explicit TestSecAggExperiment() = default;
  bool IsEnabled(absl::string_view experiment_name) override {
    return enabled_experiment_names_.find(static_cast<std::string>(
               experiment_name)) != enabled_experiment_names_.end();
  }

 private:
  std::set<std::string> enabled_experiment_names_;
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_TESTING_SERVER_TEST_SECAGG_EXPERIMENTS_H_
