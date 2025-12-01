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

#ifndef FCP_SECAGG_SERVER_SEND_TO_CLIENTS_INTERFACE_H_
#define FCP_SECAGG_SERVER_SEND_TO_CLIENTS_INTERFACE_H_

#include "fcp/secagg/shared/secagg_messages.pb.h"

namespace fcp {
namespace secagg {

// Used to provide a SecAggServer with a private and authenticated channel with
// the clients, which can be used to send protocol buffer messages.

class SendToClientsInterface {
 public:
  // Sends a message to a single client.
  virtual void Send(uint32_t recipient_id,
                    const ServerToClientWrapperMessage& message) = 0;

  virtual ~SendToClientsInterface() = default;
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SERVER_SEND_TO_CLIENTS_INTERFACE_H_
