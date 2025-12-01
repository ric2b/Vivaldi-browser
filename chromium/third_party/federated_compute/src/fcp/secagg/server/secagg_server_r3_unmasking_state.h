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

#ifndef FCP_SECAGG_SERVER_SECAGG_SERVER_R3_UNMASKING_STATE_H_
#define FCP_SECAGG_SERVER_SECAGG_SERVER_R3_UNMASKING_STATE_H_

#include <memory>

#include "fcp/secagg/server/secagg_server_state.h"

namespace fcp {
namespace secagg {

// This class is the State for the SecAggServer when it is in the
// Round 3: Unmasking state. This state covers the process of collecting secret
// shares from clients, based on which clients submitted masked input in the
// previous round. Unless the server aborts a client (or itself), it should not
// need to send messages this state. This state should transition to
// SecAggServerPrngRunningState once enough secret shares have been collected.
// Unlike previous steps, there is no particular reason to wait for more than
// the bare minimum number of clients to proceed.

class SecAggServerR3UnmaskingState : public SecAggServerState {
 public:
  SecAggServerR3UnmaskingState(
      std::unique_ptr<SecAggServerProtocolImpl> impl,
      int number_of_clients_failed_after_sending_masked_input,
      int number_of_clients_failed_before_sending_masked_input,
      int number_of_clients_terminated_without_unmasking);

  ~SecAggServerR3UnmaskingState() override;

  // Handles an unmasking response or abort message from a client.
  Status HandleMessage(uint32_t client_id,
                       const ClientToServerWrapperMessage& message) override;

  bool IsNumberOfIncludedInputsCommitted() const override;

  int MinimumMessagesNeededForNextRound() const override;

  int NumberOfIncludedInputs() const override;

  int NumberOfPendingClients() const override;

  StatusOr<std::unique_ptr<SecAggServerState> > ProceedToNextRound() override;

  // This will return true only after minimum_number_of_clients_to_proceed
  // messages have been received.
  bool ReadyForNextRound() const override;

 private:
  void HandleAbortClient(uint32_t client_id,
                         ClientDropReason reason_code) override;
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SERVER_SECAGG_SERVER_R3_UNMASKING_STATE_H_
