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

/**
 * Tests for the ExternalDataset op.
 *
 * In make_external_dataset_test_graph.py, we generate a GraphDef that connects
 * an ExternalDataset (producing serialized tf.Example protos), through
 * tf.parse_example (parsing each to an int64_t scalar), to a Reduce (sum).
 *
 * Here, we load that graph and try to run it, with some ExternalDataset
 * provided as we call Session::Run. We just need to Run once since Reduce
 * should consume the entire dataset iterator.
 */

#include <fcntl.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "fcp/base/monitoring.h"
#include "fcp/base/random_token.h"
#include "fcp/tensorflow/external_dataset.h"
#include "fcp/tensorflow/test_selector.pb.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/text_format.h"
#include "tensorflow/core/example/example.pb.h"
#include "tensorflow/core/example/feature_util.h"
#include "tensorflow/core/framework/graph.pb.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/tensor_shape.h"
#include "tensorflow/core/framework/tensor_testutil.h"
#include "tensorflow/core/platform/tstring.h"
#include "tensorflow/core/protobuf/error_codes.pb.h"
#include "tensorflow/core/public/session.h"
#include "tensorflow/core/public/session_options.h"
#include "tensorflow/tsl/platform/status.h"

namespace fcp {

using ::testing::Eq;

//
// Constants related to the GraphDef we test with
// See make_external_dataset_test_graph.py
//

char const* const kExampleGraphPath =
    "fcp/tensorflow/external_dataset_test.pbtxt";
char const* const kFeatureName = "val";
char const* const kTokenPlaceholderName = "token";
char const* const kSelectorPlaceholderName = "selector";
char const* const kOutputName = "total:0";

//
// TensorFlow boilerplate
//

tensorflow::GraphDef LoadExampleGraph() {
  int fd = open(kExampleGraphPath, O_RDONLY);
  FCP_CHECK(fd != -1) << "Failed to open the example graph, using path "
                      << kExampleGraphPath;

  google::protobuf::io::FileInputStream fs(fd);
  fs.SetCloseOnDelete(true);

  tensorflow::GraphDef graph;
  bool parsed = google::protobuf::TextFormat::Parse(&fs, &graph);
  FCP_CHECK(parsed) << "Invalid text-format GraphDef";

  return graph;
}

std::unique_ptr<tensorflow::Session> PrepareExampleGraphSession() {
  tensorflow::GraphDef graph = LoadExampleGraph();

  std::unique_ptr<tensorflow::Session> session;
  {
    tensorflow::SessionOptions options;
    tensorflow::Session* raw_session = nullptr;
    absl::Status session_new_status =
        tensorflow::NewSession(options, &raw_session);
    TF_CHECK_OK(session_new_status);
    session = std::unique_ptr<tensorflow::Session>(raw_session);
  }

  absl::Status graph_build_status = session->Create(graph);
  TF_CHECK_OK(graph_build_status);
  return session;
}

tensorflow::Example MakeExample(int64_t value) {
  tensorflow::Example example;
  tensorflow::AppendFeatureValues({value}, kFeatureName, &example);
  return example;
}

std::string SerializeExample(int64_t value) {
  std::string serialized;
  FCP_CHECK(MakeExample(value).SerializeToString(&serialized));
  return serialized;
}

absl::Status RunSession(tensorflow::Session* session, RandomToken dataset_token,
                        tensorflow::Tensor selector,
                        tensorflow::Tensor* output) {
  auto token_tensor =
      tensorflow::test::AsScalar<tensorflow::tstring>(dataset_token.ToString());

  std::vector<tensorflow::Tensor> outputs;
  absl::Status run_status = session->Run({{kTokenPlaceholderName, token_tensor},
                                          {kSelectorPlaceholderName, selector}},
                                         {kOutputName}, {}, &outputs);

  if (run_status.ok() && output) {
    FCP_CHECK(outputs.size() == 1);
    *output = outputs[0];
  }

  return run_status;
}

absl::Status RunSession(tensorflow::Session* session, RandomToken dataset_token,
                        TestSelector const& selector,
                        tensorflow::Tensor* output) {
  std::string selector_str;
  FCP_CHECK(selector.SerializeToString(&selector_str));
  auto selector_tensor =
      tensorflow::test::AsScalar<tensorflow::tstring>(selector_str);
  return RunSession(session, dataset_token, selector_tensor, output);
}

tensorflow::Tensor RunSessionAndGetOutput(tensorflow::Session* session,
                                          RandomToken dataset_token,
                                          TestSelector const& selector) {
  tensorflow::Tensor output;
  absl::Status run_status =
      RunSession(session, dataset_token, selector, &output);
  TF_CHECK_OK(run_status);
  return output;
}

//
// ExternalDataset host object implementations for testing
//

class TestDatasetIterator : public ExternalDatasetIterator {
 public:
  explicit TestDatasetIterator(
      std::shared_ptr<std::vector<int64_t> const> examples,
      int64_t lower_inclusive, int64_t upper_inclusive)
      : examples_(std::move(examples)),
        lower_inclusive_(lower_inclusive),
        upper_inclusive_(upper_inclusive) {}

  absl::StatusOr<std::string> GetNext() final {
    while (index_ < examples_->size()) {
      int64_t ex = examples_->at(index_);
      index_++;

      if (ex >= lower_inclusive_ && ex < upper_inclusive_) {
        return SerializeExample(ex);
      }
    }

    return absl::OutOfRangeError("");
  }

