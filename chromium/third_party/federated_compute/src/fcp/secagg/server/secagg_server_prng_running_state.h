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

#ifndef FCP_SECAGG_SERVER_SECAGG_SERVER_PRNG_RUNNING_STATE_H_
#define FCP_SECAGG_SERVER_SECAGG_SERVER_PRNG_RUNNING_STATE_H_

#include <functional>
#include <memory>
#include <optional>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "fcp/secagg/server/secagg_server_state.h"

namespace fcp {
namespace secagg {

// This class is the State for the SecAggServer when it has collected all secret
// shares from the clients and is ready to compute its final output. The
// protocol is essentially done, but this is a separate state from
// SecAggClientCompletedState because there the server still needs to run the
// potentially expensive step of using the PRNG to stretch client keys into
// masking vectors.

class SecAggServerPrngRunningState final : public SecAggServerState {
 public:
  SecAggServerPrngRunningState(
      std::unique_ptr<SecAggServerProtocolImpl> impl,
      int number_of_clients_failed_after_sending_masked_input,
      int number_of_clients_failed_before_sending_masked_input,
      int number_of_clients_terminated_without_unmasking);

  ~SecAggServerPrngRunningState() override;

  void EnterState() override;

  // Handles abort message from a client. Any other type of message is
  // unexpected and results in the client being aborted.
  Status HandleMessage(uint32_t client_id,
                       const ClientToServerWrapperMessage& message) override;

  bool IsNumberOfIncludedInputsCommitted() const override;

  int NumberOfIncludedInputs() const override;

  StatusOr<std::unique_ptr<SecAggServerState> > ProceedToNextRound() override;

  bool ReadyForNextRound() const override;

  bool SetAsyncCallback(std::function<void()> async_callback) override;

 private:
  void HandleAbort() override;

  void HandleAbortClient(uint32_t client_id,
                         ClientDropReason reason_code) override;

  // Called to perform the initial synchronous part of PRNG state.
  StatusOr<SecAggServerProtocolImpl::PrngWorkItems> Initialize();

  // This is called when all computations are finished.
  // final_status indicates whether PRNG computation has finished successfully.
  void PrngRunnerFinished(Status final_status);

  // The status is assigned when the state completes either successfully or
  // unsuccessfully.
  std::optional<Status> completion_status_ ABSL_GUARDED_BY(mutex_);

  absl::Time prng_started_time_;
  AsyncToken async_token_;

  std::function<void()> prng_done_callback_ ABSL_GUARDED_BY(mutex_);

  // Protects this object from being destroyed while StartPrng call is still
  // in progress. Also protects completion_status_ and prng_done_callback_.
  mutable absl::Mutex mutex_;
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SERVER_SECAGG_SERVER_PRNG_RUNNING_STATE_H_
