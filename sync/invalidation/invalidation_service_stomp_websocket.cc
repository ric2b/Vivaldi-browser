// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "sync/invalidation/invalidation_service_stomp_websocket.h"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "url/origin.h"

namespace vivaldi {
namespace {
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("vivaldi_sync_notification_client",
                                        R"(
        semantics {
          sender: "Vivaldi Sync Notification Client"
          description:
            "This websocket connection is used by Vivaldi sync to be notified "
            "of changes to sync data by the sync server, in order to know when "
            "to request an update."
          trigger:
            "This websocket connection is set up when a user logs in to sync "
            "and is terminated when the user logs out. It resumes "
            "automatically after a browser restarts when sync is active."
          data:
            "Notifications about which sync types have received changes."
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings, but if user signs "
            "out of sync, this connection would not be established."
        })");

constexpr char kStomp12Protocol[] = "v12.stomp";

// Upper-bound expected size for an invalidation frames are unlikely to exceed
// 1KiB with current server implementation. Accept frames up to 4KiB to be
// safe. This includes the stomp frame type, headers and the actual message.
constexpr size_t kMaxInvalidationFrameSize = 1 << 12;

constexpr char kConnectFrame[] = R"(STOMP
accept-version:1.2
host:%s
login:%s
heart-beat:10000,10000

)";

constexpr base::TimeDelta kHeartBeatDelay = base::Milliseconds(10000);
constexpr base::TimeDelta kHeartBeatGrace = base::Milliseconds(5000);

constexpr char kSubscribeFrame[] = R"(SUBSCRIBE
id:0
destination:%s
receipt:sync-subscribed

)";

constexpr char kHeartBeatFrame[] = "\n";

constexpr char kLf[] = "\n";
constexpr char kCrLf[] = "\r\n";

constexpr char kConnectedCommand[] = "CONNECTED";
constexpr char kReceiptCommand[] = "RECEIPT";
constexpr char kMessageCommand[] = "MESSAGE";
constexpr char kVersionHeader[] = "version";
constexpr char kStompVersion[] = "1.2";
constexpr char kHeartBeatHeader[] = "heart-beat";
constexpr char kReceiptIdHeader[] = "receipt-id";
constexpr char kExpectedSubscriptionReceipt[] = "sync-subscribed";
constexpr char kContentLengthHeader[] = "content-length";
}  // namespace

InvalidationServiceStompWebsocket::Client::~Client() = default;

InvalidationServiceStompWebsocket::InvalidationServiceStompWebsocket(
    network::mojom::NetworkContext* network_context,
    const GURL& url,
    Client* client)
    : url_(url),
      client_(client),
      readable_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      writable_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL) {
  DCHECK(client_);
  url::Origin origin = url::Origin::Create(url);
  std::vector<network::mojom::HttpHeaderPtr> headers;
  auto remote = handshake_receiver_.BindNewPipeAndPassRemote();
  // Unretained ok, because the disconnect handler is only called until unbound.
  handshake_receiver_.set_disconnect_handler(
      base::BindOnce(&InvalidationServiceStompWebsocket::OnMojoPipeDisconnect,
                     base::Unretained(this)));
  network_context->CreateWebSocket(
      url, {kStomp12Protocol}, net::SiteForCookies(),
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                 origin, origin, net::SiteForCookies()),
      std::move(headers), network::mojom::kBrowserProcessId,
      url::Origin::Create(url), network::mojom::kWebSocketOptionBlockAllCookies,
      net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation),
      std::move(remote),
      /*auth_cert_observer=*/mojo::NullRemote(),
      /*auth_handler=*/mojo::NullRemote(),
      /*header_client=*/mojo::NullRemote(),
      /*throttling_profile_id=*/absl::nullopt);
}

InvalidationServiceStompWebsocket::~InvalidationServiceStompWebsocket() {
  // Stomp normally calls for sending a DISCONNECT frame when going away,
  // but that only matters if we want to make sure that the server has reveived
  // all frames from our side. Since we don't send any actual message or acks,
  // closing the socket is good enough.
  if (websocket_.is_bound())
    websocket_->StartClosingHandshake(1000, "Sync shutting down");
}

