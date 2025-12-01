/*
 * Copyright 2022 Google LLC
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
#include "fcp/client/federated_select.h"

#include <atomic>
#include <cstdint>
#include <deque>
#include <filesystem>  // NOLINT(build/c++17)
#include <fstream>
#include <functional>
#include <ios>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "fcp/base/wall_clock_stopwatch.h"
#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/engine/example_iterator_factory.h"
#include "fcp/client/files.h"
#include "fcp/client/http/http_client.h"
#include "fcp/client/http/in_memory_request_response.h"
#include "fcp/client/interruptible_runner.h"
#include "fcp/client/log_manager.h"
#include "fcp/client/simple_task_environment.h"
#include "fcp/protos/plan.pb.h"

namespace fcp {
namespace client {

using fcp::client::http::HttpClient;
using fcp::client::http::InMemoryHttpResponse;
using fcp::client::http::UriOrInlineData;
using ::google::internal::federated::plan::SlicesSelector;

namespace {

// A Federated Select `ExampleIteratorFactory` that fails all queries.
class DisabledFederatedSelectExampleIteratorFactory
    : public FederatedSelectExampleIteratorFactory {
 public:
  explicit DisabledFederatedSelectExampleIteratorFactory(
      LogManager* log_manager)
      : log_manager_(*log_manager) {}

  // Will fetch the slice data via HTTP and return an error if any of the
  // slice fetch requests failed.
  absl::StatusOr<std::unique_ptr<ExampleIterator>> CreateExampleIterator(
      const ::google::internal::federated::plan::ExampleSelector&
          example_selector) override {
    log_manager_.LogDiag(
        ProdDiagCode::FEDSELECT_SLICE_HTTP_FETCH_REQUESTED_BUT_DISABLED);
    return absl::InvalidArgumentError("Federated Select is disabled.");
  }

 private:
  LogManager& log_manager_;
};

absl::StatusOr<std::deque<absl::Cord>> FetchSlicesViaHttp(
    const SlicesSelector& slices_selector, absl::string_view uri_template,
    HttpClient& http_client, InterruptibleRunner& interruptible_runner,
    int64_t* bytes_received_acc, int64_t* bytes_sent_acc) {
  std::vector<UriOrInlineData> resources;
  for (int32_t slice_key : slices_selector.keys()) {
    std::string slice_uri = absl::StrReplaceAll(
        // Note that `served_at_id` is documented to not require URL-escaping,
        // so we don't apply any here.
        uri_template, {{"{served_at_id}", slices_selector.served_at_id()},
                       {"{key_base10}", absl::StrCat(slice_key)}});

    resources.push_back(
        UriOrInlineData::CreateUri(slice_uri, "", absl::ZeroDuration()));
  }

  // Perform the requests.
  absl::StatusOr<std::vector<absl::StatusOr<InMemoryHttpResponse>>>
      slice_fetch_result = http::FetchResourcesInMemory(
          http_client, interruptible_runner, std::move(resources),
          bytes_received_acc, bytes_sent_acc,
          // TODO(team): Enable caching for federated select slices.
          /*resource_cache=*/nullptr,
          // Retries are disabled for federated select slices.
          /*clock=*/nullptr, /*bit_gen=*/nullptr, /*retry_max_attempts=*/0,
          /*retry_delay_ms=*/0);

  // Check whether issuing the requests failed as a whole (generally indicating
  // a programming error).
  if (!slice_fetch_result.ok()) {
    return absl::InternalError(absl::StrCat(
        "Failed to perform HTTP requests (URI template: ", uri_template,
        "): ", absl::StatusCodeToString(slice_fetch_result.status().code())));
  }

  std::deque<absl::Cord> slices;
  for (const absl::StatusOr<InMemoryHttpResponse>& http_response :
       *slice_fetch_result) {
    if (!http_response.ok()) {
      return absl::UnavailableError(absl::StrCat(
          "Slice fetch request failed (URI template: ", uri_template,
          "): ", absl::StatusCodeToString(http_response.status().code())));
    }
    slices.push_back(http_response->body);
  }
  return slices;
}

