/*
 * Copyright 2018 Google LLC
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

#ifndef FCP_SECAGG_CLIENT_SECAGG_CLIENT_R0_ADVERTISE_KEYS_INPUT_SET_STATE_H_
#define FCP_SECAGG_CLIENT_SECAGG_CLIENT_R0_ADVERTISE_KEYS_INPUT_SET_STATE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "fcp/base/monitoring.h"
#include "fcp/secagg/client/secagg_client_alive_base_state.h"
#include "fcp/secagg/client/secagg_client_state.h"
#include "fcp/secagg/client/send_to_server_interface.h"
#include "fcp/secagg/client/state_transition_listener_interface.h"
#include "fcp/secagg/shared/aes_prng_factory.h"
#include "fcp/secagg/shared/input_vector_specification.h"
#include "fcp/secagg/shared/prng.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"

namespace fcp {
namespace secagg {

// This class represents the client's Round 0: Advertise Keys state with the
// input already set. This state should transition to the Round 1: Share Keys
// (Input Set) state, but can also transition directly to the Completed or
// Aborted states.

class SecAggClientR0AdvertiseKeysInputSetState
    : public SecAggClientAliveBaseState {
 public:
  SecAggClientR0AdvertiseKeysInputSetState(
      uint32_t max_neighbors_expected,
      uint32_t minimum_surviving_neighbors_for_reconstruction,
      std::unique_ptr<SecAggVectorMap> input_map,
      std::unique_ptr<std::vector<InputVectorSpecification> >
          input_vector_specs,
      std::unique_ptr<SecurePrng> prng,
      std::unique_ptr<SendToServerInterface> sender,
      std::unique_ptr<StateTransitionListenerInterface> transition_listener,

      std::unique_ptr<AesPrngFactory> prng_factory,
      AsyncAbort* async_abort = nullptr);

  ~SecAggClientR0AdvertiseKeysInputSetState() override;

  StatusOr<std::unique_ptr<SecAggClientState> > Start() override;

  StatusOr<std::unique_ptr<SecAggClientState> > HandleMessage(
      const ServerToClientWrapperMessage& message) override;

  // Returns the name of this state, "R0_ADVERTISE_KEYS_INPUT_SET".
  std::string StateName() const override;

 private:
  const uint32_t max_neighbors_expected_;
  const uint32_t minimum_surviving_neighbors_for_reconstruction_;
  std::unique_ptr<SecAggVectorMap> input_map_;
  std::unique_ptr<std::vector<InputVectorSpecification> > input_vector_specs_;
  std::unique_ptr<SecurePrng> prng_;
  std::unique_ptr<AesPrngFactory> prng_factory_;
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_CLIENT_SECAGG_CLIENT_R0_ADVERTISE_KEYS_INPUT_SET_STATE_H_
