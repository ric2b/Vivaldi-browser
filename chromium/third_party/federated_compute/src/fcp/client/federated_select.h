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
#ifndef FCP_CLIENT_FEDERATED_SELECT_H_
#define FCP_CLIENT_FEDERATED_SELECT_H_

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "fcp/base/wall_clock_stopwatch.h"
#include "fcp/client/engine/example_iterator_factory.h"
#include "fcp/client/files.h"
#include "fcp/client/http/http_client.h"
#include "fcp/client/interruptible_runner.h"
#include "fcp/client/log_manager.h"
#include "fcp/client/simple_task_environment.h"
#include "fcp/client/stats.h"
#include "fcp/protos/plan.pb.h"

namespace fcp {
namespace client {

// The example query collection URI via which slice fetch requests will arrive.
inline static constexpr char kFederatedSelectCollectionUri[] =
    "internal:/federated_select";

// An interface via which a Federated Select `ExampleIteratorFactory` can be
// created. Each factory is expected to fetch slice data using the given
// `uri_template`, and to then serve the slice data by writing it to a file and
// by then returning that filename as a tf.Example to the plan.
class FederatedSelectManager {
 public:
  virtual std::unique_ptr<::fcp::client::engine::ExampleIteratorFactory>
  CreateExampleIteratorFactoryForUriTemplate(
      absl::string_view uri_template) = 0;

  // The best estimate of the over-the-wire bytes downloaded and uploadeded over
  // the network, and the total duration of wall clock time spent waiting on
  // network requests.

  // Note that if two different slice fetches are in flight from different
  // threads, this should measure just the wall clock time spent completing both
  // sets of fetches (i.e. it should not double-count the wall clock time by
  // summing each per-thread duration individually).
  //
  // If possible, this estimate should also include time spent decompressing
  // payloads after reading them from the network.
  virtual NetworkStats GetNetworkStats() = 0;

  virtual ~FederatedSelectManager() = default;
};

// An base class for `ExampleIteratorFactory` implementations that can handle
// Federated Select example queries.
class FederatedSelectExampleIteratorFactory
    : public ::fcp::client::engine::ExampleIteratorFactory {
 public:
  bool CanHandle(const ::google::internal::federated::plan::ExampleSelector&
                     example_selector) override {
    return example_selector.collection_uri() == kFederatedSelectCollectionUri;
  }

  bool ShouldCollectStats() override {
    // Federated Select example queries should not be recorded in the OpStats
    // DB, since the fact that Federated Select uses the example iterator
    // interface is an internal implementation detail.
    return false;
  }
};

class DisabledFederatedSelectManager : public FederatedSelectManager {
 public:
  explicit DisabledFederatedSelectManager(LogManager* log_manager);

  std::unique_ptr<::fcp::client::engine::ExampleIteratorFactory>
  CreateExampleIteratorFactoryForUriTemplate(
      absl::string_view uri_template) override;

  NetworkStats GetNetworkStats() override { return NetworkStats(); }

 private:
  LogManager& log_manager_;
};

// A FederatedSelectManager implementation that actually issues HTTP requests to
// fetch slice data (i.e. the "real" implementation).
class HttpFederatedSelectManager : public FederatedSelectManager {
 public:
  HttpFederatedSelectManager(
      LogManager* log_manager, Files* files,
      fcp::client::http::HttpClient* http_client,
      std::function<bool()> should_abort,
      const InterruptibleRunner::TimingConfig& timing_config);

  std::unique_ptr<::fcp::client::engine::ExampleIteratorFactory>
  CreateExampleIteratorFactoryForUriTemplate(
      absl::string_view uri_template) override;

  NetworkStats GetNetworkStats() override {
    return {.bytes_downloaded = bytes_received_.load(),
            .bytes_uploaded = bytes_sent_.load(),
            .network_duration = network_stopwatch_->GetTotalDuration()};
  }

 private:
  LogManager& log_manager_;
  Files& files_;
  std::atomic<int64_t> bytes_sent_ = 0;
  std::atomic<int64_t> bytes_received_ = 0;
  std::unique_ptr<WallClockStopwatch> network_stopwatch_ =
      WallClockStopwatch::Create();
  fcp::client::http::HttpClient& http_client_;
  std::unique_ptr<InterruptibleRunner> interruptible_runner_;
};

// A Federated Select ExampleIterator that simply returns slice data that is
// already in-memory.
class InMemoryFederatedSelectExampleIterator : public ExampleIterator {
 public:
  // Each time another slice is requested by a call to Next(), the slice data at
  // the front of the `slices` deque will be written to the `scratch_filename`
  // and the filename will be returned as the example data. The scratch file
  // will be deleted at the end of the iterator, or when the iterator is closed.
  InMemoryFederatedSelectExampleIterator(std::string scratch_filename,
                                         std::deque<absl::Cord> slices)
      : scratch_filename_(scratch_filename), slices_(std::move(slices)) {}
  absl::StatusOr<std::string> Next() override;
  void Close() override;

  ~InMemoryFederatedSelectExampleIterator() override;

 private:
  void CleanupInternal() ABSL_LOCKS_EXCLUDED(mutex_);

  std::string scratch_filename_;

  absl::Mutex mutex_;
  std::deque<absl::Cord> slices_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_FEDERATED_SELECT_H_
