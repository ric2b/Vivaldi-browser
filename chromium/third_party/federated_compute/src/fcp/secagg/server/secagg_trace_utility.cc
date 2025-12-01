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
#include "fcp/secagg/server/secagg_trace_utility.h"

#include "fcp/secagg/server/secagg_server_enums.pb.h"
#include "fcp/secagg/server/tracing_schema.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"

namespace fcp {
namespace secagg {

TracingClientStatus ClientStatusType(ClientStatus client_status) {
  switch (client_status) {
    case (ClientStatus::READY_TO_START):
      return TracingClientStatus_ReadyToStart;
    case (ClientStatus::DEAD_BEFORE_SENDING_ANYTHING):
      return TracingClientStatus_DeadBeforeSendingAnything;
    case (ClientStatus::ADVERTISE_KEYS_RECEIVED):
      return TracingClientStatus_AdvertiseKeysReceived;
    case (ClientStatus::DEAD_AFTER_ADVERTISE_KEYS_RECEIVED):
      return TracingClientStatus_DeadAfterAdvertiseKeysReceived;
    case (ClientStatus::SHARE_KEYS_RECEIVED):
      return TracingClientStatus_ShareKeysReceived;
    case (ClientStatus::DEAD_AFTER_SHARE_KEYS_RECEIVED):
      return TracingClientStatus_DeadAfterShareKeysReceived;
    case (ClientStatus::MASKED_INPUT_RESPONSE_RECEIVED):
      return TracingClientStatus_MaskedInputResponseReceived;
    case (ClientStatus::DEAD_AFTER_MASKED_INPUT_RESPONSE_RECEIVED):
      return TracingClientStatus_DeadAfterMaskedInputResponseReceived;
    case (ClientStatus::UNMASKING_RESPONSE_RECEIVED):
      return TracingClientStatus_UnmaskingResponseReceived;
    case (ClientStatus::DEAD_AFTER_UNMASKING_RESPONSE_RECEIVED):
      return TracingClientStatus_DeadAfterUnmaskingResponseReceived;
    default:
      return TracingClientStatus_Unknown;
  }
}

TracingClientDropReason ClientDropReasonType(ClientDropReason reason_code) {
  switch (reason_code) {
    case (ClientDropReason::SENT_ABORT_MESSAGE):
      return TracingClientDropReason_SentAbortMessage;
    case (ClientDropReason::UNEXPECTED_MESSAGE_TYPE):
      return TracingClientDropReason_UnexpectedMessageType;
    case (ClientDropReason::UNKNOWN_MESSAGE_TYPE):
      return TracingClientDropReason_UnknownMessageType;
    case (ClientDropReason::ADVERTISE_KEYS_UNEXPECTED):
      return TracingClientDropReason_AdvertiseKeysUnexpected;
    case (ClientDropReason::EMPTY_PUBLIC_KEY):
      return TracingClientDropReason_EmptyPublicKey;
    case (ClientDropReason::NO_ADVERTISE_KEYS):
      return TracingClientDropReason_NoAdvertiseKeys;
    case (ClientDropReason::SHARE_KEYS_UNEXPECTED):
      return TracingClientDropReason_ShareKeysUnexpected;
    case (ClientDropReason::WRONG_NUMBER_OF_KEY_SHARES):
      return TracingClientDropReason_WrongNumberOfKeyShares;
    case (ClientDropReason::MISSING_KEY_SHARE):
      return TracingClientDropReason_MissingKeyShare;
    case (ClientDropReason::EXTRA_KEY_SHARE):
      return TracingClientDropReason_ExtraKeyShare;
    case (ClientDropReason::NO_SHARE_KEYS):
      return TracingClientDropReason_NoShareKeys;
    case (ClientDropReason::MASKED_INPUT_UNEXPECTED):
      return TracingClientDropReason_MaskedInputUnexpected;
    case (ClientDropReason::INVALID_MASKED_INPUT):
      return TracingClientDropReason_InvalidMaskedInput;
    case (ClientDropReason::NO_MASKED_INPUT):
      return TracingClientDropReason_NoMaskedInput;
    case (ClientDropReason::UNMASKING_RESPONSE_UNEXPECTED):
      return TracingClientDropReason_UnmaskingResponseUnexpected;
    case (ClientDropReason::INVALID_UNMASKING_RESPONSE):
      return TracingClientDropReason_InvalidUnmaskingResponse;
    case (ClientDropReason::NO_UNMASKING_RESPONSE):
      return TracingClientDropReason_NoUnmaskingResponse;
    case (ClientDropReason::INVALID_PUBLIC_KEY):
      return TracingClientDropReason_InvalidPublicKey;
    case (ClientDropReason::SERVER_PROTOCOL_ABORT_CLIENT):
      return TracingClientDropReason_ServerProtocolAbortClient;
    case (ClientDropReason::EARLY_SUCCESS):
      return TracingClientDropReason_EarlySuccess;
    case (ClientDropReason::CONNECTION_CLOSED):
      return TracingClientDropReason_ConnectionClosed;
    default:
      return TracingClientDropReason_Unknown;
  }
}

ClientToServerMessageType GetClientToServerMessageType(
    const ClientToServerWrapperMessage& message) {
  switch (message.message_content_case()) {
    case ClientToServerWrapperMessage::MESSAGE_CONTENT_NOT_SET:
      return ClientToServerMessageType_MessageContentNotSet;
    case ClientToServerWrapperMessage::kAbort:
      return ClientToServerMessageType_Abort;
    case ClientToServerWrapperMessage::kAdvertiseKeys:
      return ClientToServerMessageType_AdvertiseKeys;
    case ClientToServerWrapperMessage::kShareKeysResponse:
      return ClientToServerMessageType_ShareKeysResponse;
    case ClientToServerWrapperMessage::kMaskedInputResponse:
      return ClientToServerMessageType_MaskedInputResponse;
    case ClientToServerWrapperMessage::kUnmaskingResponse:
      return ClientToServerMessageType_UnmaskingResponse;
  }
}

ServerToClientMessageType GetServerToClientMessageType(
    const ServerToClientWrapperMessage& message) {
  switch (message.message_content_case()) {
    case ServerToClientWrapperMessage::kAbort:
      return ServerToClientMessageType_Abort;
    case ServerToClientWrapperMessage::kShareKeysRequest:
      return ServerToClientMessageType_ShareKeysRequest;
    case ServerToClientWrapperMessage::kMaskedInputRequest:
      return ServerToClientMessageType_MaskedInputRequest;
    case ServerToClientWrapperMessage::kUnmaskingRequest:
      return ServerToClientMessageType_UnmaskingRequest;
    default:
      return ServerToClientMessageType_MessageContentNotSet;
  }
}

TracingSecAggServerOutcome ConvertSecAccServerOutcomeToTrace(
    SecAggServerOutcome outcome) {
  switch (outcome) {
    case (SecAggServerOutcome::EXTERNAL_REQUEST):
      return TracingSecAggServerOutcome_ExternalRequest;
    case (SecAggServerOutcome::NOT_ENOUGH_CLIENTS_REMAINING):
      return TracingSecAggServerOutcome_NotEnoughClientsRemaining;
    case (SecAggServerOutcome::UNHANDLED_ERROR):
      return TracingSecAggServerOutcome_UnhandledError;
    case (SecAggServerOutcome::SUCCESS):
      return TracingSecAggServerOutcome_Success;
    default:
      return TracingSecAggServerOutcome_Unknown;
  }
}

SecAggServerTraceState TracingState(SecAggServerStateKind state_kind) {
  switch (state_kind) {
    case SecAggServerStateKind::ABORTED:
      return SecAggServerTraceState_Aborted;
    case SecAggServerStateKind::COMPLETED:
      return SecAggServerTraceState_Completed;
    case SecAggServerStateKind::PRNG_RUNNING:
      return SecAggServerTraceState_PrngRunning;
    case SecAggServerStateKind::R0_ADVERTISE_KEYS:
      return SecAggServerTraceState_R0AdvertiseKeys;
    case SecAggServerStateKind::R1_SHARE_KEYS:
      return SecAggServerTraceState_R1ShareKeys;
    case SecAggServerStateKind::R2_MASKED_INPUT_COLLECTION:
      return SecAggServerTraceState_R2MaskedInputCollection;
    case SecAggServerStateKind::R3_UNMASKING:
      return SecAggServerTraceState_R3Unmasking;
    default:
      return SecAggServerTraceState_UnknownState;
  }
}

}  // namespace secagg
}  // namespace fcp
