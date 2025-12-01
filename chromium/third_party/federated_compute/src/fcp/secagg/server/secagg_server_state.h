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

#ifndef FCP_SECAGG_SERVER_SECAGG_SERVER_STATE_H_
#define FCP_SECAGG_SERVER_SECAGG_SERVER_STATE_H_

#include <functional>
#include <memory>
#include <string>

#include "absl/container/flat_hash_set.h"
#include "absl/time/time.h"
#include "fcp/secagg/server/secagg_server_enums.pb.h"
#include "fcp/secagg/server/secagg_server_protocol_impl.h"
#include "fcp/secagg/server/tracing_schema.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"

namespace fcp {
namespace secagg {

// This is an abstract class which is the parent of the other SecAggServer*State
// classes. It should not be instantiated directly. Default versions of all the
// methods declared here are provided for use by states which do not expect, and
// therefore do not implement, those methods.

class SecAggServerState {
 public:
  // Returns the number of clients selected to be in the cohort for this
  // instance of Secure Aggregation.
  inline size_t total_number_of_clients() const {
    return impl_->total_number_of_clients();
  }

  // Returns the number of neighbors of each client.
  inline int number_of_neighbors() const {
    return impl_->number_of_neighbors();
  }

  // Returns the minimum number of neighbors of a client that must not drop-out
  // for that client's contribution to be included in the sum. This corresponds
  // to the threshold in the shamir secret sharing of self and pairwise masks.
  inline int minimum_surviving_neighbors_for_reconstruction() const {
    return impl_->minimum_surviving_neighbors_for_reconstruction();
  }

  // Returns the index of client_id_2 in the list of neighbors of client_id_1,
  // if present
  inline std::optional<int> GetNeighborIndex(int client_id_1,
                                             int client_id_2) const {
    return impl_->GetNeighborIndex(client_id_1, client_id_2);
  }

  // EnterState must be called just after transitioning to a state.
  // States may use this to initialize their state or trigger work.
  virtual void EnterState() {}

  // Processes the received message in a way consistent with the current state.
  //
  // Returns OK status to indicate that the message has been handled
  // successfully.
  //
  // Returns a FAILED_PRECONDITION status if the server is in a state from which
  // it does not expect to receive any messages. In that case no reply will be
  // sent.
  virtual Status HandleMessage(uint32_t client_id,
                               const ClientToServerWrapperMessage& message);
  // Analog of the above method, bu giving ownership of the message.
  virtual Status HandleMessage(
      uint32_t client_id,
      std::unique_ptr<ClientToServerWrapperMessage> message);

  // Proceeds to the next round, doing all necessary computation and sending
  // messages to clients as appropriate. If the server is not yet ready to
  // proceed, returns an UNAVAILABLE status.
  //
  // If the server is in a terminal state, returns a FAILED_PRECONDITION status.
  //
  // Otherwise, returns the new state. This may be an abort state if the server
  // has aborted.
  //
  // If this method returns a new state (i.e. if the status is OK), then the old
  // state is no longer valid and the new state must be considered the current
  // state. If it returns a non-OK status, this method does not change the
  // underlying state.
  virtual StatusOr<std::unique_ptr<SecAggServerState>> ProceedToNextRound();

  // Returns true if the client state is considered to be "dead" e.g. aborted or
  // disconnected; otherwise returns false.
  bool IsClientDead(uint32_t client_id) const;

  // Abort the specified client for the given reason. If notify is true, send a
  // notification message to the client. (If the client was already closed, no
  // message will be sent).
  //
  // The reason code will be used for recording metrics if log_metrics is true,
  // else no metrics are recorded. By default, metrics will always be logged.
  void AbortClient(uint32_t client_id, const std::string& reason,
                   ClientDropReason reason_code, bool notify = true,
                   bool log_metrics = true);

