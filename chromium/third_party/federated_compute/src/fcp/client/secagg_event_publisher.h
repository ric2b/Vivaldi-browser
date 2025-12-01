/*
 * Copyright 2020 Google LLC
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
#ifndef FCP_CLIENT_SECAGG_EVENT_PUBLISHER_H_
#define FCP_CLIENT_SECAGG_EVENT_PUBLISHER_H_

#include <cstddef>
#include <cstdint>
#include <string>

namespace fcp::secagg {
enum class ClientState : int;
}  // namespace fcp::secagg

namespace fcp {
namespace client {

// An interface for publishing events that occur during the secure
// aggregation protocol. All methods in here either succeed with OK, or fail
// with INVALID_ARGUMENT.
class SecAggEventPublisher {
 public:
  virtual ~SecAggEventPublisher() = default;

  // Publishes that the protocol has left the prior state and entered the
  // given state, along with the size of the last message sent.
  virtual void PublishStateTransition(::fcp::secagg::ClientState state,
                                      size_t last_sent_message_size,
                                      size_t last_received_message_size) = 0;
  // Publishes a top-level SecAgg client error.
  virtual void PublishError() = 0;
  // Publishes a SecAgg client abort.
  virtual void PublishAbort(bool client_initiated,
                            const std::string& error_message) = 0;
  // After calling this function, all subsequently published events will be
  // annotated with the specified execution logging ID, which is set during
  // protocol execution.
  virtual void set_execution_session_id(int64_t execution_session_id) = 0;
};

}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_SECAGG_EVENT_PUBLISHER_H_
