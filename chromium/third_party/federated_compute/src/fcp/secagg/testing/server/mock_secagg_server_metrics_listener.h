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

#ifndef FCP_SECAGG_TESTING_SERVER_MOCK_SECAGG_SERVER_METRICS_LISTENER_H_
#define FCP_SECAGG_TESTING_SERVER_MOCK_SECAGG_SERVER_METRICS_LISTENER_H_

#include "gmock/gmock.h"
#include "fcp/secagg/server/secagg_server_enums.pb.h"
#include "fcp/secagg/server/secagg_server_metrics_listener.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"

namespace fcp {
namespace secagg {

// GMock Implementation of SecAggServerMetricsListener.
class MockSecAggServerMetricsListener : public SecAggServerMetricsListener {
 public:
  MOCK_METHOD(void, ProtocolStarts, (ServerVariant server_variant), (override));
  MOCK_METHOD(void, IndividualMessageSizes,
              (ServerToClientWrapperMessage::MessageContentCase message_type,
               uint64_t size),
              (override));
  MOCK_METHOD(void, MessageReceivedSizes,
              (ClientToServerWrapperMessage::MessageContentCase message_type,
               bool message_expected, uint64_t size),
              (override));
  MOCK_METHOD(void, ClientResponseTimes,
              (ClientToServerWrapperMessage::MessageContentCase message_type,
               uint64_t elapsed_millis),
              (override));
  MOCK_METHOD(void, RoundTimes,
              (SecAggServerStateKind target_state, bool successful,
               uint64_t elapsed_millis),
              (override));
  MOCK_METHOD(void, PrngExpansionTimes, (uint64_t elapsed_millis), (override));
  MOCK_METHOD(void, RoundSurvivingClients,
              (SecAggServerStateKind target_state, uint64_t number_of_clients),
              (override));
  MOCK_METHOD(void, RoundCompletionFractions,
              (SecAggServerStateKind target_state, ClientStatus client_state,
               double fraction),
              (override));
  MOCK_METHOD(void, ProtocolOutcomes, (SecAggServerOutcome outcome),
              (override));
  MOCK_METHOD(void, ClientsDropped,
              (ClientStatus abort_state, ClientDropReason error_code),
              (override));
  MOCK_METHOD(void, ShamirReconstructionTimes, (uint64_t elapsed_millis),
              (override));
};

}  // namespace secagg
}  // namespace fcp
#endif  // FCP_SECAGG_TESTING_SERVER_MOCK_SECAGG_SERVER_METRICS_LISTENER_H_
