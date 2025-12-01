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

#ifndef FCP_SECAGG_SERVER_SECAGG_SERVER_METRICS_LISTENER_H_
#define FCP_SECAGG_SERVER_SECAGG_SERVER_METRICS_LISTENER_H_

#include "fcp/secagg/server/secagg_server_enums.pb.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"

namespace fcp {
namespace secagg {

// Callback interface for reporting SecAggServer metrics.
class SecAggServerMetricsListener {
 public:
  virtual ~SecAggServerMetricsListener() = default;

  // Called each time SecAggServer is instantiated, starting the SecAgg
  // protocol.
  virtual void ProtocolStarts(ServerVariant server_variant) = 0;

  // Size (in bytes) of a message sent by the SecAgg server to an individual
  // user.
  virtual void IndividualMessageSizes(
      ServerToClientWrapperMessage::MessageContentCase message_type,
      uint64_t size) = 0;

  // Size (in bytes) of a message received by the SecAgg server from a user.
  virtual void MessageReceivedSizes(
      ClientToServerWrapperMessage::MessageContentCase message_type,
      bool message_expected, uint64_t size) = 0;

  // Time (in milliseconds) taken for a client to send a response message to the
  // server.
  // Measured from the time the server sent the previous round's message to
  // the time a new message was received. The first round is measured starting
  // from the instantiation of the SecAggServer. Only messages received
  // before the end of the round are monitored.
  virtual void ClientResponseTimes(
      ClientToServerWrapperMessage::MessageContentCase message_type,
      uint64_t elapsed_millis) = 0;

  // Time (in milliseconds) spent in each round.
  // Counts end-to-end time spent in each state, starting from transitioning to
  // that state and including waiting for the client messages necessary to
  // transition to a next state.
  virtual void RoundTimes(SecAggServerStateKind target_state, bool successful,
                          uint64_t elapsed_millis) = 0;

  // Times (in milliseconds) taken to execute the PRF expansion step.
  // During PRNG expansion, the server computes the map of masking vectors
  // needed for unmasking. These are wall times measured over the execution of a
  // multi-threaded process.
  virtual void PrngExpansionTimes(uint64_t elapsed_millis) = 0;

  // Number of clients at the end of each round.
  virtual void RoundSurvivingClients(SecAggServerStateKind target_state,
                                     uint64_t number_of_clients) = 0;

  // Fraction of clients at each client state at the end of each round.
  // Fractions are calculates off the total number of clients that the protocol
  // starts with.
  virtual void RoundCompletionFractions(SecAggServerStateKind target_state,
                                        ClientStatus client_state,
                                        double fraction) = 0;

  // Records outcomes of SecAggServerImpl protocol runs.
  // SUCCESS means the protocol ran through all phases and produced output.
  virtual void ProtocolOutcomes(SecAggServerOutcome outcome) = 0;

  // Called when a client drops during an execution of the SecAgg protocol.
  virtual void ClientsDropped(ClientStatus abort_state,
                              ClientDropReason error_code) = 0;

  // Time (in milliseconds) taken to reconstruct all users' keys from their
  // Shamir secret shares.
  // This includes all reconstruction operations for all shares, taken together.
  virtual void ShamirReconstructionTimes(uint64_t elapsed_millis) = 0;
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SERVER_SECAGG_SERVER_METRICS_LISTENER_H_
