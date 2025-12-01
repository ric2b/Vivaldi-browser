/*
 * Copyright 2023 Google LLC
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

#include "fcp/client/testing/utils.h"

#include <memory>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "fcp/client/selector_context.pb.h"
#include "fcp/client/simple_task_environment.h"

namespace fcp::client::testing {

TestTaskEnvironment::TestTaskEnvironment(const Dataset::ClientDataset* dataset,
                                         const std::string& base_dir)
    : base_dir_(base_dir) {
  default_data_ = std::vector<std::string>(dataset->example().begin(),
                                           dataset->example().end());
  for (const auto& selected_example : dataset->selected_example()) {
    data_[selected_example.selector().SerializeAsString()] =
        std::vector<std::string>(selected_example.example().begin(),
                                 selected_example.example().end());
  }
}

absl::StatusOr<std::unique_ptr<ExampleIterator>>
TestTaskEnvironment::CreateExampleIterator(
    const google::internal::federated::plan::ExampleSelector&
        example_selector) {
  SelectorContext unused;
  return CreateExampleIterator(example_selector, unused);
}

absl::StatusOr<std::unique_ptr<ExampleIterator>>
TestTaskEnvironment::CreateExampleIterator(
    const ExampleSelector& example_selector,
    const SelectorContext& selector_context) {
  const std::string key = example_selector.SerializeAsString();
  if (data_.contains(key)) {
    return std::make_unique<TestExampleIterator>(data_[key]);
  } else {
    return std::make_unique<TestExampleIterator>(default_data_);
  }
}

}  // namespace fcp::client::testing
