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

#ifndef FCP_SECAGG_SERVER_SECAGG_SERVER_ABORTED_STATE_H_
#define FCP_SECAGG_SERVER_SECAGG_SERVER_ABORTED_STATE_H_

#include <memory>
#include <string>

#include "fcp/secagg/server/secagg_server_state.h"

namespace fcp {
namespace secagg {

// This class is the State for the SecAggServer after it has aborted. The server
// cannot transition out of this state; a new SecAggServer object will be needed
// to start a new run of the protocol. However, an aborted SecAggServer still
// stores some of the information about the server before it aborted.

class SecAggServerAbortedState : public SecAggServerState {
 public:
  SecAggServerAbortedState(
      const std::string& error_message,
      std::unique_ptr<SecAggServerProtocolImpl> impl,
      int number_of_clients_failed_after_sending_masked_input,
      int number_of_clients_failed_before_sending_masked_input,
      int number_of_clients_terminated_without_unmasking);

  ~SecAggServerAbortedState() override;

  // Returns true.
  bool IsAborted() const override;

  // Returns an error message explaining why the server aborted.
  StatusOr<std::string> ErrorMessage() const override;

  bool IsNumberOfIncludedInputsCommitted() const override;

 private:
  const std::string error_message_;
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SERVER_SECAGG_SERVER_ABORTED_STATE_H_