// A Federated Select `ExampleIteratorFactory` that, upon creation of an
// iterator, fetches the slice data via HTTP, buffers it in-memory, and then
// exposes it to the plan via an `InMemoryFederatedSelectExampleIterator`.
class HttpFederatedSelectExampleIteratorFactory
    : public FederatedSelectExampleIteratorFactory {
 public:
  HttpFederatedSelectExampleIteratorFactory(
      LogManager* log_manager, Files* files, HttpClient* http_client,
      InterruptibleRunner* interruptible_runner, absl::string_view uri_template,
      std::atomic<int64_t>& bytes_sent_acc,
      std::atomic<int64_t>& bytes_received_acc,
      WallClockStopwatch* network_stopwatch)
      : log_manager_(*log_manager),
        files_(*files),
        http_client_(*http_client),
        interruptible_runner_(*interruptible_runner),
        uri_template_(uri_template),
        bytes_sent_acc_(bytes_sent_acc),
        bytes_received_acc_(bytes_received_acc),
        network_stopwatch_(*network_stopwatch) {}

  // Will fetch the slice data via HTTP and return an error if any of the slice
  // fetch requests failed.
  absl::StatusOr<std::unique_ptr<ExampleIterator>> CreateExampleIterator(
      const ::google::internal::federated::plan::ExampleSelector&
          example_selector) override;

 private:
  LogManager& log_manager_;
  Files& files_;
  HttpClient& http_client_;
  InterruptibleRunner& interruptible_runner_;
  std::string uri_template_;
  std::atomic<int64_t>& bytes_sent_acc_;
  std::atomic<int64_t>& bytes_received_acc_;
  WallClockStopwatch& network_stopwatch_;
};

absl::StatusOr<std::unique_ptr<ExampleIterator>>
HttpFederatedSelectExampleIteratorFactory::CreateExampleIterator(
    const ::google::internal::federated::plan::ExampleSelector&
        example_selector) {
  SlicesSelector slices_selector;
  if (!example_selector.criteria().UnpackTo(&slices_selector)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unexpected/unparseable selection criteria: ",
                     example_selector.criteria().GetTypeName()));
  }

  log_manager_.LogDiag(ProdDiagCode::FEDSELECT_SLICE_HTTP_FETCH_REQUESTED);

  // Create the temporary scratch file to store the checkpoint data in.
  // Deletion of the file is done in the
  // InMemoryFederatedSelectExampleIterator::Close() method or its destructor.
  absl::StatusOr<std::string> scratch_filename =
      files_.CreateTempFile("slice", ".ckp");

  if (!scratch_filename.ok()) {
    return absl::InternalError(absl::StrCat(
        "Failed to create scratch file for slice data (URI template: ",
        uri_template_,
        "): ", absl::StatusCodeToString(scratch_filename.status().code()), ": ",
        scratch_filename.status().message()));
  }

  // Fetch the slices.
  int64_t bytes_received = 0;
  int64_t bytes_sent = 0;
  absl::StatusOr<std::deque<absl::Cord>> slices;
  {
    auto started_stopwatch = network_stopwatch_.Start();
    slices = FetchSlicesViaHttp(slices_selector, uri_template_, http_client_,
                                interruptible_runner_,
                                /*bytes_received_acc=*/&bytes_received,
                                /*bytes_sent_acc=*/&bytes_sent);
  }
  bytes_sent_acc_ += bytes_sent;
  bytes_received_acc_ += bytes_received;
  if (!slices.ok()) {
    log_manager_.LogDiag(ProdDiagCode::FEDSELECT_SLICE_HTTP_FETCH_FAILED);
    return absl::Status(slices.status().code(),
                        absl::StrCat("Failed to fetch slice data: ",
                                     slices.status().message()));
  }
  log_manager_.LogDiag(ProdDiagCode::FEDSELECT_SLICE_HTTP_FETCH_SUCCEEDED);

  return std::make_unique<InMemoryFederatedSelectExampleIterator>(
      *scratch_filename, std::move(*slices));
}
}  // namespace

