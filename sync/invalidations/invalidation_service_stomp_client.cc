// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include "sync/invalidations/invalidation_service_stomp_client.h"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "sync/invalidations/stomp_constants.h"
#include "sync/invalidations/stomp_frame_builder.h"

namespace vivaldi {
namespace {
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("vivaldi_sync_notification_client",
                                        R"(
        semantics {
          sender: "Vivaldi Sync Notification Client"
          description:
            "This connection is used by Vivaldi sync to be notified of changes "
            "to sync data by the sync server, in order to know when to request "
            " an update."
          trigger:
            "This connection is set up when a user logs in to sync and is "
            "terminated when the user logs out. It resumes automatically after "
            "a browser restarts when sync is active."
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

constexpr char kConnectFrame[] = R"(STOMP
accept-version:1.2
host:%s
login:%s
heart-beat:20000,20000

)";

constexpr base::TimeDelta kHeartBeatDelay = base::Milliseconds(20000);
constexpr base::TimeDelta kHeartBeatGrace = base::Milliseconds(5000);

constexpr char kSubscribeFrame[] = R"(SUBSCRIBE
id:0
destination:%s
receipt:sync-subscribed

)";

constexpr char kHeartBeatFrame[] = {stomp::kLf};
}  // namespace

InvalidationServiceStompClient::Delegate::~Delegate() = default;

InvalidationServiceStompClient::InvalidationServiceStompClient(
    network::mojom::NetworkContext* network_context,
    const GURL& url,
    Delegate* delegate)
    : delegate_(delegate),
      readable_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      writable_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL) {
  DCHECK(delegate_);
  network_context->CreateProxyResolvingSocketFactory(
      socket_factory_.BindNewPipeAndPassReceiver());

  network::mojom::ProxyResolvingSocketOptionsPtr options =
      network::mojom::ProxyResolvingSocketOptions::New();
  options->use_tls = url.scheme() == "stomps";

  auto site = net::SchemefulSite(url);
  socket_factory_->CreateProxyResolvingSocket(
      url, net::NetworkAnonymizationKey::CreateSameSite(site),
      std::move(options),
      net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation),
      socket_.BindNewPipeAndPassReceiver(),
      socket_observer_receiver_.BindNewPipeAndPassRemote(),
      base::BindOnce(&InvalidationServiceStompClient::OnConnectionEstablished,
                     base::Unretained(this)));
}

InvalidationServiceStompClient::~InvalidationServiceStompClient() {
  // Stomp normally calls for sending a DISCONNECT frame when going away,
  // but that only matters if we want to make sure that the server has received
  // all frames from our side. Since we don't send any actual message or ack,
  // closing the socket is good enough.
}

void InvalidationServiceStompClient::OnConnectionEstablished(
    int result,
    const std::optional<net::IPEndPoint>& local_addr,
    const std::optional<net::IPEndPoint>& peer_addr,
    mojo::ScopedDataPipeConsumerHandle readable,
    mojo::ScopedDataPipeProducerHandle writable) {
  if (result != net::OK) {
    delegate_->OnClosed();
    return;
  }

  // Unretained ok, because the callback is owned by the watchers, which are
  // owned by this.
  readable_ = std::move(readable);
  MojoResult watch_result = readable_watcher_.Watch(
      readable_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&InvalidationServiceStompClient::OnReadable,
                          base::Unretained(this)));
  DCHECK(watch_result == MOJO_RESULT_OK);

  writable_ = std::move(writable);
  watch_result = writable_watcher_.Watch(
      writable_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&InvalidationServiceStompClient::OnWritable,
                          base::Unretained(this)));
  DCHECK(watch_result == MOJO_RESULT_OK);

  incoming_frames_ = std::make_unique<stomp::StompFrameBuilder>();

  Send(base::StringPrintf(kConnectFrame, delegate_->GetVHost().c_str(),
                          delegate_->GetLogin().c_str()));

  readable_watcher_.ArmOrNotify();
}

void InvalidationServiceStompClient::OnReadError(int32_t net_error) {
  OnClose();
}
void InvalidationServiceStompClient::OnWriteError(int32_t net_error) {
  OnClose();
}

void InvalidationServiceStompClient::ProcessIncoming() {
  do {
    base::span<const uint8_t> buffer;
    MojoResult result =
        readable_->BeginReadData(MOJO_READ_DATA_FLAG_NONE, buffer);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      readable_watcher_.ArmOrNotify();
      return;
    }
    if (result != MOJO_RESULT_OK || buffer.size() == 0) {
      // Connection error or EOF. In practice, we do not expect the server to
      // terminate the connection, so they are both treated identically. We will
      // request a new connection regardless.
      OnClose();
      return;
    }

    if (heart_beats_in_timer_.IsRunning()) {
      heart_beats_in_timer_.Reset();
    }

    std::string_view incoming = base::as_string_view(buffer);
    if (!incoming_frames_->ProcessIncoming(incoming)) {
      OnClose();
      return;
    }

    while (incoming_frames_->IsComplete()) {
      std::unique_ptr<stomp::StompFrameBuilder> complete_frame =
          std::move(incoming_frames_);
      incoming_frames_ = complete_frame->TakeOverNextframe();
      if (!HandleFrame(std::move(complete_frame))) {
        OnClose();
        return;
      }
    }

    readable_->EndReadData(buffer.size());
  } while (true);
}

