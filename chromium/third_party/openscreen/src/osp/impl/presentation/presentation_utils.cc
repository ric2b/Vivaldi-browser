// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/presentation/presentation_utils.h"

#include "osp/public/network_service_manager.h"

namespace openscreen::osp {

MessageDemuxer& GetServerDemuxer() {
  return NetworkServiceManager::Get()
      ->GetProtocolConnectionServer()
      ->GetMessageDemuxer();
}

MessageDemuxer& GetClientDemuxer() {
  return NetworkServiceManager::Get()
      ->GetProtocolConnectionClient()
      ->GetMessageDemuxer();
}

std::unique_ptr<ProtocolConnection> CreateServerProtocolConnection(
    uint64_t instance_id) {
  return NetworkServiceManager::Get()
      ->GetProtocolConnectionServer()
      ->CreateProtocolConnection(instance_id);
}

std::unique_ptr<ProtocolConnection> CreateClientProtocolConnection(
    uint64_t instance_id) {
  return NetworkServiceManager::Get()
      ->GetProtocolConnectionClient()
      ->CreateProtocolConnection(instance_id);
}

msgs::PresentationConnectionCloseEvent_reason ConvertCloseEventReason(
    Connection::CloseReason reason) {
  switch (reason) {
    case Connection::CloseReason::kDiscarded:
      return msgs::PresentationConnectionCloseEvent_reason::
          kConnectionObjectDiscarded;

    case Connection::CloseReason::kError:
      return msgs::PresentationConnectionCloseEvent_reason::
          kUnrecoverableErrorWhileSendingOrReceivingMessage;

    case Connection::CloseReason::kClosed:  // fallthrough
    default:
      return msgs::PresentationConnectionCloseEvent_reason::kCloseMethodCalled;
  }
}

msgs::PresentationTerminationSource ConvertTerminationSource(
    TerminationSource source) {
  switch (source) {
    case TerminationSource::kController:
      return msgs::PresentationTerminationSource::kController;
    case TerminationSource::kReceiver:
      return msgs::PresentationTerminationSource::kReceiver;
    default:
      return msgs::PresentationTerminationSource::kUnknown;
  }
}

msgs::PresentationTerminationReason ConvertTerminationReason(
    TerminationReason reason) {
  switch (reason) {
    case TerminationReason::kApplicationTerminated:
      return msgs::PresentationTerminationReason::kApplicationRequest;
    case TerminationReason::kUserTerminated:
      return msgs::PresentationTerminationReason::kUserRequest;
    case TerminationReason::kReceiverPresentationReplaced:
      return msgs::PresentationTerminationReason::kReceiverReplacedPresentation;
    case TerminationReason::kReceiverIdleTooLong:
      return msgs::PresentationTerminationReason::kReceiverIdleTooLong;
    case TerminationReason::kReceiverPresentationUnloaded:
      return msgs::PresentationTerminationReason::kReceiverAttemptedToNavigate;
    case TerminationReason::kReceiverShuttingDown:
      return msgs::PresentationTerminationReason::kReceiverPoweringDown;
    case TerminationReason::kReceiverError:
      return msgs::PresentationTerminationReason::kReceiverError;
    default:
      return msgs::PresentationTerminationReason::kUnknown;
  }
}

}  // namespace openscreen::osp
