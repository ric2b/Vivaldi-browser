/*
 * Copyright 2020 Google LLC
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

#ifndef FCP_SECAGG_SERVER_AES_AES_SECAGG_SERVER_PROTOCOL_IMPL_H_
#define FCP_SECAGG_SERVER_AES_AES_SECAGG_SERVER_PROTOCOL_IMPL_H_

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "fcp/secagg/server/secagg_scheduler.h"
#include "fcp/secagg/server/secagg_server_enums.pb.h"
#include "fcp/secagg/server/secagg_server_protocol_impl.h"
#include "fcp/secagg/server/tracing_schema.h"
#include "fcp/secagg/shared/secagg_vector.h"
#include "fcp/tracing/tracing_span.h"

namespace fcp {
namespace secagg {

class AesSecAggServerProtocolImpl
    : public SecAggServerProtocolImpl,
      public std::enable_shared_from_this<AesSecAggServerProtocolImpl> {
 public:
  typedef struct {
  } Empty;
  using MaskedInputAccumulator = Accumulator<Empty>;

  AesSecAggServerProtocolImpl(
      std::unique_ptr<SecretSharingGraph> graph,
      int minimum_number_of_clients_to_proceed,
      std::vector<InputVectorSpecification> input_vector_specs,
      std::unique_ptr<SecAggServerMetricsListener> metrics,
      std::unique_ptr<AesPrngFactory> prng_factory,
      SendToClientsInterface* sender,
      std::unique_ptr<SecAggScheduler> scheduler,
      std::vector<ClientStatus> client_statuses, ServerVariant server_variant,
      std::unique_ptr<ExperimentsInterface> experiments = nullptr)
      : SecAggServerProtocolImpl(
            std::move(graph), minimum_number_of_clients_to_proceed,
            std::move(metrics), std::move(prng_factory), sender,
            std::move(scheduler), std::move(client_statuses),
            std::move(experiments)),
        server_variant_(server_variant),
        input_vector_specs_(std::move(input_vector_specs)) {}

  ServerVariant server_variant() const override { return server_variant_; }

  // Returns one InputVectorSpecification for each input vector which the
  // protocol will aggregate.
  inline const std::vector<InputVectorSpecification>& input_vector_specs()
      const {
    return input_vector_specs_;
  }

  Status InitializeShareKeysRequest(ShareKeysRequest* request) const override {
    return ::absl::OkStatus();
  }

  // TODO(team): Remove this method. This field must be set from
  // inside the protocol implementation.
  void set_masked_input(std::unique_ptr<SecAggUnpackedVectorMap> masked_input) {
    masked_input_ = std::move(masked_input);
  }

  // Takes out ownership the accumulated queue of masked inputs and empties
  // the current queue.
  std::vector<std::unique_ptr<SecAggVectorMap>> TakeMaskedInputQueue();

  AsyncToken SetupMaskedInputCollection() override;

  void FinalizeMaskedInputCollection() override;

  Status HandleMaskedInputCollectionResponse(
      std::unique_ptr<MaskedInputCollectionResponse> masked_input_response)
      override;

  AsyncToken StartPrng(const PrngWorkItems& work_items,
                       std::function<void(Status)> done_callback) override;

 private:
  std::unique_ptr<SecAggUnpackedVectorMap> masked_input_;
  // Protects masked_input_queue_.
  absl::Mutex mutex_;

  // Parallel masked input collection fields
  std::vector<std::unique_ptr<SecAggVectorMap>> masked_input_queue_
      ABSL_GUARDED_BY(mutex_);

  absl::Mutex async_r2_mutex_;
  std::shared_ptr<MaskedInputAccumulator> masked_input_accumulator_;
  ServerVariant server_variant_;
  std::vector<InputVectorSpecification> input_vector_specs_;
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SERVER_AES_AES_SECAGG_SERVER_PROTOCOL_IMPL_H_