void InvalidationServiceStompClient::Send(std::string message) {
  DCHECK(!message.empty());
  // Stomp frames must end with a NUL byte.
  message.push_back(stomp::kNul);
  SendRaw(std::move(message));
}

void InvalidationServiceStompClient::SendRaw(std::string message) {
  DCHECK(!message.empty());
  outgoing_messages_.push(std::move(message));
  ProcessOutgoing();
}

void InvalidationServiceStompClient::ProcessOutgoing() {
  if (!is_writable_ready_ || !writable_.is_valid())
    return;

  while (!outgoing_messages_.empty()) {
    if (remaining_outgoing_size_ == 0) {
      // Size includes the terminating NUL-byte.
      remaining_outgoing_size_ = outgoing_messages_.front().size();
    }

    auto outgoing = base::as_byte_span(outgoing_messages_.front());
    DCHECK_LE(remaining_outgoing_size_, outgoing.size());

    size_t written;
    MojoResult result = writable_->WriteData(
        outgoing.subspan(outgoing.size() - remaining_outgoing_size_),
        MOJO_WRITE_DATA_FLAG_NONE, written);

    if (result == MOJO_RESULT_SHOULD_WAIT) {
      is_writable_ready_ = false;
      writable_watcher_.ArmOrNotify();
      break;
    }

    if (result != MOJO_RESULT_OK) {
      OnClose();
      return;
    }
    DCHECK_LE(written, remaining_outgoing_size_);
    remaining_outgoing_size_ -= written;

    if (remaining_outgoing_size_ == 0) {
      outgoing_messages_.pop();
    }
  }
}

void InvalidationServiceStompClient::OnReadable(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  if (result != MOJO_RESULT_OK) {
    return;
  }

  ProcessIncoming();
}

void InvalidationServiceStompClient::OnWritable(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  if (result != MOJO_RESULT_OK) {
    return;
  }

  is_writable_ready_ = true;
  ProcessOutgoing();
}

void InvalidationServiceStompClient::OnClose() {
  readable_watcher_.Cancel();
  writable_watcher_.Cancel();
  delegate_->OnClosed();
}

bool InvalidationServiceStompClient::HandleFrame(
    std::unique_ptr<stomp::StompFrameBuilder> frame) {
  if (frame->command() == stomp::kConnectedCommand) {
    if (stomp_state_ != StompState::kConnecting)
      return false;

    const auto& version_header = frame->headers().find(stomp::kVersionHeader);
    if (version_header == frame->headers().end() ||
        version_header->second != stomp::kStompVersion)
      return false;

    const auto& session_header = frame->headers().find(stomp::kSessionHeader);
    if (session_header == frame->headers().end()) {
      return false;
    }
    session_id_ = session_header->second;

    const auto& heart_beat_header =
        frame->headers().find(stomp::kHeartBeatHeader);
    if (heart_beat_header != frame->headers().end()) {
      std::vector<std::string_view> heart_beat_delays = base::SplitStringPiece(
          heart_beat_header->second, ",", base::TRIM_WHITESPACE,
          base::SPLIT_WANT_NONEMPTY);
      if (heart_beat_delays.size() != 2)
        return false;

      // Unretained for the timers is fine, since they own the callbacks and are
      // owned by this.
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
            base::BindOnce(&InvalidationServiceStompClient::OnClose,
                           base::Unretained(this)));
      }

      int server_heart_beats_delay_out;
      if (!base::StringToInt(heart_beat_delays[1],
                             &server_heart_beats_delay_out))
        return false;
      if (server_heart_beats_delay_out != 0) {
        base::TimeDelta heart_beats_delay_out = std::max(
            kHeartBeatDelay, base::Milliseconds(server_heart_beats_delay_out));
        heart_beats_out_timer_.Start(
            FROM_HERE, heart_beats_delay_out,
            base::BindRepeating(&InvalidationServiceStompClient::SendRaw,
                                base::Unretained(this), kHeartBeatFrame));
      }
    }

    stomp_state_ = StompState::kSubscribing;
    Send(base::StringPrintf(kSubscribeFrame, delegate_->GetChannel().c_str()));
  } else if (frame->command() == stomp::kReceiptCommand) {
    const auto& receipt_id_header =
        frame->headers().find(stomp::kReceiptIdHeader);
    if (receipt_id_header == frame->headers().end())
      return false;
    if (stomp_state_ == StompState::kSubscribing &&
        receipt_id_header->second == stomp::kExpectedSubscriptionReceipt) {
      stomp_state_ = StompState::kConnected;
      delegate_->OnConnected();
    }
    // We shouldn't be receiving any other kind of receipt, but it isn't
    // strictly an error if we do.
  } else if (frame->command() == stomp::kMessageCommand) {
    std::string message;
    if (!base::Base64Decode(frame->body(), &message)) {
      return false;
    }
    delegate_->OnInvalidation(message);
  } else {
    // Either we received an ERROR frame, a DISCONNECT frame or a malformed one.
    // In either case, we are done.
    return false;
  }

  return true;
}

}  // namespace vivaldi