  // Aborts the protocol for the specified reason. Notifies all clients of
  // the abort. Returns the new state.
  // Calling this method on a terminal state isn't valid.
  std::unique_ptr<SecAggServerState> Abort(const std::string& reason,
                                           SecAggServerOutcome outcome);

  // Returns true if the current state is Abort, false else.
  virtual bool IsAborted() const;

  // Returns true if the current state is ProtocolCompleted, false else.
  virtual bool IsCompletedSuccessfully() const;

  // Returns an error message explaining why the server aborted, if the current
  // state is an abort state. If not returns an error Status with code
  // FAILED_PRECONDITION.
  virtual StatusOr<std::string> ErrorMessage() const;

  // Returns an enum specifying the current state.
  SecAggServerStateKind State() const;

  // Returns the name of the current state in the form of a short string.
  std::string StateName() const;

  // Returns whether or not the server has received enough messages to be ready
  // for the next phase of the protocol.
  // In the PRNG Running state, it returns whether or not the PRNG has stopped
  // running.
  // Always false in a terminal state.
  virtual bool ReadyForNextRound() const;

  // Returns the number of valid messages received by clients this round.
  int NumberOfMessagesReceivedInThisRound() const;

  // Returns the number of clients that would still be alive if
  // ProceedToNextRound were called immediately after. This value may be less
  // than NumberOfMessagesReceivedInThisRound if a client fails after sending a
  // message in this round.
  // Note that this value is not guaranteed to be monotonically increasing, even
  // within a round. Client failures can cause this value to decrease.
  virtual int NumberOfClientsReadyForNextRound() const;

  // Indicates the total number of clients that the server expects to receive a
  // response from in this round (i.e. the ones that have not aborted).
  // In the COMPLETED state, this returns the number of clients that survived to
  // the final protocol message.
  virtual int NumberOfAliveClients() const;

  // Number of clients that failed before submitting their masked input. These
  // clients' inputs won't be included in the aggregate value, even if the
  // protocol succeeds.
  int NumberOfClientsFailedBeforeSendingMaskedInput() const;

  // Number of clients that failed after submitting their masked input. These
  // clients' inputs will be included in the aggregate value, even though these
  // clients did not complete the protocol.
  int NumberOfClientsFailedAfterSendingMaskedInput() const;

  // Number of clients that submitted a masked value, but didn't report their
  // unmasking values fast enough to have them used in the final unmasking
  // process. These clients' inputs will be included in the aggregate value.
  int NumberOfClientsTerminatedWithoutUnmasking() const;

  // Returns the number of live clients that have not yet submitted the expected
  // response for the current round. In terminal states, this will be 0.
  virtual int NumberOfPendingClients() const;

  // Returns the number of inputs that will appear in the final sum, if the
  // protocol completes.
  // Once IsNumberOfIncludedInputsCommitted is true, this value will be fixed
  // for the remainder of the protocol.
  // This will be 0 if the server is aborted. This will also be 0 if the server
  // is in an early state, prior to receiving masked inputs. It is incremented
  // only when the server receives a masked input from a client.
  virtual int NumberOfIncludedInputs() const;

  // Whether the set of inputs that will be included in the final aggregation is
  // fixed.
  // If true, the value of NumberOfIncludedInputs will be fixed for the
  // remainder of the protocol.
  virtual bool IsNumberOfIncludedInputsCommitted() const = 0;

  // Indicates the minimum number of valid messages needed to be able to
  // successfully move to the next round.
  // Note that this value is not guaranteed to be monotonically decreasing.
  // Client failures can cause this value to increase.
  // In terminal states, this returns 0.
  virtual int MinimumMessagesNeededForNextRound() const;

  // Returns the minimum threshold number of clients that need to send valid
  // responses in order for the protocol to proceed from one round to the next.
  inline int minimum_number_of_clients_to_proceed() const {
    return impl_->minimum_number_of_clients_to_proceed();
  }

  // Returns the set of clients that aborted the protocol. Can be used by the
  // caller to close the relevant RPC connections or just start ignoring
  // incoming messages from those clients for performance reasons.
  absl::flat_hash_set<uint32_t> AbortedClientIds() const;