void InvalidationServiceStompWebsocket::OnOpeningHandshakeStarted(
    network::mojom::WebSocketHandshakeRequestPtr request) {}
void InvalidationServiceStompWebsocket::OnFailure(const std::string& message,
                                                  int net_error,
                                                  int response_code) {
  client_->OnClosed();
}
void InvalidationServiceStompWebsocket::OnConnectionEstablished(
    mojo::PendingRemote<network::mojom::WebSocket> socket,
    mojo::PendingReceiver<network::mojom::WebSocketClient> client_receiver,
    network::mojom::WebSocketHandshakeResponsePtr response,
    mojo::ScopedDataPipeConsumerHandle readable,
    mojo::ScopedDataPipeProducerHandle writable) {
  if (response->selected_protocol != kStomp12Protocol) {
    LOG(ERROR) << "Sync notification server selected wrong protocol";
    return;
  }

  // Unretained ok, because the callback is owned by the watchers, which are
  // owned by this.
  websocket_.Bind(std::move(socket));
  readable_ = std::move(readable);
  MojoResult watch_result = readable_watcher_.Watch(
      readable_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&InvalidationServiceStompWebsocket::OnReadable,
                          base::Unretained(this)));
  DCHECK(watch_result == MOJO_RESULT_OK);

  writable_ = std::move(writable);
  watch_result = writable_watcher_.Watch(
      writable_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&InvalidationServiceStompWebsocket::OnWritable,
                          base::Unretained(this)));
  DCHECK(watch_result == MOJO_RESULT_OK);

  client_receiver_.Bind(std::move(client_receiver));

  // |handshake_receiver_| will disconnect soon. In order to catch network
  // process crashes, we switch to watching |client_receiver_|.
  handshake_receiver_.set_disconnect_handler(base::DoNothing());
  // Unretained ok, because the disconnect handler is only called until unbound.
  client_receiver_.set_disconnect_handler(
      base::BindOnce(&InvalidationServiceStompWebsocket::OnMojoPipeDisconnect,
                     base::Unretained(this)));

  websocket_->StartReceiving();
  Send(base::StringPrintf(kConnectFrame, client_->GetVHost().c_str(),
                          client_->GetLogin().c_str()));
}

void InvalidationServiceStompWebsocket::OnDataFrame(
    bool finish,
    network::mojom::WebSocketMessageType type,
    uint64_t data_len) {
  // Non-final frames cannot be empty.
  DCHECK(finish || data_len > 0);
  if ((type != network::mojom::WebSocketMessageType::TEXT &&
       type != network::mojom::WebSocketMessageType::CONTINUATION) ||
      data_len > kMaxInvalidationFrameSize)
    OnClose();

  DCHECK(incoming_ || type == network::mojom::WebSocketMessageType::TEXT);

  if (!incoming_) {
    incoming_sizes_.push(0);
    incoming_ = true;
  }

  if (finish) {
    incoming_ = false;
  }
  // No overflow here because both values are bounded by
  // kMaxInvalidationFrameSize
  incoming_sizes_.back() += data_len;
  if (incoming_sizes_.back() > kMaxInvalidationFrameSize)
    OnClose();

  ProcessIncoming();
}

void InvalidationServiceStompWebsocket::OnDropChannel(
    bool was_clean,
    uint16_t code,
    const std::string& reason) {
  OnClose();
}

void InvalidationServiceStompWebsocket::OnClosingHandshake() {}

