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
#ifndef FCP_SECAGG_SERVER_SECAGG_TRACE_UTILITY_H_
#define FCP_SECAGG_SERVER_SECAGG_TRACE_UTILITY_H_

#include "fcp/secagg/server/secagg_server_enums.pb.h"
#include "fcp/secagg/server/tracing_schema.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"

namespace fcp {
namespace secagg {
// Returns the ClientStatus state to be used for the context of tracing
TracingClientStatus ClientStatusType(ClientStatus client_status);

// Returns the ClientDropReason state to be used for the context of tracing
TracingClientDropReason ClientDropReasonType(ClientDropReason reason_code);

// Returns the ClientToServerWrapperMessage state
// to be used for the context of tracing
ClientToServerMessageType GetClientToServerMessageType(
    const ClientToServerWrapperMessage& message);

// Returns the ClientToServerWrapperMessage state
// to be used for the context of tracing
ServerToClientMessageType GetServerToClientMessageType(
    const ServerToClientWrapperMessage& message);

// Returns the SecAggServerOutcome state
// to be used for the context of tracing
TracingSecAggServerOutcome ConvertSecAccServerOutcomeToTrace(
    SecAggServerOutcome outcome);

// Returns the SecAggServerStateKind state
// to be used for the context of tracing
SecAggServerTraceState TracingState(SecAggServerStateKind state_kind);

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SERVER_SECAGG_TRACE_UTILITY_H_
