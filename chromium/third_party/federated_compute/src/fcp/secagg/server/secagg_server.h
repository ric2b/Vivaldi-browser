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

#ifndef FCP_SECAGG_SERVER_SECAGG_SERVER_H_
#define FCP_SECAGG_SERVER_SECAGG_SERVER_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_set.h"
#include "fcp/base/monitoring.h"
#include "fcp/base/scheduler.h"
#include "fcp/secagg/server/experiments_interface.h"
#include "fcp/secagg/server/experiments_names.h"
#include "fcp/secagg/server/secagg_scheduler.h"
#include "fcp/secagg/server/secagg_server_enums.pb.h"
#include "fcp/secagg/server/secagg_server_messages.pb.h"
#include "fcp/secagg/server/secagg_server_metrics_listener.h"
#include "fcp/secagg/server/secagg_server_state.h"
#include "fcp/secagg/server/secret_sharing_graph.h"
#include "fcp/secagg/server/tracing_schema.h"
#include "fcp/secagg/shared/aes_prng_factory.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/tracing/tracing_span.h"

namespace fcp {
namespace secagg {

// Represents a server for the Secure Aggregation protocol. Each instance of
// this class performs just *one* session of the protocol.
//
// To create a new instance, use the public constructor. Once constructed, the
// server is ready to receive messages from clients with the ReceiveMessage
// method.
//
// When enough messages have been received (i.e. when ReceiveMessage or
// ReadyForNextRound return true) or any time after that, proceed to the next
// round by calling ProceedToNextRound.
//
// After all client interaction is done, the server needs to do some
// multi-threaded computation using the supplied Scheduler. Call StartPrng to
// begin this computation.
//
// When the computation is complete, call Result to get the final result.
//
// This class is not thread-safe.

class SecAggServer {
 public:
  // Constructs a new instance of the Secure Aggregation server.
  //
  // minimum_number_of_clients_to_proceed is the threshold lower bound on the
  // total number of clients expected to complete the protocol. If there are
  // ever fewer than this many clients still alive in the protocol, the server
  // will abort (causing all clients to abort as well).
  //
  // total_number_of_clients is the number of clients selected to be in the
  // cohort for this instance of Secure Aggregation.
  //
  // input_vector_specs must contain one InputVectorSpecification for each input
  // vector which the protocol will aggregate.
  //
  // sender is used by the server to send messages to clients. The server will
  // consume this object, taking ownership of it.
  //
  // sender may be called on a different thread than the thread used to call
  // into SecAggServer, specifically in the PrngRunning state.
  //
  // prng_factory is a pointer to an instance of a subclass of AesPrngFactory.
  // If this client will be communicating with the (C++) version of SecAggClient
  // in this package, then the server and all clients should use
  // AesCtrPrngFactory.
  //
  // metrics will be called over the course of the protocol to record message
  // sizes and events. If it is null, no metrics will be recorded.
  //
  // threat_model includes the assumed maximum adversarial, maximum dropout
  // rate, and adversary class.
  //
  //
  // The protocol successfully
  // completes and returns a sum if and only if no more than
  // floor(total_number_of_clients * threat_model.estimated_dropout_rate())
  // clients dropout before the end of the protocol execution. This ensure that
  // at least ceil(total_number_of_clients
  // *(1. - threat_model.estimated_dropout_rate() -
  // threat_model.adversarial_client_rate)) values from honest clients are
  // included in the final sum.
  // The protocol allows to make that threshold larger by providing a larger
  // value of minimum_number_of_clients_to_proceed, but
  // never lower (if the provided minimum_number_of_clients_to_proceed is
  // smaller than ceil(total_number_of_clients *(1. -
  // threat_model.estimated_dropout_rate())), the protocol defaults to the
  // latter value.
  static StatusOr<std::unique_ptr<SecAggServer>> Create(
      int minimum_number_of_clients_to_proceed, int total_number_of_clients,
      const std::vector<InputVectorSpecification>& input_vector_specs,
      SendToClientsInterface* sender,
      std::unique_ptr<SecAggServerMetricsListener> metrics,
      std::unique_ptr<SecAggScheduler> prng_runner,
      std::unique_ptr<ExperimentsInterface> experiments,
      const SecureAggregationRequirements& threat_model);

  ////////////////////////////// PROTOCOL METHODS //////////////////////////////

