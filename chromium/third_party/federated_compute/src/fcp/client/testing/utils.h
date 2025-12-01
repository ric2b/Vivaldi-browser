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

#ifndef FCP_CLIENT_TESTING_UTILS_H_
#define FCP_CLIENT_TESTING_UTILS_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "fcp/base/platform.h"
#include "fcp/client/files.h"
#include "fcp/client/selector_context.pb.h"
#include "fcp/client/simple_task_environment.h"
#include "fcp/protos/federated_api.pb.h"
#include "fcp/protos/plan.pb.h"

namespace fcp::client::testing {

using google::internal::federated::plan::Dataset;
using google::internal::federated::plan::ExampleSelector;
using google::internal::federated::plan::Plan;
using google::internal::federatedml::v2::RetryWindow;

inline std::string MakeTestFileName(absl::string_view dir,
                                    absl::string_view prefix,
                                    absl::string_view suffix) {
  return ConcatPath(StripTrailingPathSeparator(dir),
                    absl::StrCat(prefix, suffix));
}

// Basic implementation of ExampleIterator for testing purposes.
// It iterates over examples from a given dataset.
class TestExampleIterator : public ExampleIterator {
 public:
  explicit TestExampleIterator(const std::vector<std::string>& dataset)
      : dataset_(dataset), next_index_(0) {}

  absl::StatusOr<std::string> Next() override {
    if (next_index_ >= dataset_.size()) {
      return absl::OutOfRangeError("");
    }
    return dataset_[next_index_++];
  }

  void Close() override {}

 private:
  const std::vector<std::string>& dataset_;
  int32_t next_index_;
};

// Implementation of TaskEnvironment, the interface by which the client plan
// engine interacts with the environment, that allows tests to provide a dataset
// as input and consume the output checkpoint.
class TestTaskEnvironment : public SimpleTaskEnvironment {
 public:
  explicit TestTaskEnvironment(const Dataset::ClientDataset* dataset,
                               const std::string& base_dir);

  absl::StatusOr<std::unique_ptr<ExampleIterator>> CreateExampleIterator(
      const google::internal::federated::plan::ExampleSelector&
          example_selector) override;

  absl::StatusOr<std::unique_ptr<ExampleIterator>> CreateExampleIterator(
      const ExampleSelector& example_selector,
      const SelectorContext& selector_context) override;

  std::string GetBaseDir() override { return base_dir_; }

  std::string GetCacheDir() override { return base_dir_; }

 private:
  bool TrainingConditionsSatisfied() override { return true; }
  std::string base_dir_;
  // Key: serialized ExampleSelector, Value: serialized examples.
  absl::flat_hash_map<std::string, std::vector<std::string>> data_;
  std::vector<std::string> default_data_;
};

// Implementation of client file API that creates files in a temporary test
// directory.
class TestFiles : public Files {
 public:
  explicit TestFiles(absl::string_view test_dir) : test_dir_(test_dir) {}
  absl::StatusOr<std::string> CreateTempFile(
      const std::string& prefix, const std::string& suffix) override {
    return MakeTestFileName(test_dir_, prefix, suffix);
  }

 private:
  std::string test_dir_;
};

}  // namespace fcp::client::testing

#endif  // FCP_CLIENT_TESTING_UTILS_H_
