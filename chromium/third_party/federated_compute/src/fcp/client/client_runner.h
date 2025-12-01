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

#ifndef FCP_CLIENT_CLIENT_RUNNER_H_
#define FCP_CLIENT_CLIENT_RUNNER_H_

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <filesystem>  // NOLINT(build/c++17)
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <variant>

#include "google/protobuf/any.pb.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "fcp/client/client_runner_example_data.pb.h"
#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/engine/engine.pb.h"
#include "fcp/client/fake_event_publisher.h"
#include "fcp/client/files.h"
#include "fcp/client/flags.h"
#include "fcp/client/histogram_counters.pb.h"
#include "fcp/client/http/curl/curl_api.h"
#include "fcp/client/http/curl/curl_http_client.h"
#include "fcp/client/http/http_client.h"
#include "fcp/client/log_manager.h"
#include "fcp/client/selector_context.pb.h"
#include "fcp/client/simple_task_environment.h"
#include "fcp/protos/plan.pb.h"

namespace fcp::client {

// A stub implementation of the SimpleTaskEnvironment interface that logs calls
// to stderr and returns canned example data.
class FederatedTaskEnvDepsImpl : public SimpleTaskEnvironment {
 public:
  // Constructs a SimpleTaskEnvironment that will return an example iterator
  // with `num_empty_examples` empty examples.
  explicit FederatedTaskEnvDepsImpl(int num_empty_examples,
                                    std::string test_cert_path = "")
      : examples_(num_empty_examples),
        test_cert_path_(std::move(test_cert_path)) {}

  // Constructs a SimpleTaskEnvironment that will return an example iterator
  // with examples determined by the collection URI.
  explicit FederatedTaskEnvDepsImpl(ClientRunnerExampleData example_data,
                                    std::string test_cert_path = "")
      : examples_(std::move(example_data)),
        test_cert_path_(std::move(test_cert_path)) {}

  std::string GetBaseDir() override {
    return std::filesystem::path(testing::TempDir());
  }

  std::string GetCacheDir() override {
    return std::filesystem::path(testing::TempDir());
  }

  absl::StatusOr<std::unique_ptr<ExampleIterator>> CreateExampleIterator(
      const google::internal::federated::plan::ExampleSelector&
          example_selector) override {
    SelectorContext unused;
    return CreateExampleIterator(example_selector, unused);
  }

  absl::StatusOr<std::unique_ptr<ExampleIterator>> CreateExampleIterator(
      const google::internal::federated::plan::ExampleSelector&
          example_selector,
      const SelectorContext& selector_context) override {
    FCP_CLIENT_LOG_FUNCTION_NAME
        << ":\n\turi: " << example_selector.collection_uri()
        << "\n\ttype: " << example_selector.criteria().type_url();
    if (auto* num_empty_examples = std::get_if<int>(&examples_)) {
      return std::make_unique<FakeExampleIterator>(*num_empty_examples);
    } else if (auto* store = std::get_if<ClientRunnerExampleData>(&examples_)) {
      const auto& examples_map = store->examples_by_collection_uri();
      if (auto it = examples_map.find(example_selector.collection_uri());
          it != examples_map.end()) {
        return std::make_unique<FakeExampleIterator>(&it->second);
      }
      return absl::InvalidArgumentError("no examples for collection_uri");
    }
    return absl::InternalError("unsupported examples variant type");
  }

  std::unique_ptr<fcp::client::http::HttpClient> CreateHttpClient() override {
    return std::make_unique<fcp::client::http::curl::CurlHttpClient>(
        &curl_api_, test_cert_path_);
  }

 private:
  class FakeExampleIterator : public ExampleIterator {
   public:
    explicit FakeExampleIterator(int num_examples)
        : example_list_(nullptr), num_examples_(num_examples) {}
    explicit FakeExampleIterator(
        const ClientRunnerExampleData::ExampleList* examples)
        : example_list_(examples), num_examples_(examples->examples_size()) {}
    absl::StatusOr<std::string> Next() override {
      if (num_examples_served_ >= num_examples_) {
        return absl::OutOfRangeError("");
      }
      std::string example =
          example_list_ ? example_list_->examples(num_examples_served_) : "";
      num_examples_served_++;
      return example;
    }
    void Close() override {}

   private:
    const ClientRunnerExampleData::ExampleList* const example_list_;
    const int num_examples_;
    int num_examples_served_ = 0;
  };

  bool TrainingConditionsSatisfied() override {
    FCP_CLIENT_LOG_FUNCTION_NAME;
    return true;
  }

  const std::variant<int, ClientRunnerExampleData> examples_;
  const std::string test_cert_path_;
  fcp::client::http::curl::CurlApi curl_api_;
};

// An implementation of the Files interface that attempts to create a temporary
// file with the given prefix and suffix in a directory suitable for temporary
// files.
// NB this is a proof-of-concept implementation that does not use existing infra
// such as mkstemps() or std::tmpfile due to the requirements of the existing
// Files API: include prefix, suffix strings in filename; return file path
// instead of file descriptor.
class FilesImpl : public Files {
 public:
  FilesImpl() { std::srand(static_cast<int32_t>(std::time(nullptr))); }

  absl::StatusOr<std::string> CreateTempFile(
      const std::string& prefix, const std::string& suffix) override {
    const auto tmp_dir = std::filesystem::path(testing::TempDir());
    std::filesystem::path candidate_path;
    int fd;
    do {
      candidate_path =
          tmp_dir / absl::StrCat(prefix, std::to_string(std::rand()), suffix);
    } while ((fd = open(candidate_path.c_str(), O_CREAT | O_EXCL | O_RDWR,
                        S_IRWXU)) == -1 &&
             errno == EEXIST);
    close(fd);
    std::ofstream tmp_file(candidate_path);
    if (!tmp_file) {
      return absl::InvalidArgumentError(
          absl::StrCat("could not create file ", candidate_path.string()));
    }
    FCP_CLIENT_LOG_FUNCTION_NAME << ": " << candidate_path;
    return candidate_path.string();
  }
};

// A stub implementation of the LogManager interface that logs invocations to
// stderr.
class LogManagerImpl : public LogManager {
 public:
  void LogDiag(ProdDiagCode diag_code) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << ": " << ProdDiagCode_Name(diag_code);
  }
  void LogDiag(DebugDiagCode diag_code) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << ": " << DebugDiagCode_Name(diag_code);
  }
  void LogToLongHistogram(HistogramCounters histogram_counter, int, int,
                          engine::DataSourceType data_source_type,
                          int64_t value) override {
    FCP_CLIENT_LOG_FUNCTION_NAME
        << ": " << HistogramCounters_Name(histogram_counter) << " <- " << value;
  }

  void SetModelIdentifier(const std::string& model_identifier) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << ":\n\t" << model_identifier;
  }
};

class FlagsImpl : public Flags {
 public:
  int64_t condition_polling_period_millis() const override { return 1000; }
  int64_t tf_execution_teardown_grace_period_millis() const override {
    return 1000;
  }
  int64_t tf_execution_teardown_extended_period_millis() const override {
    return 2000;
  }
  bool log_tensorflow_error_messages() const override { return true; }
};

}  // namespace fcp::client

#endif  // FCP_CLIENT_CLIENT_RUNNER_H_