DisabledFederatedSelectManager::DisabledFederatedSelectManager(
    LogManager* log_manager)
    : log_manager_(*log_manager) {}

std::unique_ptr<::fcp::client::engine::ExampleIteratorFactory>
DisabledFederatedSelectManager::CreateExampleIteratorFactoryForUriTemplate(
    absl::string_view uri_template) {
  return std::make_unique<DisabledFederatedSelectExampleIteratorFactory>(
      &log_manager_);
}

HttpFederatedSelectManager::HttpFederatedSelectManager(
    LogManager* log_manager, Files* files,
    fcp::client::http::HttpClient* http_client,
    std::function<bool()> should_abort,
    const InterruptibleRunner::TimingConfig& timing_config)
    : log_manager_(*log_manager),
      files_(*files),
      http_client_(*http_client),
      interruptible_runner_(std::make_unique<InterruptibleRunner>(
          log_manager, should_abort, timing_config,
          InterruptibleRunner::DiagnosticsConfig{
              .interrupted = ProdDiagCode::BACKGROUND_TRAINING_INTERRUPT_HTTP,
              .interrupt_timeout =
                  ProdDiagCode::BACKGROUND_TRAINING_INTERRUPT_HTTP_TIMED_OUT,
              .interrupted_extended = ProdDiagCode::
                  BACKGROUND_TRAINING_INTERRUPT_HTTP_EXTENDED_COMPLETED,
              .interrupt_timeout_extended = ProdDiagCode::
                  BACKGROUND_TRAINING_INTERRUPT_HTTP_EXTENDED_TIMED_OUT})) {}

std::unique_ptr<::fcp::client::engine::ExampleIteratorFactory>
HttpFederatedSelectManager::CreateExampleIteratorFactoryForUriTemplate(
    absl::string_view uri_template) {
  // If the server didn't populate the URI template then we can't support any
  // slice fetch requests.
  if (uri_template.empty()) {
    return std::make_unique<DisabledFederatedSelectExampleIteratorFactory>(
        &log_manager_);
  }
  return std::make_unique<HttpFederatedSelectExampleIteratorFactory>(
      &log_manager_, &files_, &http_client_, interruptible_runner_.get(),
      uri_template,
      /*bytes_sent_acc=*/bytes_sent_, /*bytes_received_acc=*/bytes_received_,
      network_stopwatch_.get());
}

absl::StatusOr<std::string> InMemoryFederatedSelectExampleIterator::Next() {
  absl::MutexLock lock(&mutex_);

  if (slices_.empty()) {
    // Eagerly delete the scratch file, since we won't need it anymore.
    std::filesystem::remove(scratch_filename_);
    return absl::OutOfRangeError("end of iterator reached");
  }

  absl::Cord& slice_data = slices_.front();

  // Write the checkpoint data to the file (truncating any data previously
  // written to the file).
  std::fstream checkpoint_stream(scratch_filename_,
                                 std::ios_base::out | std::ios_base::trunc);
  if (checkpoint_stream.fail()) {
    return absl::InternalError("Failed to write slice to file");
  }
  for (absl::string_view chunk : slice_data.Chunks()) {
    if (!(checkpoint_stream << chunk).good()) {
      return absl::InternalError("Failed to write slice to file");
    }
  }
  checkpoint_stream.close();

  // Remove the slice from the deque, releasing its data from memory.
  slices_.pop_front();

  return scratch_filename_;
}

void InMemoryFederatedSelectExampleIterator::Close() { CleanupInternal(); }

InMemoryFederatedSelectExampleIterator::
    ~InMemoryFederatedSelectExampleIterator() {
  // Remove the scratch file, even if Close() wasn't called first.
  CleanupInternal();
}

void InMemoryFederatedSelectExampleIterator::CleanupInternal() {
  absl::MutexLock lock(&mutex_);
  // Remove the scratch filename, if it hadn't been removed yet.
  slices_.clear();
  std::filesystem::remove(scratch_filename_);
}

}  // namespace client
}  // namespace fcp
