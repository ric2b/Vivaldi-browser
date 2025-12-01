/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fcp/secagg/server/aes/aes_secagg_server_protocol_impl.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/node_hash_map.h"
#include "absl/status/status.h"
#include "fcp/base/monitoring.h"
#include "fcp/secagg/server/experiments_names.h"
#include "fcp/secagg/server/secagg_scheduler.h"
#include "fcp/secagg/shared/map_of_masks.h"
#include "fcp/secagg/shared/math.h"
#include "fcp/secagg/shared/secagg_vector.h"

namespace {

void AddReduce(
    std::vector<std::unique_ptr<fcp::secagg::SecAggVectorMap>> vector_of_maps,
    fcp::secagg::SecAggUnpackedVectorMap& sum_of_maps, absl::Mutex* mu) {
  FCP_CHECK(!vector_of_maps.empty());
  // Initialize result
  fcp::secagg::SecAggUnpackedVectorMap partial_sum(*vector_of_maps[0]);
  // Add all input vectors together
  for (int i = 1; i < vector_of_maps.size(); ++i) {
    partial_sum.Add(*vector_of_maps[i]);
  }
  // Finally add to the sum of all inputs
  {
    absl::MutexLock lock(mu);
    sum_of_maps.Add(partial_sum);
  }
}

// Initializes a SecAggUnpackedVectorMap object according to a provided input
// vector specification
std::unique_ptr<fcp::secagg::SecAggUnpackedVectorMap> InitializeVectorMap(
    const std::vector<fcp::secagg::InputVectorSpecification>&
        input_vector_specs) {
  auto vector_map = std::make_unique<fcp::secagg::SecAggUnpackedVectorMap>();
  for (const fcp::secagg::InputVectorSpecification& vector_spec :
       input_vector_specs) {
    vector_map->emplace(vector_spec.name(),
                        fcp::secagg::SecAggUnpackedVector(
                            vector_spec.length(), vector_spec.modulus()));
  }
  return vector_map;
}

}  // namespace