void InvalidationServiceStompWebsocket::ProcessIncoming() {
  while (!incoming_sizes_.empty()) {
    CHECK(incoming_sizes_.front() >= incoming_message_.length());
    uint32_t remaining_size = base::checked_cast<uint32_t>(
        incoming_sizes_.front() - incoming_message_.length());
    if (remaining_size == 0)
      return;

    const void* buffer;
    uint32_t available_size;
    MojoResult result = readable_->BeginReadData(&buffer, &available_size,
                                                 MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      readable_watcher_.ArmOrNotify();
      return;
    }
    if (result != MOJO_RESULT_OK) {
      // |client_receiver_| will catch the connection error.
      return;
    }

    uint32_t read_size = std::min(remaining_size, available_size);
    incoming_message_.append(static_cast<const char*>(buffer), read_size);
    readable_->EndReadData(read_size);

    // If incoming is true and there is no further message to process,
    // we are still waiting for more chunks of this message.
    if (read_size == remaining_size &&
        (incoming_sizes_.size() > 1 || !incoming_)) {
      if (!HandleFrame()) {
        OnClose();
        return;
      }
      incoming_message_.clear();
      incoming_sizes_.pop();
    }
  }
}

void InvalidationServiceStompWebsocket::Send(std::string message) {
  DCHECK(!message.empty());
  // Stomp frames must end with a NUL byte
  message.push_back('\0');
  outgoing_messages_.push(std::move(message));
  ProcessOutgoing();
}

void InvalidationServiceStompWebsocket::ProcessOutgoing() {
  if (!is_writable_ready_ || !websocket_.is_bound() || !writable_.is_valid())
    return;

  while (!outgoing_messages_.empty()) {
    if (remaining_outgoing_size_ == 0) {
      // Size includes the terminating NUL-byte
      remaining_outgoing_size_ = outgoing_messages_.front().size();
      websocket_->SendMessage(network::mojom::WebSocketMessageType::TEXT,
                              remaining_outgoing_size_);
    }

    base::StringPiece outgoing(outgoing_messages_.front());
    DCHECK_LE(remaining_outgoing_size_, outgoing.size());

    uint32_t num_bytes = static_cast<uint32_t>(remaining_outgoing_size_);
    MojoResult result =
        writable_->WriteData(outgoing.end() - remaining_outgoing_size_,
                             &num_bytes, MOJO_WRITE_DATA_FLAG_NONE);

    if (result == MOJO_RESULT_SHOULD_WAIT) {
      is_writable_ready_ = false;
      writable_watcher_.ArmOrNotify();
      break;
    }

    if (result != MOJO_RESULT_OK) {
      OnClose();
      return;
    }
    DCHECK_LE(num_bytes, remaining_outgoing_size_);
    remaining_outgoing_size_ -= num_bytes;

    if (remaining_outgoing_size_ == 0) {
      outgoing_messages_.pop();
    }
  }
}

void InvalidationServiceStompWebsocket::OnMojoPipeDisconnect() {
  if (websocket_.is_bound() || client_receiver_.is_bound())
    OnClose();
  else
    client_->OnClosed();
}

void InvalidationServiceStompWebsocket::OnReadable(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  if (result != MOJO_RESULT_OK) {
    // We don't detect mojo errors on data pipe. Mojo connection errors will
    // be detected via |client_receiver_|.
    return;
  }

  ProcessIncoming();
}

void InvalidationServiceStompWebsocket::OnWritable(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  if (result != MOJO_RESULT_OK) {
    // We don't detect mojo errors on data pipe. Mojo connection errors will
    // be detected via |client_receiver_|.
    return;
  }

  is_writable_ready_ = true;
  ProcessOutgoing();
}

void InvalidationServiceStompWebsocket::OnClose() {
  websocket_.reset();
  client_receiver_.reset();
  readable_watcher_.Cancel();
  writable_watcher_.Cancel();
  client_->OnClosed();
}