  // Returns true if the server has determined that it needs to abort itself,
  // If the server is in a terminal state, returns false.
  bool NeedsToAbort() const;

  // Sets up a callback to be triggered when any background asynchronous work
  // has been done. The callback is guaranteed to invoked via the server's
  // callback scheduler.
  //
  // Returns true if the state supports asynchronous processing and the callback
  // has been setup successfully.
  // Returns false if the state doesn't support asynchronous processing or if
  // no further asynchronous processing is possible. The callback argument is
  // ignored in this case.
  virtual bool SetAsyncCallback(std::function<void()> async_callback);

  // Transfers ownership of the result of the protocol to the caller. Requires
  // the server to be in a completed state; returns UNAVAILABLE otherwise.
  // Can be called only once; any consecutive calls result in an error.
  virtual StatusOr<std::unique_ptr<SecAggVectorMap>> Result();

  virtual ~SecAggServerState();

 protected:
  // SecAggServerState should never be instantiated directly.
  SecAggServerState(int number_of_clients_failed_after_sending_masked_input,
                    int number_of_clients_failed_before_sending_masked_input,
                    int number_of_clients_terminated_without_unmasking,
                    SecAggServerStateKind state_kind,
                    std::unique_ptr<SecAggServerProtocolImpl> impl);

  SecAggServerProtocolImpl* impl() { return impl_.get(); }

  // Returns the callback interface for recording metrics.
  inline SecAggServerMetricsListener* metrics() const {
    return impl_->metrics();
  }

  // Returns the callback interface for sending protocol buffer messages to the
  // client.
  inline SendToClientsInterface* sender() const { return impl_->sender(); }

  inline const ClientStatus& client_status(uint32_t client_id) const {
    return impl_->client_status(client_id);
  }

  inline void set_client_status(uint32_t client_id, ClientStatus status) {
    impl_->set_client_status(client_id, status);
  }

  // Records information about a message that was received from a client.
  void MessageReceived(const ClientToServerWrapperMessage& message,
                       bool expected);

  // Broadcasts the message and records metrics.
  void BroadcastMessage(const ServerToClientWrapperMessage& message);

  // Sends the message to the given client and records metrics.
  void Send(uint32_t recipient_id, const ServerToClientWrapperMessage& message);

  // Returns an aborted version of the current state, storing the specified
  // reason. Calling this method makes the current state unusable. The caller is
  // responsible for sending any failure messages that need to be sent, and for
  // doing so BEFORE calling this method.
  // The SecAggServerOutcome outcome is used for recording metrics.
  std::unique_ptr<SecAggServerState> AbortState(const std::string& reason,
                                                SecAggServerOutcome outcome);

  // ExitState must be called on the current state just before transitioning to
  // a new state to record metrics and transfer out the shared state.
  enum class StateTransition {
    // Indicates a successful state transition to any state other than Aborted.
    kSuccess = 0,
    // Indicates transition to Aborted state.
    kAbort = 1
  };
  std::unique_ptr<SecAggServerProtocolImpl>&& ExitState(
      StateTransition state_transition_status);

  bool needs_to_abort_;
  int number_of_clients_failed_after_sending_masked_input_;
  int number_of_clients_failed_before_sending_masked_input_;
  int number_of_clients_ready_for_next_round_;
  int number_of_clients_terminated_without_unmasking_;
  int number_of_messages_received_in_this_round_;
  absl::Time round_start_;
  SecAggServerStateKind state_kind_;

 private:
  // Performs state specific action when a client is aborted.
  virtual void HandleAbortClient(uint32_t client_id,
                                 ClientDropReason reason_code) {}

  // Performs state specific action when the server is aborted.
  virtual void HandleAbort() {}

  std::unique_ptr<SecAggServerProtocolImpl> impl_;
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SERVER_SECAGG_SERVER_STATE_H_