  // Makes the server abort the protocol, sending a message to all still-alive
  // clients that the protocol has been aborted. Most of the state will be
  // erased except for some diagnostic information. A new instance of
  // SecAggServer will be needed to restart the protocol.
  //
  // If a reason string is provided, it will be stored by the server and sent to
  // the clients as diagnostic information.
  // An optional outcome can be provided for diagnostic purposes to be recorded
  // via SecAggServerMetricsListener. By default, EXTERNAL_REQUEST outcome is
  // assumed.
  //
  // The status will be OK unless the protocol was already completed or aborted.
  Status Abort();
  Status Abort(const std::string& reason, SecAggServerOutcome outcome);

  // Abort the specified client for the given reason.
  //
  // If the server is in a terminal state, returns a FAILED_PRECONDITION status.
  Status AbortClient(uint32_t client_id, ClientAbortReason reason);

  // Proceeds to the next round, doing necessary computation and sending
  // messages to clients as appropriate.
  //
  // If the server is not ready to proceed, this method will do nothing and
  // return an UNAVAILABLE status. If the server is already in a terminal state,
  // this method will do nothing and return a FAILED_PRECONDITION status.
  //
  // If the server is ready to proceed, but not all clients have yet sent in
  // responses, any client that hasn't yet sent a response will be aborted (and
  // a message informing them of this will be sent).
  //
  // After proceeding to the next round, the server is ready to receive more
  // messages from clients in rounds 1, 2, and 3. In the PrngRunning round, it
  // is instead ready to have StartPrng called.
  //
  // Returns OK as long as the server has actually executed the transition to
  // the next state.
  Status ProceedToNextRound();

  // Processes a message that has been received from a client with the given
  // client_id.
  //
  // The boolean returned indicates whether the server is ready to proceed to
  // the next round. This will be true when a number of clients equal to the
  // minimum_number_of_clients_to_proceed threshold have sent in valid messages
  // (and not subsequently aborted), including this one.
  //
  // If the message is invalid, the client who sent it will be aborted, and a
  // message will be sent to them notifying them of the fact. A client may also
  // send the server a message that it wishes to abort (in which case no further
  // message to it is sent). This may cause a server that was previously ready
  // for the next round to no longer be ready, or it may cause the server to
  // abort if not enough clients remain alive.
  //
  // Returns a FAILED_PRECONDITION status if the server is in a terminal state
  // or the PRNG_RUNNING state.
  //
  // Returns an ABORTED status to signify that the server has aborted after
  // receiving this message. (This will cause all surviving clients to be
  // notified as well.)
  StatusOr<bool> ReceiveMessage(
      uint32_t client_id,
      std::unique_ptr<ClientToServerWrapperMessage> message);
  // Sets up a callback to be invoked when any background asynchronous work
  // has been done. The callback is guaranteed to invoked via the server's
  // callback scheduler.
  //
  // Returns true if asynchronous processing is supported in the current
  // server state and the callback has been setup successfully. Returns false
  // if asynchronous processing isn't supported in the current server state or
  // if no further asynchronous processing is possible. The callback argument
  // is ignored in that case.
  bool SetAsyncCallback(std::function<void()> async_callback);

  /////////////////////////////// STATUS METHODS ///////////////////////////////

  // Returns the set of clients that aborted the protocol. Can be used by the
  // caller to close the relevant RPC connections or just start ignoring
  // incoming messages from those clients for performance reasons.
  absl::flat_hash_set<uint32_t> AbortedClientIds() const;

  // Returns a string describing the reason that the protocol was aborted.
  // If the protocol has not actually been aborted, returns an error Status
  // with code PRECONDITION_FAILED.
  StatusOr<std::string> ErrorMessage() const;

  // Returns true if the protocol has been aborted, false else.
  bool IsAborted() const;

  // Returns true if the protocol has been successfully completed, false else.
  // The Result method can be called exactly when this method returns true.
  bool IsCompletedSuccessfully() const;

  // Whether the set of inputs that will be included in the final aggregation
  // has been fixed.
  //
  // If true, the value of NumberOfIncludedInputs will be fixed for the
  // remainder of the protocol.
  bool IsNumberOfIncludedInputsCommitted() const;

  // Indicates the minimum number of valid messages needed to be able to
  // successfully move to the next round.
  //
  // Note that this value is not guaranteed to be monotonically decreasing.
  // Client failures can cause this value to increase.
  //
  // Calling this in a terminal state results in an error.
  StatusOr<int> MinimumMessagesNeededForNextRound() const;

  // Indicates the total number of clients that the server expects to receive
  // a response from in this round (i.e. the ones that have not aborted). In
  // the COMPLETED state, this returns the number of clients that survived to
  // the final protocol message.
  int NumberOfAliveClients() const;