bool InvalidationServiceStompWebsocket::HandleFrame() {
  if (heart_beats_in_timer_.IsRunning())
    heart_beats_in_timer_.Reset();
  if (incoming_message_ == kLf || incoming_message_ == kCrLf)
    return true;
  base::StringPiece incoming_message(incoming_message_);

  const char* line_ending = kLf;
  int line_ending_length = 1;
  size_t header_end = incoming_message.find("\n\n");
  if (header_end == base::StringPiece::npos) {
    line_ending = kCrLf;
    line_ending_length = 2;
    header_end = incoming_message.find("\r\n\r\n");
    if (header_end == base::StringPiece::npos)
      return false;
  }

  base::StringPiece command;
  std::map<base::StringPiece, base::StringPiece> headers;

  std::vector<base::StringPiece> header_lines = SplitStringPieceUsingSubstr(
      incoming_message.substr(0, header_end), line_ending,
      base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  if (header_lines.empty())
    return false;

  command = header_lines[0];
  header_lines.erase(header_lines.begin());

  for (base::StringPiece header_line : header_lines) {
    size_t separator = header_line.find(':');
    if (separator == base::StringPiece::npos)
      return false;
    headers[header_line.substr(0, separator)] =
        header_line.substr(separator + 1);
  }

  if (command == kConnectedCommand) {
    if (stomp_state_ != StompState::kConnecting)
      return false;

    const auto& version_header = headers.find(kVersionHeader);
    if (version_header == headers.end() ||
        version_header->second != kStompVersion)
      return false;

    const auto& heart_beat_header = headers.find(kHeartBeatHeader);
    if (heart_beat_header != headers.end()) {
      std::vector<base::StringPiece> heart_beat_delays = base::SplitStringPiece(
          heart_beat_header->second, ",", base::TRIM_WHITESPACE,
          base::SPLIT_WANT_NONEMPTY);
      if (heart_beat_delays.size() != 2)
        return false;

      // Unretained for the timers is fine, since they own the callbacks and are
      // owned by this
      int server_heart_beats_delay_in;
      if (!base::StringToInt(heart_beat_delays[0],
                             &server_heart_beats_delay_in))
        return false;
      if (server_heart_beats_delay_in != 0) {
        base::TimeDelta heart_beats_delay_in =
            std::max(kHeartBeatDelay,
                     base::Milliseconds(server_heart_beats_delay_in)) +
            kHeartBeatGrace;
        heart_beats_in_timer_.Start(
            FROM_HERE, heart_beats_delay_in,
            base::BindOnce(&InvalidationServiceStompWebsocket::OnClose,
                           base::Unretained(this)));
      }

      int server_heart_beats_delay_out;
      if (!base::StringToInt(heart_beat_delays[1],
                             &server_heart_beats_delay_out))
        return false;
      if (server_heart_beats_delay_out != 0) {
        base::TimeDelta heart_beats_delay_out =
            std::max(kHeartBeatDelay,
                     base::Milliseconds(server_heart_beats_delay_out)) -
            kHeartBeatGrace;
        heart_beats_out_timer_.Start(
            FROM_HERE, heart_beats_delay_out,
            base::BindRepeating(&InvalidationServiceStompWebsocket::Send,
                                base::Unretained(this), kHeartBeatFrame));
      }
    }

    stomp_state_ = StompState::kSubscribing;
    Send(base::StringPrintf(kSubscribeFrame, client_->GetChannel().c_str()));
  } else if (command == kReceiptCommand) {
    const auto& receipt_id_header = headers.find(kReceiptIdHeader);
    if (receipt_id_header == headers.end())
      return false;
    if (stomp_state_ == StompState::kSubscribing &&
        receipt_id_header->second == kExpectedSubscriptionReceipt) {
      stomp_state_ = StompState::kConnected;
      client_->OnConnected();
    }
    // We shouldn't be receiving any other kind of receipt, but it isn't
    // strictly an error if we do.
  } else if (command == kMessageCommand) {
    const auto& content_length_header = headers.find(kContentLengthHeader);
    base::StringPiece body =
        incoming_message.substr(header_end + 2 * line_ending_length);
    size_t body_end;
    if (content_length_header == headers.end()) {
      body_end = body.find('\0');
      // Frames are supposed to always end with a NUL-byte
      if (body_end == base::StringPiece::npos)
        return false;
    } else {
      if (!base::StringToSizeT(content_length_header->second, &body_end))
        return false;
    }
    body = body.substr(0, body_end);

    absl::optional<base::Value> invalidation = base::JSONReader::Read(body);
    if (invalidation && invalidation->is_dict())
      client_->OnInvalidation(std::move(invalidation->GetDict()));
  } else {
    // Either we received an ERROR frame or a malformed one. In either case,
    // we are done.
    return false;
  }

  return true;
}

}  // namespace vivaldi
