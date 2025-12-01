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

#ifndef FCP_SECAGG_SERVER_SECAGG_SERVER_R0_ADVERTISE_KEYS_STATE_H_
#define FCP_SECAGG_SERVER_SECAGG_SERVER_R0_ADVERTISE_KEYS_STATE_H_

#include <memory>

#include "fcp/secagg/server/secagg_server_state.h"

namespace fcp {
namespace secagg {

// This class is the State for the SecAggServer when it is in the
// Round 0: Advertise state. The server begins in this state. This state
// collects public ECDH keys from clients, stores the one of them that will be
// used later in running the PRNG, and then sends to each client both public
// keys from each of its neighbors. It also computes and stores the session ID
// from the received keys, and sends the session ID to each client along with
// its neighbor's keys. This state should transition to Round 1: Share Keys, but
// might transition to Aborted if too many clients abort.

class SecAggServerR0AdvertiseKeysState : public SecAggServerState {
 public:
  explicit SecAggServerR0AdvertiseKeysState(
      std::unique_ptr<SecAggServerProtocolImpl> impl);

  ~SecAggServerR0AdvertiseKeysState() override;

  // Handles an advertise keys response or abort message from a client.
  Status HandleMessage(uint32_t client_id,
                       const ClientToServerWrapperMessage& message) override;

  bool IsNumberOfIncludedInputsCommitted() const override;

  int MinimumMessagesNeededForNextRound() const override;

  int NumberOfPendingClients() const override;

  StatusOr<std::unique_ptr<SecAggServerState> > ProceedToNextRound() override;

  // This will return true only after minimum_number_of_clients_to_proceed
  // clients have sent messages (and not subsequently aborted).
  bool ReadyForNextRound() const override;

 private:
  void HandleAbortClient(uint32_t client_id,
                         ClientDropReason reason_code) override;
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SERVER_SECAGG_SERVER_R0_ADVERTISE_KEYS_STATE_H_