 private:
  std::shared_ptr<std::vector<int64_t> const> examples_;
  int index_ = 0;
  int64_t lower_inclusive_;
  int64_t upper_inclusive_;
};

class TestDatasetProvider
    : public ExternalDatasetProvider::UsingProtoSelector<TestSelector> {
 public:
  explicit TestDatasetProvider(std::vector<int64_t> examples) {
    auto ex = std::make_shared<std::vector<int64_t>>(std::move(examples));
    examples_ = std::move(ex);
  }

  absl::StatusOr<std::unique_ptr<ExternalDataset>> MakeDataset(
      TestSelector selector) final {
    int64_t lower = selector.has_lower_inclusive()
                        ? selector.lower_inclusive().value()
                        : std::numeric_limits<int64_t>::min();
    int64_t upper = selector.has_upper_inclusive()
                        ? selector.upper_inclusive().value()
                        : std::numeric_limits<int64_t>::max();
    auto examples = examples_;
    return ExternalDataset::FromFunction([examples, lower, upper]() {
      return std::make_unique<TestDatasetIterator>(examples, lower, upper);
    });
  }

 private:
  std::shared_ptr<std::vector<int64_t> const> examples_;
};

class FailingIterator : public ExternalDatasetIterator {
 public:
  absl::StatusOr<std::string> GetNext() final {
    return absl::NotFoundError("");
  }
};

class FailingIteratorDatasetProvider
    : public ExternalDatasetProvider::UsingProtoSelector<TestSelector> {
 public:
  absl::StatusOr<std::unique_ptr<ExternalDataset>> MakeDataset(
      TestSelector selector) final {
    return ExternalDataset::FromFunction(
        []() { return std::make_unique<FailingIterator>(); });
  }
};

//
// Actual tests
//

TEST(ExternalDatasetOpTest, RunExampleGraph) {
  std::vector<int64_t> examples{123, 456, 789};

  // Default selector (no filtering)
  TestSelector selector;

  tensorflow::Tensor expected = tensorflow::test::AsTensor<int64_t>(
      {123 + 456 + 789}, tensorflow::TensorShape({1}));

  auto stub = std::make_shared<TestDatasetProvider>(std::move(examples));
  auto stub_reg = ExternalDatasetProviderRegistry::Register(stub);

  auto session = PrepareExampleGraphSession();
  tensorflow::Tensor output =
      RunSessionAndGetOutput(session.get(), stub_reg.token(), selector);

  tensorflow::test::ExpectTensorEqual<int64_t>(output, expected);
}

TEST(ExternalDatasetOpTest, RunExampleGraph_SelectorFilter) {
  std::vector<int64_t> examples{123, 456, 789, 1024};

  TestSelector selector;
  selector.mutable_lower_inclusive()->set_value(124);
  selector.mutable_upper_inclusive()->set_value(1023);

  // Expecting some of the examples to be skipped, due to the filter.
  tensorflow::Tensor expected = tensorflow::test::AsTensor<int64_t>(
      {456 + 789}, tensorflow::TensorShape({1}));

  auto stub = std::make_shared<TestDatasetProvider>(std::move(examples));
  auto stub_reg = ExternalDatasetProviderRegistry::Register(stub);

  auto session = PrepareExampleGraphSession();
  tensorflow::Tensor output =
      RunSessionAndGetOutput(session.get(), stub_reg.token(), selector);

  tensorflow::test::ExpectTensorEqual<int64_t>(output, expected);
}

TEST(ExternalDatasetOpTest, TokenNotFound) {
  TestSelector selector;
  auto session = PrepareExampleGraphSession();
  absl::Status status =
      RunSession(session.get(), RandomToken::Generate(), selector, nullptr);
  EXPECT_THAT(status.code(), Eq(absl::StatusCode::kInvalidArgument));
}

TEST(ExternalDatasetOpTest, FailingIterator) {
  auto stub = std::make_shared<FailingIteratorDatasetProvider>();
  auto stub_reg = ExternalDatasetProviderRegistry::Register(stub);

  TestSelector selector;

  auto session = PrepareExampleGraphSession();
  absl::Status status =
      RunSession(session.get(), stub_reg.token(), selector, nullptr);
  EXPECT_THAT(status.code(), Eq(absl::StatusCode::kNotFound));
}

TEST(ExternalDatasetOpTest, RunExampleGraph_InvalidSelector) {
  std::vector<int64_t> examples{123};

  // This is interpreted as a varint. The MSB is set, so it asks for another
  // byte (but there aren't any).
  std::string bad_selector = "\xFF";
  tensorflow::Tensor bad_selector_tensor =
      tensorflow::test::AsScalar<tensorflow::tstring>(bad_selector);
  auto stub = std::make_shared<TestDatasetProvider>(std::move(examples));
  auto stub_reg = ExternalDatasetProviderRegistry::Register(stub);

  auto session = PrepareExampleGraphSession();
  absl::Status status =
      RunSession(session.get(), stub_reg.token(), bad_selector_tensor, nullptr);
  EXPECT_THAT(status.code(), Eq(absl::StatusCode::kInvalidArgument));
}

}  // namespace fcp
