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

#ifndef FCP_SECAGG_CLIENT_SECAGG_CLIENT_R3_UNMASKING_STATE_H_
#define FCP_SECAGG_CLIENT_SECAGG_CLIENT_R3_UNMASKING_STATE_H_

#include <cstdint>
#include <memory>
#include <set>
#include <string>

#include "fcp/base/monitoring.h"
#include "fcp/secagg/client/other_client_state.h"
#include "fcp/secagg/client/secagg_client_alive_base_state.h"
#include "fcp/secagg/client/secagg_client_state.h"
#include "fcp/secagg/client/send_to_server_interface.h"
#include "fcp/secagg/client/state_transition_listener_interface.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/secagg/shared/shamir_secret_sharing.h"

namespace fcp {
namespace secagg {

// This class represents the client's Round 3: Unmasking state. This state
// should transition to the Completed state, but can also transition to the
// Aborted state.

class SecAggClientR3UnmaskingState : public SecAggClientAliveBaseState {
 public:
  SecAggClientR3UnmaskingState(
      uint32_t client_id, uint32_t number_of_alive_neighbors,
      uint32_t minimum_surviving_neighbors_for_reconstruction,
      uint32_t number_of_neighbors,
      std::unique_ptr<std::vector<OtherClientState> > other_client_states,
      std::unique_ptr<std::vector<ShamirShare> > pairwise_key_shares,
      std::unique_ptr<std::vector<ShamirShare> > self_key_shares,
      std::unique_ptr<SendToServerInterface> sender,
      std::unique_ptr<StateTransitionListenerInterface> transition_listener,

      AsyncAbort* async_abort = nullptr);

  ~SecAggClientR3UnmaskingState() override;

  StatusOr<std::unique_ptr<SecAggClientState> > HandleMessage(
      const ServerToClientWrapperMessage& message) override;

  // Returns the name of this state, "R3_UNMASKING".
  std::string StateName() const override;

 private:
  const uint32_t client_id_;
  uint32_t number_of_alive_neighbors_;
  const uint32_t minimum_surviving_neighbors_for_reconstruction_;
  const uint32_t number_of_neighbors_;
  std::unique_ptr<std::vector<OtherClientState> > other_client_states_;
  std::unique_ptr<std::vector<ShamirShare> > pairwise_key_shares_;
  std::unique_ptr<std::vector<ShamirShare> > self_key_shares_;
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_CLIENT_SECAGG_CLIENT_R3_UNMASKING_STATE_H_
