// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/public/presentation/presentation_connection.h"

#include <algorithm>
#include <memory>

#include "osp/impl/presentation/presentation_utils.h"
#include "osp/msgs/osp_messages.h"
#include "osp/public/network_service_manager.h"
#include "osp/public/presentation/presentation_common.h"
#include "osp/public/presentation/presentation_controller.h"
#include "osp/public/presentation/presentation_receiver.h"
#include "osp/public/protocol_connection.h"
#include "util/osp_logging.h"
#include "util/std_util.h"

namespace openscreen::osp {

Connection::Delegate::Delegate() = default;
Connection::Delegate::~Delegate() = default;

Connection::Controller::Controller() = default;
Connection::Controller::~Controller() = default;

Connection::Connection(const PresentationInfo& info,
                       Delegate* delegate,
                       Controller* controller)
    : presentation_info_(info),
      state_(State::kConnecting),
      delegate_(delegate),
      controller_(controller),
      connection_id_(0),
      protocol_connection_(nullptr) {}

Connection::~Connection() {
  if (state_ == State::kConnected) {
    Close(CloseReason::kDiscarded);
    delegate_->OnDiscarded();
  }
  controller_->OnConnectionDestroyed(this);
}

Error Connection::SendString(std::string_view message) {
  if (state_ != State::kConnected) {
    return Error::Code::kNoActiveConnection;
  }

  OSP_LOG_INFO << "sending '" << message << "' to (" << presentation_info_.id
               << ", " << connection_id_.value() << ")";
  msgs::PresentationConnectionMessage cbor_message;
  cbor_message.connection_id = connection_id_.value();
  cbor_message.message.which =
      msgs::PresentationConnectionMessage::Message::Which::kString;
  new (&cbor_message.message.str) std::string(message);

  return protocol_connection_->WriteMessage(
      cbor_message, &msgs::EncodePresentationConnectionMessage);
}

Error Connection::SendBinary(std::vector<uint8_t>&& data) {
  if (state_ != State::kConnected) {
    return Error::Code::kNoActiveConnection;
  }

  OSP_LOG_INFO << "sending " << data.size() << " bytes to ("
               << presentation_info_.id << ", " << connection_id_.value()
               << ")";
  msgs::PresentationConnectionMessage cbor_message;
  cbor_message.connection_id = connection_id_.value();
  cbor_message.message.which =
      msgs::PresentationConnectionMessage::Message::Which::kBytes;
  new (&cbor_message.message.bytes) std::vector<uint8_t>(std::move(data));

  return protocol_connection_->WriteMessage(
      cbor_message, &msgs::EncodePresentationConnectionMessage);
}

Error Connection::Close(CloseReason reason) {
  if (state_ == State::kClosed || state_ == State::kTerminated) {
    return Error::Code::kAlreadyClosed;
  }

  state_ = State::kClosed;
  protocol_connection_.reset();
  return controller_->CloseConnection(this, reason);
}

void Connection::Terminate(TerminationSource source, TerminationReason reason) {
  if (state_ == State::kTerminated) {
    return;
  }

  state_ = State::kTerminated;
  protocol_connection_.reset();
  controller_->OnPresentationTerminated(presentation_info_.id, source, reason);
}

void Connection::OnConnecting() {
  OSP_CHECK(!protocol_connection_);
  state_ = State::kConnecting;
}

void Connection::OnConnected(
    uint64_t connection_id,
    uint64_t instance_id,
    std::unique_ptr<ProtocolConnection> protocol_connection) {
  if (state_ != State::kConnecting) {
    return;
  }

  connection_id_ = connection_id;
  instance_id_ = instance_id;
  protocol_connection_ = std::move(protocol_connection);
  state_ = State::kConnected;
  delegate_->OnConnected();
}

void Connection::OnClosedByError(const Error& cause) {
  if (OnClosed()) {
    delegate_->OnError(cause.ToString());
  }
}

void Connection::OnClosedByRemote() {
  if (OnClosed()) {
    delegate_->OnClosedByRemote();
  }
}

void Connection::OnTerminated() {
  if (state_ == State::kTerminated) {
    return;
  }

  protocol_connection_.reset();
  state_ = State::kTerminated;
  delegate_->OnTerminated();
}

bool Connection::OnClosed() {
  if (state_ != State::kConnecting && state_ != State::kConnected) {
    return false;
  }

  protocol_connection_.reset();
  state_ = State::kClosed;
  return true;
}

ConnectionManager::ConnectionManager(MessageDemuxer& demuxer) {
  message_watch_ = demuxer.SetDefaultMessageTypeWatch(
      msgs::Type::kPresentationConnectionMessage, this);

  close_event_watch_ = demuxer.SetDefaultMessageTypeWatch(
      msgs::Type::kPresentationConnectionCloseEvent, this);
}

ConnectionManager::~ConnectionManager() = default;

void ConnectionManager::AddConnection(Connection* connection) {
  auto emplace_result =
      connections_.emplace(connection->connection_id(), connection);

  OSP_CHECK(emplace_result.second);
}

void ConnectionManager::RemoveConnection(Connection* connection) {
  auto entry = connections_.find(connection->connection_id());
  if (entry != connections_.end()) {
    connections_.erase(entry);
  }
}

// TODO(jophba): add a utility object to track requests/responses
// TODO(jophba): refine the RegisterWatch/OnStreamMessage API. We
// should add a layer between the message logic and the parse/dispatch
// logic, and remove the CBOR information from ConnectionManager.
ErrorOr<size_t> ConnectionManager::OnStreamMessage(uint64_t instance_id,
                                                   uint64_t connection_id,
                                                   msgs::Type message_type,
                                                   const uint8_t* buffer,
                                                   size_t buffer_size,
                                                   Clock::time_point now) {
  switch (message_type) {
    case msgs::Type::kPresentationConnectionMessage: {
      msgs::PresentationConnectionMessage message;
      const msgs::CborResult bytes_decoded =
          msgs::DecodePresentationConnectionMessage(buffer, buffer_size,
                                                    message);
      if (bytes_decoded < 0) {
        if (bytes_decoded == msgs::kParserEOF) {
          return Error::Code::kCborIncompleteMessage;
        }

        OSP_LOG_WARN << "presentation-connection-message parse error";
        return Error::Code::kParseError;
      }

      Connection* connection = GetConnection(message.connection_id);
      if (!connection) {
        return Error::Code::kItemNotFound;
      }

      switch (message.message.which) {
        case decltype(message.message.which)::kString: {
          connection->delegate()->OnStringMessage(message.message.str);
          break;
        }

        case decltype(message.message.which)::kBytes: {
          connection->delegate()->OnBinaryMessage(message.message.bytes);
          break;
        }

        default: {
          OSP_LOG_WARN << "uninitialized message data in "
                          "presentation-connection-message";
          break;
        }
      }
      return bytes_decoded;
    }

    case msgs::Type::kPresentationConnectionCloseEvent: {
      msgs::PresentationConnectionCloseEvent event;
      const msgs::CborResult bytes_decoded =
          msgs::DecodePresentationConnectionCloseEvent(buffer, buffer_size,
                                                       event);
      if (bytes_decoded < 0) {
        if (bytes_decoded == msgs::kParserEOF) {
          return Error::Code::kCborIncompleteMessage;
        }

        OSP_LOG_WARN << "decode presentation-connection-close-event error: "
                     << bytes_decoded;
        return Error::Code::kParseError;
      }

      Connection* connection = GetConnection(event.connection_id);
      if (!connection) {
        return Error::Code::kNoActiveConnection;
      }

      connection->OnClosedByRemote();
      return bytes_decoded;
    }

    // TODO(jophba): The spec says to close the connection if we get a message
    // we don't understand. Figure out how to honor the spec here.
    default:
      return Error::Code::kUnknownMessageType;
  }
}

Connection* ConnectionManager::GetConnection(uint64_t connection_id) {
  auto entry = connections_.find(connection_id);
  if (entry != connections_.end()) {
    return entry->second;
  }

  OSP_DVLOG << "unknown ID: " << connection_id;
  return nullptr;
}

}  // namespace openscreen::osp
