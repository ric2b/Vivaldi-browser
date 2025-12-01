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

#ifndef FCP_SECAGG_CLIENT_SECAGG_CLIENT_H_
#define FCP_SECAGG_CLIENT_SECAGG_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "fcp/base/monitoring.h"
#include "fcp/secagg/client/secagg_client_state.h"
#include "fcp/secagg/client/send_to_server_interface.h"
#include "fcp/secagg/client/state_transition_listener_interface.h"
#include "fcp/secagg/shared/aes_prng_factory.h"
#include "fcp/secagg/shared/async_abort.h"
#include "fcp/secagg/shared/input_vector_specification.h"
#include "fcp/secagg/shared/prng.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/secagg/shared/secagg_vector.h"

namespace fcp {
namespace secagg {

// Represents a client for the secure aggregation protocol. Each instance of
// this class performs just *one* session of the protocol.
//
// To create a new instance, use the public constructor. The Start method can be
// used to produce the first message of the protocol, the SetInput method sets
// the input for this client and the ReceiveMessage method is used to process
// incoming messages from the server.
//
// The class is thread-safe, but will deadlock if accessed reentrantly from
// the SendToServerInterface callback.
//
// Functions are marked virtual for mockability.  Additional virtual attributes
// should be added as needed by tests.

class SecAggClient {
 public:
  // Creates a new instance of the client.
  //
  // max_neighbors_expected is the upper bound on the total number of neighbors
  // this client may interact with. If the server tries to start a protocol
  // session with more than this many neighbors, this client will abort.
  //
  // minimum_surviving_neighbors_for_reconstruction is the threshold lower bound
  // on the number of neighbors participating. If there are ever fewer than this
  // number of remaining neighbors in the protocol, this client will abort.
  //
  // input_vector_specs must contain one InputVectorSpecification for each input
  // vector which the protocol will aggregate.  This may optionally be moved
  // from using std::move(caller_input_vector_specs).
  //
  // prng should always be an instance of CryptoRandPrng, except as needed for
  // testing purposes. The client will consume prng, taking ownership of it.
  //
  // sender is used by the client to send messages to the server. The client
  // will consume sender, taking ownership of it.
  //
  // transition_listener is used to trigger state transition events, used for
  // logging.
  //
  // prng_factory is a pointer to an instance of a subclass of AesPrngFactory.
  // The type of prng_factory must be consistent with the one used on the
  // server.
  //
  // async_abort_for_test, optionally, allows the caller to reset the abort
  // signal. This is used to exhaustively test all abort paths, and should not
  // be used in production; specifically, if this paramter is not nullptr,
  // Abort() will no longer abort a state-in-progress; it will only abort across
  // state transitions.
  SecAggClient(
      int max_neighbors_expected,
      int minimum_surviving_neighbors_for_reconstruction,
      std::vector<InputVectorSpecification> input_vector_specs,
      std::unique_ptr<SecurePrng> prng,
      std::unique_ptr<SendToServerInterface> sender,
      std::unique_ptr<StateTransitionListenerInterface> transition_listener,
      std::unique_ptr<AesPrngFactory> prng_factory,
      std::atomic<std::string*>* abort_signal_for_test = nullptr);
  virtual ~SecAggClient() = default;

  // Disallow copy and move.
  SecAggClient(const SecAggClient&) = delete;
  SecAggClient& operator=(const SecAggClient&) = delete;

  // Initiates the protocol by computing its first message and sending it to
  // the server. This method should only be called once. The output will be OK
  // unless it is called more than once.
  virtual Status Start();

  // Makes this client abort the protocol and sends a message to notify the
  // server. All the state is erased. A new instance of SecAggClient will have
  // to be created to restart the protocol.
  //
  // The status will be OK unless the protocol was already completed or aborted.
  Status Abort();

  // Makes this client abort the protocol and sends a message to notify the
  // server. All the state is erased. A new instance of SecAggClient will have
  // to be created to restart the protocol.
  //
  // The specified reason for aborting will be sent to the server and logged.
  //
  // The status will be OK unless the protocol was already completed or aborted.
  Status Abort(const std::string& reason);

  // Sets the input of this client for this protocol session. This method should
  // only be called once.
  //
  // If the input does not match the format laid out in input_vector_specs,
  // this will return INVALID_ARGUMENT. If SetInput has already been called or
  // if the client is in an aborted or completed state, this will return
  // FAILED_PRECONDITION. Otherwise returns OK.
  Status SetInput(std::unique_ptr<SecAggVectorMap> input_map);

  // Returns a string uniquely describing the current state of the client's FSM.
  ABSL_MUST_USE_RESULT std::string State() const;

  // Returns true if the client has aborted the protocol, false else.
  ABSL_MUST_USE_RESULT bool IsAborted() const;

  // Returns true if the client has successfully completed the protocol,
  // false else.
  ABSL_MUST_USE_RESULT bool IsCompletedSuccessfully() const;

  // Returns a string describing the reason that the client aborted.
  // If the client has not actually aborted, returns an error Status with code
  // PRECONDITION_FAILED.
  ABSL_MUST_USE_RESULT StatusOr<std::string> ErrorMessage() const;

  // Used to process an incoming message from the server. This method uses the
  // SendToServerInterface passed to the constructor to send the response
  // directly to the server.
  //
  // The output will be true if the client is still active, or false if the
  // client is now in a terminal state. The output will be a failure status if
  // the client did not process the message because it was in a terminal state,
  // or because the message was the wrong type.
  StatusOr<bool> ReceiveMessage(const ServerToClientWrapperMessage& incoming);

 private:
  mutable absl::Mutex mu_;

  std::atomic<std::string*> abort_signal_;
  AsyncAbort async_abort_;

  // The internal State object, containing details about this client's current
  // state.
  std::unique_ptr<SecAggClientState> state_ ABSL_GUARDED_BY(mu_);
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_CLIENT_SECAGG_CLIENT_H_
