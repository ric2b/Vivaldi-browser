// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/public/presentation/presentation_receiver.h"

#include <algorithm>
#include <memory>

#include "osp/impl/presentation/presentation_id.h"
#include "osp/impl/presentation/presentation_utils.h"
#include "osp/msgs/osp_messages.h"
#include "osp/public/message_demuxer.h"
#include "osp/public/network_service_manager.h"
#include "osp/public/protocol_connection_server.h"
#include "platform/api/time.h"
#include "util/osp_logging.h"
#include "util/trace_logging.h"

namespace openscreen::osp {

ReceiverDelegate::ReceiverDelegate() = default;
ReceiverDelegate::~ReceiverDelegate() = default;

Receiver::Receiver() = default;
Receiver::~Receiver() = default;

Error Receiver::CloseConnection(Connection* connection,
                                Connection::CloseReason reason) {
  std::unique_ptr<ProtocolConnection> protocol_connection =
      CreateServerProtocolConnection(connection->instance_id());
  if (!protocol_connection) {
    return Error::Code::kNoActiveConnection;
  }

  msgs::PresentationConnectionCloseEvent event = {
      .connection_id = connection->connection_id(),
      .reason = ConvertCloseEventReason(reason),
      .connection_count = connection_manager_->ConnectionCount(),
      .has_error_message = false};
  return protocol_connection->WriteMessage(
      event, msgs::EncodePresentationConnectionCloseEvent);
}

Error Receiver::OnPresentationTerminated(const std::string& presentation_id,
                                         TerminationSource source,
                                         TerminationReason reason) {
  auto presentation_entry = started_presentations_by_id_.find(presentation_id);
  if (presentation_entry == started_presentations_by_id_.end()) {
    return Error::Code::kNoStartedPresentation;
  }

  Presentation& presentation = presentation_entry->second;
  presentation.terminate_watch.Reset();
  std::unique_ptr<ProtocolConnection> protocol_connection =
      CreateServerProtocolConnection(presentation.instance_id);
  if (!protocol_connection) {
    return Error::Code::kNoActiveConnection;
  }

  for (auto* connection : presentation.connections) {
    connection->OnTerminated();
  }

  if (presentation.terminate_request_id) {
    // TODO(btolsch): Also timeout if this point isn't reached.
    msgs::PresentationTerminationResponse response = {
        .request_id = presentation.terminate_request_id,
        .result = msgs::PresentationTerminationResponse_result::kSuccess};
    started_presentations_by_id_.erase(presentation_entry);
    return protocol_connection->WriteMessage(
        response, msgs::EncodePresentationTerminationResponse);
  }

  msgs::PresentationTerminationEvent event = {
      .presentation_id = presentation_id,
      .source = ConvertTerminationSource(source),
      .reason = ConvertTerminationReason(reason)};
  started_presentations_by_id_.erase(presentation_entry);
  return protocol_connection->WriteMessage(
      event, msgs::EncodePresentationTerminationEvent);
}

void Receiver::OnConnectionDestroyed(Connection* connection) {
  auto presentation_entry =
      started_presentations_by_id_.find(connection->presentation_info().id);
  if (presentation_entry == started_presentations_by_id_.end()) {
    return;
  }

  std::vector<Connection*>& connections =
      presentation_entry->second.connections;
  connections.erase(
      std::remove(connections.begin(), connections.end(), connection),
      connections.end());

  connection_manager_->RemoveConnection(connection);
}

ErrorOr<size_t> Receiver::OnStreamMessage(uint64_t instance_id,
                                          uint64_t connection_id,
                                          msgs::Type message_type,
                                          const uint8_t* buffer,
                                          size_t buffer_size,
                                          Clock::time_point now) {
  TRACE_SCOPED(TraceCategory::kPresentation, "Receiver::OnStreamMessage");
  switch (message_type) {
    case msgs::Type::kPresentationUrlAvailabilityRequest: {
      TRACE_SCOPED(TraceCategory::kPresentation,
                   "kPresentationUrlAvailabilityRequest");
      OSP_VLOG << "got presentation-url-availability-request";
      msgs::PresentationUrlAvailabilityRequest request;
      const msgs::CborResult decode_result =
          msgs::DecodePresentationUrlAvailabilityRequest(buffer, buffer_size,
                                                         request);
      if (decode_result < 0) {
        if (decode_result == msgs::kParserEOF) {
          return Error::Code::kCborIncompleteMessage;
        }

        OSP_LOG_WARN << "Presentation-url-availability-request parse error: "
                     << decode_result;
        TRACE_SET_RESULT(Error::Code::kParseError);
        return Error::Code::kParseError;
      }

      msgs::PresentationUrlAvailabilityResponse response = {
          .request_id = request.request_id,
          .url_availabilities = delegate_->OnUrlAvailabilityRequest(
              request.watch_id, request.watch_duration,
              std::move(request.urls))};
      CreateServerProtocolConnection(instance_id)
          ->WriteMessage(response,
                         msgs::EncodePresentationUrlAvailabilityResponse);
      return decode_result;
    }

    case msgs::Type::kPresentationStartRequest: {
      TRACE_SCOPED(TraceCategory::kPresentation, "kPresentationStartRequest");
      OSP_VLOG << "got presentation-start-request";
      msgs::PresentationStartRequest request;
      const msgs::CborResult result =
          msgs::DecodePresentationStartRequest(buffer, buffer_size, request);
      if (result < 0) {
        if (result == msgs::kParserEOF) {
          return Error::Code::kCborIncompleteMessage;
        }

        OSP_LOG_WARN << "Presentation-initiation-request parse error: "
                     << result;
        TRACE_SET_RESULT(Error::Code::kParseError);
        return Error::Code::kParseError;
      }

      OSP_LOG_INFO << "Got an initiation request for: " << request.url;
      PresentationID presentation_id(std::move(request.presentation_id));
      if (!presentation_id) {
        msgs::PresentationStartResponse response = {
            .request_id = request.request_id,
            .result =
                msgs::PresentationStartResponse_result::kInvalidPresentationId,
        };
        Error write_error =
            CreateServerProtocolConnection(instance_id)
                ->WriteMessage(response, msgs::EncodePresentationStartResponse);
        if (!write_error.ok()) {
          TRACE_SET_RESULT(write_error);
          return write_error;
        }

        return result;
      }

      auto& response_list = queued_responses_by_id_[presentation_id];
      QueuedResponse queued_response = {
          .type = QueuedResponse::Type::kInitiation,
          .request_id = request.request_id,
          .connection_id = this->GetNextConnectionId(),
          .instance_id = instance_id};
      response_list.push_back(std::move(queued_response));

      const bool starting = delegate_->StartPresentation(
          Connection::PresentationInfo{presentation_id, request.url},
          instance_id, request.headers);
      if (starting) {
        return result;
      }

      queued_responses_by_id_.erase(presentation_id);
      msgs::PresentationStartResponse response = {
          .request_id = request.request_id,
          .result = msgs::PresentationStartResponse_result::kUnknownError};
      Error write_error =
          CreateServerProtocolConnection(instance_id)
              ->WriteMessage(response, msgs::EncodePresentationStartResponse);
      if (!write_error.ok()) {
        TRACE_SET_RESULT(write_error);
        return write_error;
      }

      return result;
    }

    case msgs::Type::kPresentationConnectionOpenRequest: {
      TRACE_SCOPED(TraceCategory::kPresentation,
                   "kPresentationConnectionOpenRequest");
      OSP_VLOG << "Got a presentation-connection-open-request";
      msgs::PresentationConnectionOpenRequest request;
      const msgs::CborResult result =
          msgs::DecodePresentationConnectionOpenRequest(buffer, buffer_size,
                                                        request);
      if (result < 0) {
        if (result == msgs::kParserEOF) {
          return Error::Code::kCborIncompleteMessage;
        }

        OSP_LOG_WARN << "Presentation-connection-open-request parse error: "
                     << result;
        TRACE_SET_RESULT(Error::Code::kParseError);
        return Error::Code::kParseError;
      }

      PresentationID presentation_id(std::move(request.presentation_id));

      // TODO(jophba): add logic to queue presentation connection open
      // (and terminate connection)
      // requests to check against when a presentation starts, in case
      // we get a request right before the beginning of the presentation.
      if (!presentation_id ||
          started_presentations_by_id_.find(presentation_id) ==
              started_presentations_by_id_.end()) {
        msgs::PresentationConnectionOpenResponse response = {
            .request_id = request.request_id,
            .result = msgs::PresentationConnectionOpenResponse_result::
                kInvalidPresentationId};
        Error write_error =
            CreateServerProtocolConnection(instance_id)
                ->WriteMessage(response,
                               msgs::EncodePresentationConnectionOpenResponse);
        if (!write_error.ok()) {
          TRACE_SET_RESULT(write_error);
          return write_error;
        }

        return result;
      }

      // TODO(btolsch): We would also check that connection_id isn't already
      // requested/in use but since the spec has already shifted to a
      // receiver-chosen connection ID, we'll ignore that until we change our
      // CDDL messages.
      std::vector<QueuedResponse>& responses =
          queued_responses_by_id_[presentation_id];
      responses.emplace_back(
          QueuedResponse{QueuedResponse::Type::kConnection, request.request_id,
                         this->GetNextConnectionId(), instance_id});
      bool connecting = delegate_->ConnectToPresentation(
          request.request_id, presentation_id, instance_id);
      if (connecting) {
        return result;
      }

      responses.pop_back();
      if (responses.empty()) {
        queued_responses_by_id_.erase(presentation_id);
      }

      msgs::PresentationConnectionOpenResponse response = {
          .request_id = request.request_id,
          .result =
              msgs::PresentationConnectionOpenResponse_result::kUnknownError};
      Error write_error =
          CreateServerProtocolConnection(instance_id)
              ->WriteMessage(response,
                             msgs::EncodePresentationConnectionOpenResponse);
      if (!write_error.ok()) {
        TRACE_SET_RESULT(write_error);
        return write_error;
      }

      return result;
    }

    case msgs::Type::kPresentationTerminationRequest: {
      TRACE_SCOPED(TraceCategory::kPresentation,
                   "kPresentationTerminationRequest");
      OSP_VLOG << "got presentation-termination-request";
      msgs::PresentationTerminationRequest request;
      const msgs::CborResult result =
          msgs::DecodePresentationTerminationRequest(buffer, buffer_size,
                                                     request);
      if (result < 0) {
        if (result == msgs::kParserEOF) {
          return Error::Code::kCborIncompleteMessage;
        }

        OSP_LOG_WARN << "Presentation-termination-request parse error: "
                     << result;
        TRACE_SET_RESULT(Error::Code::kParseError);
        return Error::Code::kParseError;
      }

      PresentationID presentation_id(std::move(request.presentation_id));
      OSP_LOG_INFO << "Got termination request for: " << presentation_id;

      auto presentation_entry =
          started_presentations_by_id_.find(presentation_id);
      if (!presentation_id ||
          presentation_entry == started_presentations_by_id_.end()) {
        msgs::PresentationTerminationResponse response = {
            .request_id = request.request_id,
            .result = msgs::PresentationTerminationResponse_result::
                kInvalidPresentationId};
        Error write_error =
            CreateServerProtocolConnection(instance_id)
                ->WriteMessage(response,
                               msgs::EncodePresentationTerminationResponse);
        if (!write_error.ok()) {
          TRACE_SET_RESULT(write_error);
          return write_error;
        }

        return result;
      }

      TerminationReason reason =
          (request.reason ==
           msgs::PresentationTerminationReason::kApplicationRequest)
              ? TerminationReason::kApplicationTerminated
              : TerminationReason::kUserTerminated;
      presentation_entry->second.terminate_request_id = request.request_id;
      delegate_->TerminatePresentation(presentation_id,
                                       TerminationSource::kController, reason);
      return result;
    }

    default: {
      TRACE_SET_RESULT(Error::Code::kUnknownMessageType);
      return Error::Code::kUnknownMessageType;
    }
  }
}

void Receiver::Init() {
  if (!connection_manager_) {
    connection_manager_ =
        std::make_unique<ConnectionManager>(GetServerDemuxer());
  }
}

void Receiver::Deinit() {
  connection_manager_.reset();
}

void Receiver::SetReceiverDelegate(ReceiverDelegate* delegate) {
  OSP_CHECK(!delegate_ || !delegate);
  delegate_ = delegate;

  MessageDemuxer& demuxer = GetServerDemuxer();
  if (delegate_) {
    availability_watch_ = demuxer.SetDefaultMessageTypeWatch(
        msgs::Type::kPresentationUrlAvailabilityRequest, this);
    initiation_watch_ = demuxer.SetDefaultMessageTypeWatch(
        msgs::Type::kPresentationStartRequest, this);
    connection_watch_ = demuxer.SetDefaultMessageTypeWatch(
        msgs::Type::kPresentationConnectionOpenRequest, this);
    return;
  }

  availability_watch_.Reset();
  initiation_watch_.Reset();
  connection_watch_.Reset();

  std::vector<std::string> presentations_to_remove(
      started_presentations_by_id_.size());
  for (auto& it : started_presentations_by_id_) {
    presentations_to_remove.push_back(it.first);
  }

  for (auto& presentation_id : presentations_to_remove) {
    OnPresentationTerminated(presentation_id, TerminationSource::kReceiver,
                             TerminationReason::kReceiverShuttingDown);
  }
}

Error Receiver::OnPresentationStarted(const std::string& presentation_id,
                                      Connection* connection,
                                      ResponseResult result) {
  auto queued_responses_entry = queued_responses_by_id_.find(presentation_id);
  if (queued_responses_entry == queued_responses_by_id_.end()) {
    return Error::Code::kNoStartedPresentation;
  }

  auto& responses = queued_responses_entry->second;
  if ((responses.size() != 1) ||
      (responses.front().type != QueuedResponse::Type::kInitiation)) {
    return Error::Code::kPresentationAlreadyStarted;
  }

  QueuedResponse& initiation_response = responses.front();
  msgs::PresentationStartResponse response = {
      .request_id = initiation_response.request_id,
      .result = msgs::PresentationStartResponse_result::kUnknownError};
  auto protocol_connection =
      CreateServerProtocolConnection(initiation_response.instance_id);
  auto* raw_protocol_connection_ptr = protocol_connection.get();

  OSP_VLOG << "presentation started with protocol_connection id: "
           << protocol_connection->GetID();
  if (result != ResponseResult::kSuccess) {
    queued_responses_by_id_.erase(queued_responses_entry);
    return raw_protocol_connection_ptr->WriteMessage(
        response, msgs::EncodePresentationStartResponse);
  }

  response.result = msgs::PresentationStartResponse_result::kSuccess;
  response.connection_id = connection->connection_id();

  Presentation& presentation = started_presentations_by_id_[presentation_id];
  presentation.instance_id = initiation_response.instance_id;
  connection->OnConnected(initiation_response.connection_id,
                          initiation_response.instance_id,
                          std::move(protocol_connection));
  presentation.connections.push_back(connection);
  connection_manager_->AddConnection(connection);

  presentation.terminate_watch = GetServerDemuxer().WatchMessageType(
      initiation_response.instance_id,
      msgs::Type::kPresentationTerminationRequest, this);

  queued_responses_by_id_.erase(queued_responses_entry);
  return raw_protocol_connection_ptr->WriteMessage(
      response, msgs::EncodePresentationStartResponse);
}

Error Receiver::OnConnectionCreated(uint64_t request_id,
                                    Connection* connection,
                                    ResponseResult result) {
  const auto presentation_id = connection->presentation_info().id;
  ErrorOr<QueuedResponseIterator> connection_response =
      GetQueuedResponse(presentation_id, request_id);
  if (connection_response.is_error()) {
    return connection_response.error();
  }

  connection->OnConnected(
      connection_response.value()->connection_id,
      connection_response.value()->instance_id,
      CreateServerProtocolConnection(connection_response.value()->instance_id));

  started_presentations_by_id_[presentation_id].connections.push_back(
      connection);
  connection_manager_->AddConnection(connection);

  msgs::PresentationConnectionOpenResponse response = {
      .request_id = request_id,
      .result = msgs::PresentationConnectionOpenResponse_result::kSuccess,
      .connection_id = connection->connection_id()};
  auto protocol_connection =
      CreateServerProtocolConnection(connection_response.value()->instance_id);
  protocol_connection->WriteMessage(
      response, msgs::EncodePresentationConnectionOpenResponse);

  DeleteQueuedResponse(presentation_id, connection_response.value());
  return Error::None();
}

void Receiver::DeleteQueuedResponse(const std::string& presentation_id,
                                    Receiver::QueuedResponseIterator response) {
  auto entry = queued_responses_by_id_.find(presentation_id);
  entry->second.erase(response);
  if (entry->second.empty()) {
    queued_responses_by_id_.erase(entry);
  }
}

ErrorOr<Receiver::QueuedResponseIterator> Receiver::GetQueuedResponse(
    const std::string& presentation_id,
    uint64_t request_id) const {
  auto entry = queued_responses_by_id_.find(presentation_id);
  if (entry == queued_responses_by_id_.end()) {
    OSP_LOG_WARN << "connection created for unknown request";
    return Error::Code::kUnknownRequestId;
  }

  const std::vector<QueuedResponse>& responses = entry->second;
  Receiver::QueuedResponseIterator it =
      std::find_if(responses.begin(), responses.end(),
                   [request_id](const QueuedResponse& response) {
                     return response.request_id == request_id;
                   });
  if (it == responses.end()) {
    OSP_LOG_WARN << "connection created for unknown request";
    return Error::Code::kUnknownRequestId;
  }

  return it;
}

uint64_t Receiver::GetNextConnectionId() {
  static uint64_t request_id = 0;
  return request_id++;
}

}  // namespace openscreen::osp
