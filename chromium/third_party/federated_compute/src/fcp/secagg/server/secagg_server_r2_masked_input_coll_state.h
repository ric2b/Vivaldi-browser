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

#ifndef FCP_SECAGG_SERVER_SECAGG_SERVER_R2_MASKED_INPUT_COLL_STATE_H_
#define FCP_SECAGG_SERVER_SECAGG_SERVER_R2_MASKED_INPUT_COLL_STATE_H_

#include <functional>
#include <memory>
#include <vector>

#include "fcp/secagg/server/secagg_scheduler.h"
#include "fcp/secagg/server/secagg_server_state.h"

namespace fcp {
namespace secagg {

// This class is the State for the SecAggServer when it is in the
// Round 2: Masked Input Collection state. This state receives masked inputs
// from clients and adds them together in preparation for the unmasking step. At
// the conclusion of masked input collection, if the server has collected enough
// masked inputs, it sends the clients a message with the set of clients that
// have not sent masked inputs and moved into Round 3: Unmasking. If too many
// clients abort, it can abort instead.
class SecAggServerR2MaskedInputCollState : public SecAggServerState {
 public:
  SecAggServerR2MaskedInputCollState(
      std::unique_ptr<SecAggServerProtocolImpl> impl,
      int number_of_clients_failed_after_sending_masked_input,
      int number_of_clients_failed_before_sending_masked_input,
      int number_of_clients_terminated_without_unmasking);

  ~SecAggServerR2MaskedInputCollState() override;

  bool IsNumberOfIncludedInputsCommitted() const override;

  int MinimumMessagesNeededForNextRound() const override;

  int NumberOfIncludedInputs() const override;

  int NumberOfPendingClients() const override;

  // This will return true only after minimum_number_of_clients_to_proceed
  // clients have sent messages (and not subsequently aborted).
  bool ReadyForNextRound() const override;

  // Handles a masked input response or abort message from a client.
  Status HandleMessage(uint32_t client_id,
                       const ClientToServerWrapperMessage& message) override;
  Status HandleMessage(
      uint32_t client_id,
      std::unique_ptr<ClientToServerWrapperMessage> message) override;

  StatusOr<std::unique_ptr<SecAggServerState> > ProceedToNextRound() override;

  bool SetAsyncCallback(std::function<void()> async_callback) override;

 protected:
  // Track the clients who abort this round and send this list to the clients.
  std::vector<uint32_t> clients_aborted_at_round_2_;

 private:
  AsyncToken async_token_;
  void HandleAbort() override;

  void HandleAbortClient(uint32_t client_id,
                         ClientDropReason reason_code) override;
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SERVER_SECAGG_SERVER_R2_MASKED_INPUT_COLL_STATE_H_