namespace fcp {
namespace secagg {

// The number of keys included in a single PRNG job.
static constexpr int kPrngBatchSize = 32;

AsyncToken AesSecAggServerProtocolImpl::SetupMaskedInputCollection() {
  masked_input_ = InitializeVectorMap(input_vector_specs());
  if (experiments()->IsEnabled(kSecAggAsyncRound2Experiment)) {
    masked_input_accumulator_ = scheduler()->CreateAccumulator<Empty>(
        std::make_unique<Empty>(), [](const Empty& a, const Empty& b) {
          return std::make_unique<Empty>();
        });
  }
  return masked_input_accumulator_;
}

std::vector<std::unique_ptr<SecAggVectorMap>>
AesSecAggServerProtocolImpl::TakeMaskedInputQueue() {
  absl::MutexLock lock(&mutex_);
  return std::move(masked_input_queue_);
}

Status AesSecAggServerProtocolImpl::HandleMaskedInputCollectionResponse(
    std::unique_ptr<MaskedInputCollectionResponse> masked_input_response) {
  FCP_CHECK(masked_input_response);
  // Make sure the received vectors match the specification.
  if (masked_input_response->vectors().size() != input_vector_specs().size()) {
    return ::absl::InvalidArgumentError(
        "Masked input does not match input vector specification - "
        "wrong number of vectors.");
  }
  auto& input_vectors = *masked_input_response->mutable_vectors();
  auto checked_masked_vectors = std::make_unique<SecAggVectorMap>();
  for (const InputVectorSpecification& vector_spec : input_vector_specs()) {
    auto masked_vector = input_vectors.find(vector_spec.name());
    if (masked_vector == input_vectors.end()) {
      return ::absl::InvalidArgumentError(
          "Masked input does not match input vector specification - wrong "
          "vector names.");
    }
    // TODO(team): This does not appear to be properly covered by unit
    // tests.
    int bit_width = SecAggVector::GetBitWidth(vector_spec.modulus());
    if (masked_vector->second.encoded_vector().size() !=
        DivideRoundUp(vector_spec.length() * bit_width, 8)) {
      return ::absl::InvalidArgumentError(
          "Masked input does not match input vector specification - vector is "
          "wrong size.");
    }
    checked_masked_vectors->emplace(
        vector_spec.name(),
        SecAggVector(std::move(*masked_vector->second.mutable_encoded_vector()),
                     vector_spec.modulus(), vector_spec.length()));
  }

  if (experiments()->IsEnabled(kSecAggAsyncRound2Experiment)) {
    // If async processing is enabled we queue the client message. Moreover, if
    // the queue we found was empty this means that it has been taken by an
    // asynchronous aggregation task. In that case, we schedule an aggregation
    // task to process the queue that we just initiated, which will happen
    // eventually.
    size_t is_queue_empty;
    {
      absl::MutexLock lock(&mutex_);
      is_queue_empty = masked_input_queue_.empty();
      masked_input_queue_.emplace_back(std::move(checked_masked_vectors));
    }
    if (is_queue_empty) {
      // TODO(team): Abort should handle the situation where `this` has
      // been destructed while the schedule task is still not running, and
      // message_queue_ can't be moved.
      Trace<Round2AsyncWorkScheduled>();
      masked_input_accumulator_->Schedule([&] {
        auto queue = TakeMaskedInputQueue();
        Trace<Round2MessageQueueTaken>(queue.size());
        AddReduce(std::move(queue), *masked_input_, &async_r2_mutex_);
        return std::make_unique<Empty>();
      });
    }
  } else {
    // Sequential processing
    FCP_CHECK(masked_input_);
    masked_input_->Add(*checked_masked_vectors);
  }

  return ::absl::OkStatus();
}

void AesSecAggServerProtocolImpl::FinalizeMaskedInputCollection() {
  if (experiments()->IsEnabled(kSecAggAsyncRound2Experiment)) {
    FCP_CHECK(masked_input_accumulator_->IsIdle());
    // The result isn't needed because the inputs are already added to the
    // masked_input_.
    masked_input_accumulator_->GetResultAndCancel();
  }
}

AsyncToken AesSecAggServerProtocolImpl::StartPrng(
    const PrngWorkItems& work_items,
    std::function<void(Status)> done_callback) {
  FCP_CHECK(done_callback);
  FCP_CHECK(masked_input_);
  auto generators =
      std::vector<std::function<std::unique_ptr<SecAggUnpackedVectorMap>()>>();

  // Break the keys to add or subtract into vectors of size kPrngBatchSize (or
  // less for the last one) and schedule them as tasks.
  for (auto it = work_items.prng_keys_to_add.begin();
       it < work_items.prng_keys_to_add.end(); it += kPrngBatchSize) {
    std::vector<AesKey> batch_prng_keys_to_add;
    std::copy(it,
              std::min(it + kPrngBatchSize, work_items.prng_keys_to_add.end()),
              std::back_inserter(batch_prng_keys_to_add));
    generators.emplace_back([=]() {
      return UnpackedMapOfMasks(batch_prng_keys_to_add, std::vector<AesKey>(),
                                input_vector_specs(), session_id(),
                                *prng_factory());
    });
  }

  for (auto it = work_items.prng_keys_to_subtract.begin();
       it < work_items.prng_keys_to_subtract.end(); it += kPrngBatchSize) {
    std::vector<AesKey> batch_prng_keys_to_subtract;
    std::copy(
        it,
        std::min(it + kPrngBatchSize, work_items.prng_keys_to_subtract.end()),
        std::back_inserter(batch_prng_keys_to_subtract));
    generators.emplace_back([=]() {
      return UnpackedMapOfMasks(
          std::vector<AesKey>(), batch_prng_keys_to_subtract,
          input_vector_specs(), session_id(), *prng_factory());
    });
  }

  auto accumulator = scheduler()->CreateAccumulator<SecAggUnpackedVectorMap>(
      std::move(masked_input_), SecAggUnpackedVectorMap::AddMaps);
  for (const auto& generator : generators) {
    accumulator->Schedule(generator);
  }
  accumulator->SetAsyncObserver([=, accumulator = accumulator.get()]() {
    auto unpacked_map = accumulator->GetResultAndCancel();
    auto packed_map = std::make_unique<SecAggVectorMap>();
    for (auto& entry : *unpacked_map) {
      uint64_t modulus = entry.second.modulus();
      packed_map->emplace(entry.first,
                          SecAggVector(std::move(entry.second), modulus));
    }
    SetResult(std::move(packed_map));
    done_callback(absl::OkStatus());
  });
  return accumulator;
}
}  // namespace secagg
}  // namespace fcp