  // Number of clients that failed after submitting their masked input. These
  // clients' inputs will be included in the aggregate value, even though
  // these clients did not complete the protocol.
  int NumberOfClientsFailedAfterSendingMaskedInput() const;

  // Number of clients that failed before submitting their masked input. These
  // clients' inputs won't be included in the aggregate value, even if the
  // protocol succeeds.
  int NumberOfClientsFailedBeforeSendingMaskedInput() const;

  // Number of clients that submitted a masked value, but didn't report their
  // unmasking values fast enough to have them used in the final unmasking
  // process. These clients' inputs will be included in the aggregate value.
  int NumberOfClientsTerminatedWithoutUnmasking() const;

  // Returns the number of inputs that will appear in the final sum, if the
  // protocol completes.
  //
  // Once IsNumberOfIncludedInputsCommitted is true, this value will be fixed
  // for the remainder of the protocol.
  //
  // This will be 0 if the server is aborted. This will also be 0 if the
  // server is in an early state, prior to receiving masked inputs. It is
  // incremented only when the server receives a masked input from a client.
  int NumberOfIncludedInputs() const;

  // Returns the number of live clients that have not yet submitted the
  // expected response for the current round. In terminal states, this will be
  // 0.
  int NumberOfPendingClients() const;

  // Returns the number of clients that would still be alive if
  // ProceedToNextRound were called immediately after. This value may be less
  // than NumberOfMessagesReceivedInThisRound if a client fails after sending
  // a message in this round.
  //
  // Note that this value is not guaranteed to be monotonically increasing,
  // even within a round. Client failures can cause this value to decrease.
  //
  // Calling this in a terminal state results in an error.
  StatusOr<int> NumberOfClientsReadyForNextRound() const;

  // Returns the number of valid messages received by clients this round.
  // Unlike NumberOfClientsReadyForNextRound, this number is monotonically
  // increasing until ProceedToNextRound is called, or the server aborts.
  //
  // Calling this in a terminal state results in an error.
  StatusOr<int> NumberOfMessagesReceivedInThisRound() const;

  // Returns a boolean indicating if the server has received enough messages
  // from clients (who have not subsequently aborted) to proceed to the next
  // round. ProceedToNextRound will do nothing unless this returns true.
  //
  // Even after this method returns true, the server will remain in the
  // current round until ProceedToNextRound is called.
  //
  // Calling this in a terminal state results in an error.
  StatusOr<bool> ReadyForNextRound() const;

  // Transfers ownership of the result of the protocol to the caller. Requires
  // the server to be in a completed state; returns UNAVAILABLE otherwise.
  // Can be called only once; any consequitive calls result in an error.
  StatusOr<std::unique_ptr<SecAggVectorMap>> Result();

  // Returns the number of neighbors of each client.
  int NumberOfNeighbors() const;

  // Returns the minimum number of neighbors of a client that must not
  // drop-out for that client's contribution to be included in the sum. This
  // corresponds to the threshold in the shamir secret sharing of self and
  // pairwise masks.
  int MinimumSurvivingNeighborsForReconstruction() const;

  // Returns a value uniquely describing the current state of the client's
  // FSM.
  SecAggServerStateKind State() const;

 private:
  // Constructs a new instance of the Secure Aggregation server.
  explicit SecAggServer(std::unique_ptr<SecAggServerProtocolImpl> impl);

  // This causes the server to transition into a new state, and call the
  // callback if one is provided.
  void TransitionState(std::unique_ptr<SecAggServerState> new_state);

  // Validates if the client_id is within the expected bounds.
  Status ValidateClientId(uint32_t client_id) const;

  // Returns an error if the server is in Aborted or Completed state.
  Status ErrorIfAbortedOrCompleted() const;

  // The internal state object, containing details about the server's current
  // state.
  std::unique_ptr<SecAggServerState> state_;

  // Tracing span for this session of SecAggServer. This is bound to the
  // lifetime of SecAggServer i.e. from the time the object is created till it
  // is destroyed.
  UnscopedTracingSpan<SecureAggServerSession> span_;

  // Holds pointer to a tracing span corresponding to the current active
  // SecAggServerState.
  std::unique_ptr<UnscopedTracingSpan<SecureAggServerState>> state_span_;
};

}  // namespace secagg
}  // namespace fcp
#endif  // FCP_SECAGG_SERVER_SECAGG_SERVER_H_
