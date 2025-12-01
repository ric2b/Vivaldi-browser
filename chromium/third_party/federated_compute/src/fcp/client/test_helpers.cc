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
#include "fcp/client/test_helpers.h"

#include <fcntl.h>

#include <cstdint>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/engine/common.h"
#include "fcp/client/engine/example_iterator_factory.h"
#include "fcp/client/example_iterator_query_recorder.h"
#include "fcp/client/flags.h"
#include "fcp/client/interruptible_runner.h"
#include "fcp/client/log_manager.h"
#include "fcp/client/opstats/opstats_logger.h"
#include "fcp/client/runner_common.h"
#include "fcp/protos/plan.pb.h"
#include "google/protobuf/message_lite.h"
#include "google/protobuf/repeated_field.h"
#include "google/protobuf/repeated_ptr_field.h"

namespace fcp {
namespace client {

using ::google::internal::federated::plan::Dataset;

bool LoadFileAsString(std::string path, std::string* msg) {
  std::ifstream checkpoint_istream(path);
  if (!checkpoint_istream) {
    return false;
  }
  std::stringstream checkpoint_stream;
  checkpoint_stream << checkpoint_istream.rdbuf();
  *msg = checkpoint_stream.str();
  return true;
}

SimpleExampleIterator::SimpleExampleIterator(
    std::vector<const char*> examples) {
  for (const auto& example : examples) {
    examples_.push_back(std::string(example));
  }
  FCP_CHECK(!examples_.empty()) << "No data was loaded";
}

SimpleExampleIterator::SimpleExampleIterator(Dataset dataset) {
  for (const Dataset::ClientDataset& client_dataset : dataset.client_data()) {
    FCP_CHECK(client_dataset.selected_example_size() == 0)
        << "This constructor can only be used for Dataset protos with unnamed "
           "example data.";
    for (const auto& example : client_dataset.example()) {
      examples_.push_back(example);
    }
  }
  FCP_CHECK(!examples_.empty()) << "No data was loaded";
}

SimpleExampleIterator::SimpleExampleIterator(
    const std::vector<std::string>& examples) {
  for (const auto& example : examples) {
    examples_.push_back(example);
  }
  FCP_CHECK(!examples_.empty()) << "No data was loaded";
}

SimpleExampleIterator::SimpleExampleIterator(Dataset dataset,
                                             absl::string_view collection_uri) {
  for (const Dataset::ClientDataset& client_dataset : dataset.client_data()) {
    FCP_CHECK(client_dataset.selected_example_size() > 0)
        << "This constructor can only be used for Dataset protos with named "
           "example data.";
    for (const Dataset::ClientDataset::SelectedExample& selected_example :
         client_dataset.selected_example()) {
      // Only use those examples whose `ExampleSelector` matches the
      // `collection_uri` argument. Note that the `ExampleSelector`'s selection
      // criteria is ignored/not taken into account here.
      if (selected_example.selector().collection_uri() != collection_uri) {
        continue;
      }
      for (const auto& example : selected_example.example()) {
        examples_.push_back(example);
      }
    }
  }
  FCP_CHECK(!examples_.empty()) << "No data was loaded for " << collection_uri;
}

absl::StatusOr<std::string> SimpleExampleIterator::Next() {
  if (index_ < examples_.size()) {
    return examples_[index_++];
  }
  return absl::OutOfRangeError("");
}

std::string ExtractSingleString(const tensorflow::Example& example,
                                const char key[]) {
  return example.features().feature().at(key).bytes_list().value().at(0);
}

google::protobuf::RepeatedPtrField<std::string> ExtractRepeatedString(
    const tensorflow::Example& example, const char key[]) {
  return example.features().feature().at(key).bytes_list().value();
}

int64_t ExtractSingleInt64(const tensorflow::Example& example,
                           const char key[]) {
  return example.features().feature().at(key).int64_list().value().at(0);
}

google::protobuf::RepeatedField<int64_t> ExtractRepeatedInt64(
    const tensorflow::Example& example, const char key[]) {
  return example.features().feature().at(key).int64_list().value();
}

engine::PlanResult
TestingTensorflowRunner::RunEligibilityEvalPlanWithTensorflowSpec(
    std::vector<engine::ExampleIteratorFactory*> example_iterator_factories,
    std::function<bool()> should_abort, LogManager* log_manager,
    opstats::OpStatsLogger* opstats_logger, const Flags* flags,
    const google::internal::federated::plan::ClientOnlyPlan& client_plan,
    const std::string& checkpoint_input_filename,
    const fcp::client::InterruptibleRunner::TimingConfig& timing_config,
    absl::Time run_plan_start_time, absl::Time reference_time) {
  return delegate_.RunEligibilityEvalPlanWithTensorflowSpec(
      std::move(example_iterator_factories), std::move(should_abort),
      log_manager, opstats_logger, flags, client_plan,
      checkpoint_input_filename, timing_config, run_plan_start_time,
      reference_time);
}

PlanResultAndCheckpointFile TestingTensorflowRunner::RunPlanWithTensorflowSpec(
    std::vector<engine::ExampleIteratorFactory*> example_iterator_factories,
    std::function<bool()> should_abort, LogManager* log_manager,
    opstats::OpStatsLogger* opstats_logger, const Flags* flags,
    ExampleIteratorQueryRecorder* example_iterator_query_recorder,
    const google::internal::federated::plan::ClientOnlyPlan& client_plan,
    const std::string& checkpoint_input_filename,
    const std::string& checkpoint_output_filename,
    const fcp::client::InterruptibleRunner::TimingConfig& timing_config) {
  return delegate_.RunPlanWithTensorflowSpec(
      std::move(example_iterator_factories), std::move(should_abort),
      log_manager, opstats_logger, flags, example_iterator_query_recorder,
      client_plan, checkpoint_input_filename, checkpoint_output_filename,
      timing_config);
}

absl::Status TestingTensorflowRunner::WriteTFV1Checkpoint(
    const std::string& output_checkpoint_filename,
    const std::vector<std::pair<
        google::internal::federated::plan::ExampleQuerySpec::ExampleQuery,
        ExampleQueryResult>>& example_query_results) {
  return delegate_.WriteTFV1Checkpoint(output_checkpoint_filename,
                                       example_query_results);
}

}  // namespace client
}  // namespace fcp
