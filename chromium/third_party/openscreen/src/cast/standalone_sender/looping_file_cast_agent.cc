// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_sender/looping_file_cast_agent.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "build/build_config.h"
#include "cast/common/channel/message_util.h"
#include "cast/common/public/cast_streaming_app_ids.h"
#include "cast/standalone_sender/looping_file_sender.h"
#include "cast/streaming/public/capture_recommendations.h"
#include "cast/streaming/public/constants.h"
#include "cast/streaming/public/offer_messages.h"
#include "json/value.h"
#include "platform/api/tls_connection_factory.h"
#include "util/json/json_helpers.h"
#include "util/stringprintf.h"
#include "util/trace_logging.h"

namespace openscreen::cast {
namespace {

using DeviceMediaPolicy = SenderSocketFactory::DeviceMediaPolicy;

}  // namespace

LoopingFileCastAgent::LoopingFileCastAgent(
    TaskRunner& task_runner,
    std::unique_ptr<TrustStore> cast_trust_store,
    ShutdownCallback shutdown_callback)
    : task_runner_(task_runner),
      shutdown_callback_(std::move(shutdown_callback)),
      connection_handler_(router_, *this),
      socket_factory_(*this,
                      task_runner_,
                      std::move(cast_trust_store),
                      CastCRLTrustStore::Create()),
      connection_factory_(
          TlsConnectionFactory::CreateFactory(socket_factory_, task_runner_)),
      message_port_(router_) {
  router_.AddHandlerForLocalId(kPlatformSenderId, this);
  socket_factory_.set_factory(connection_factory_.get());
}

LoopingFileCastAgent::~LoopingFileCastAgent() {
  Shutdown();
}

void LoopingFileCastAgent::Connect(ConnectionSettings settings) {
  TRACE_DEFAULT_SCOPED(TraceCategory::kStandaloneSender);

  OSP_CHECK(!connection_settings_);
  connection_settings_ = std::move(settings);
  const auto policy = connection_settings_->should_include_video
                          ? DeviceMediaPolicy::kIncludesVideo
                          : DeviceMediaPolicy::kAudioOnly;

  task_runner_.PostTask([this, policy] {
#if BUILDFLAG(IS_APPLE)
    wake_lock_ = ScopedWakeLock::Create(task_runner_);
#endif  // BUILDFLAG(IS_APPLE)
    socket_factory_.Connect(connection_settings_->receiver_endpoint, policy,
                            &router_);
  });
}

void LoopingFileCastAgent::OnConnected(SenderSocketFactory* factory,
                                       const IPEndpoint& endpoint,
                                       std::unique_ptr<CastSocket> socket) {
  TRACE_DEFAULT_SCOPED(TraceCategory::kStandaloneSender);

  if (message_port_.GetSocketId() != ToCastSocketId(nullptr)) {
    OSP_LOG_WARN << "Already connected, dropping peer at: " << endpoint;
    return;
  }
  message_port_.SetSocket(socket->GetWeakPtr());
  router_.TakeSocket(this, std::move(socket));

  OSP_LOG_INFO << "Launching Mirroring App on the Cast Receiver...";
  // First, CONNECT to the platform receiver.
  platform_remote_connection_.emplace(VirtualConnection{
      kPlatformSenderId, kPlatformReceiverId, message_port_.GetSocketId()});
  connection_handler_.OpenRemoteConnection(
      *platform_remote_connection_,
      [this](bool success) { OnReceiverMessagingOpened(success); });
}

void LoopingFileCastAgent::OnError(SenderSocketFactory* factory,
                                   const IPEndpoint& endpoint,
                                   const Error& error) {
  OSP_LOG_ERROR << "Cast agent received socket factory error: " << error;
  Shutdown();
}

void LoopingFileCastAgent::OnClose(CastSocket* cast_socket) {
  OSP_VLOG << "Cast agent socket closed.";
  Shutdown();
}

void LoopingFileCastAgent::OnError(CastSocket* socket, const Error& error) {
  OSP_LOG_ERROR << "Cast agent received socket error: " << error;
  Shutdown();
}

bool LoopingFileCastAgent::IsConnectionAllowed(
    const VirtualConnection& virtual_conn) const {
  return true;
}

void LoopingFileCastAgent::OnMessage(VirtualConnectionRouter* router,
                                     CastSocket* socket,
                                     proto::CastMessage message) {
  if (message_port_.GetSocketId() == ToCastSocketId(socket) &&
      !message_port_.source_id().empty() &&
      message_port_.source_id() == message.destination_id()) {
    OSP_CHECK_NE(message.destination_id(), kPlatformSenderId);
    message_port_.OnMessage(router, socket, std::move(message));
    return;
  }

  if (message.destination_id() != kPlatformSenderId &&
      message.destination_id() != kBroadcastId) {
    return;  // Message not for us.
  }

  if (message.namespace_() == kReceiverNamespace &&
      message_port_.GetSocketId() == ToCastSocketId(socket)) {
    if (message.payload_type() != proto::CastMessage::STRING) {
      OSP_DLOG_WARN << ": received an unsupported BINARY type message.";
    }

    const ErrorOr<Json::Value> payload = json::Parse(GetPayload(message));
    if (payload.is_error()) {
      OSP_LOG_ERROR << "Failed to parse message: " << payload.error();
    }

    if (HasType(payload.value(), CastMessageType::kReceiverStatus)) {
      HandleReceiverStatus(payload.value());
    } else if (HasType(payload.value(), CastMessageType::kLaunchError)) {
      std::string reason;
      if (!json::TryParseString(payload.value()[kMessageKeyReason], &reason)) {
        reason = "UNKNOWN";
      }
      OSP_LOG_ERROR
          << "Failed to launch the Cast Mirroring App on the Receiver! Reason: "
          << reason;
      Shutdown();
    } else if (HasType(payload.value(), CastMessageType::kInvalidRequest)) {
      std::string reason;
      if (!json::TryParseString(payload.value()[kMessageKeyReason], &reason)) {
        reason = "UNKNOWN";
      }
      OSP_LOG_ERROR << "Cast Receiver thinks our request is invalid: "
                    << reason;
    }
  }
}

const char* LoopingFileCastAgent::GetStreamingAppId() const {
  return connection_settings_ && !connection_settings_->should_include_video
             ? GetCastStreamingAudioOnlyAppId()
             : GetCastStreamingAudioVideoAppId();
}

void LoopingFileCastAgent::HandleReceiverStatus(const Json::Value& status) {
  const Json::Value& details =
      (status[kMessageKeyStatus].isObject() &&
       status[kMessageKeyStatus][kMessageKeyApplications].isArray())
          ? status[kMessageKeyStatus][kMessageKeyApplications][0]
          : Json::Value();

  std::string running_app_id;
  if (!json::TryParseString(details[kMessageKeyAppId], &running_app_id) ||
      running_app_id != GetStreamingAppId()) {
    if (has_launched_) {
      // The mirroring app is not running and should have already been launched.
      // If it was just stopped, Shutdown() will tear everything down. If it has
      // been stopped already, Shutdown() is a no-op.
      Shutdown();
    }
    return;
  }

  // If the mirroring app is the current streaming application, we can now
  // safely say we have been launched.
  has_launched_ = true;

  std::string session_id;
  if (!json::TryParseString(details[kMessageKeySessionId], &session_id) ||
      session_id.empty()) {
    OSP_LOG_ERROR
        << "Cannot continue: Cast Receiver did not provide a session ID for "
           "the Mirroring App running on it.";
    Shutdown();
    return;
  }
  if (app_session_id_ != session_id) {
    if (app_session_id_.empty()) {
      app_session_id_ = session_id;
    } else {
      OSP_LOG_ERROR << "Cannot continue: Different Mirroring App session is "
                       "now running on the Cast Receiver.";
      Shutdown();
      return;
    }
  }

  if (remote_connection_) {
    // The mirroring app is running and this LoopingFileCastAgent is already
    // streaming to it (or is awaiting message routing to be established). There
    // are no additional actions to be taken in response to this extra
    // RECEIVER_STATUS message.
    return;
  }

  std::string message_destination_id;
  if (!json::TryParseString(details[kMessageKeyTransportId],
                            &message_destination_id) ||
      message_destination_id.empty()) {
    OSP_LOG_ERROR
        << "Cannot continue: Cast Receiver did not provide a transport ID for "
           "routing messages to the Mirroring App running on it.";
    Shutdown();
    return;
  }

  remote_connection_.emplace(
      VirtualConnection{MakeUniqueSessionId("streaming_sender"),
                        message_destination_id, message_port_.GetSocketId()});
  OSP_LOG_INFO << "Starting-up message routing to the Cast Receiver's "
                  "Mirroring App (sessionId="
               << app_session_id_ << ")...";
  connection_handler_.OpenRemoteConnection(
      *remote_connection_,
      [this](bool success) { OnRemoteMessagingOpened(success); });
}

void LoopingFileCastAgent::OnRemoteMessagingOpened(bool success) {
  if (!remote_connection_) {
    return;  // Shutdown() was called in the meantime.
  }

  if (success) {
    OSP_LOG_INFO << "Starting streaming session...";
    CreateAndStartSession();
  } else {
    OSP_LOG_INFO << "Failed to establish messaging to the Cast Receiver's "
                    "Mirroring App. Perhaps another Cast Sender is using it?";
    Shutdown();
  }
}

void LoopingFileCastAgent::OnReceiverMessagingOpened(bool success) {
  // We established a platform connection and now need to launch.
  OSP_CHECK(platform_remote_connection_);
  OSP_CHECK(!remote_connection_);
  if (!success) {
    OSP_LOG_INFO << "Failed to establish messaging to the Cast Receiver.";
    Shutdown();
    return;
  }

  static constexpr char kLaunchMessageTemplate[] =
      R"({"type":"LAUNCH", "requestId":%d, "appId":"%s", "language": "en-US",
       "supportedAppTypes":["WEB"]})";
  router_.Send(*platform_remote_connection_,
               MakeSimpleUTF8Message(
                   kReceiverNamespace,
                   StringPrintf(kLaunchMessageTemplate, next_request_id_++,
                                GetStreamingAppId())));
}

void LoopingFileCastAgent::CreateAndStartSession() {
  TRACE_DEFAULT_SCOPED(TraceCategory::kStandaloneSender);

  OSP_CHECK(remote_connection_.has_value());
  environment_ =
      std::make_unique<Environment>(&Clock::now, task_runner_, IPEndpoint{});

  SenderSession::Configuration config{
      connection_settings_->receiver_endpoint.address,
      *this,
      environment_.get(),
      &message_port_,
      remote_connection_->local_id,
      remote_connection_->peer_id,
      connection_settings_->use_android_rtp_hack};
  current_session_ = std::make_unique<SenderSession>(std::move(config));
  current_session_->SetStatsClient(this);
  OSP_CHECK(!message_port_.source_id().empty());

  AudioCaptureConfig audio_config;
  // Opus does best at 192kbps, so we cap that here.
  audio_config.bit_rate = 192 * 1000;
  VideoCaptureConfig video_config = {
      .codec = connection_settings_->codec,
      // The video config is allowed to use whatever is left over after audio.
      .max_bit_rate =
          connection_settings_->max_bitrate - audio_config.bit_rate};
  // Use default display resolution of 1080P.
  video_config.resolutions.emplace_back(Resolution{1920, 1080});

  OSP_VLOG << "Starting session negotiation.";
  Error negotiation_error;
  if (connection_settings_->use_remoting) {
    cast_mode_ = CastMode::kRemoting;
    remoting_sender_ = std::make_unique<RemotingSender>(
        current_session_->rpc_messenger(), AudioCodec::kOpus,
        connection_settings_->codec, this);

    negotiation_error =
        current_session_->NegotiateRemoting(audio_config, video_config);
  } else {
    cast_mode_ = CastMode::kMirroring;
    negotiation_error =
        current_session_->Negotiate({audio_config}, {video_config});
  }
  if (!negotiation_error.ok()) {
    OSP_LOG_ERROR << "Failed to negotiate a session: " << negotiation_error;
  }
}

void LoopingFileCastAgent::OnNegotiated(
    const SenderSession* session,
    SenderSession::ConfiguredSenders senders,
    capture_recommendations::Recommendations capture_recommendations) {
  if (senders.audio_sender == nullptr || senders.video_sender == nullptr) {
    OSP_LOG_ERROR << "Missing both audio and video, so exiting...";
    return;
  }

  current_negotiation_ =
      std::make_unique<SenderSession::ConfiguredSenders>(std::move(senders));
  if (cast_mode_ == CastMode::kMirroring || is_ready_for_remoting_) {
    StartFileSender();
  }
}

void LoopingFileCastAgent::OnError(const SenderSession* session,
                                   const Error& error) {
  OSP_LOG_ERROR << "SenderSession fatal error: " << error;
  Shutdown();
}

void LoopingFileCastAgent::OnStatisticsUpdated(
    const SenderStats& updated_stats) {
  // Only log every 10 times, or roughly every 5 seconds.
  constexpr int kLoggingInterval = 10;
  if ((num_times_on_statistics_updated_called_++ % kLoggingInterval) == 0) {
    OSP_VLOG << __func__ << ": updated_stats=" << updated_stats.ToString();
  }
  last_reported_statistics_ = std::make_optional<SenderStats>(updated_stats);
}

void LoopingFileCastAgent::OnReady() {
  OSP_CHECK_EQ(cast_mode_, CastMode::kRemoting);
  is_ready_for_remoting_ = true;
  if (current_negotiation_) {
    StartFileSender();
  }
}

void LoopingFileCastAgent::OnPlaybackRateChange(double rate) {
  file_sender_->SetPlaybackRate(rate);
}

void LoopingFileCastAgent::StartFileSender() {
  OSP_CHECK(current_negotiation_);
  file_sender_ = std::make_unique<LoopingFileSender>(
      *environment_, connection_settings_.value(), current_session_.get(),
      std::move(*current_negotiation_), [this]() { shutdown_callback_(); });
  current_negotiation_.reset();
  is_ready_for_remoting_ = false;
}

void LoopingFileCastAgent::Shutdown() {
  TRACE_DEFAULT_SCOPED(TraceCategory::kStandaloneSender);

  file_sender_.reset();
  if (current_session_) {
    OSP_LOG_INFO << "Stopping mirroring session...";
    current_session_.reset();

    if (last_reported_statistics_) {
      OSP_LOG_INFO << "Last reported statistics="
                   << last_reported_statistics_->ToString();
    }
  }
  OSP_CHECK(message_port_.source_id().empty());
  environment_.reset();

  if (platform_remote_connection_) {
    const VirtualConnection connection = *platform_remote_connection_;
    // Reset `platform_remote_connection_` because ConnectionNamespaceHandler
    // may call-back into OnReceiverMessagingOpened().
    platform_remote_connection_.reset();
    connection_handler_.CloseRemoteConnection(connection);
  }

  if (remote_connection_) {
    const VirtualConnection connection = *remote_connection_;
    // Reset `remote_connection_` because ConnectionNamespaceHandler may
    // call-back into OnRemoteMessagingOpened().
    remote_connection_.reset();
    connection_handler_.CloseRemoteConnection(connection);
  }

  if (!app_session_id_.empty()) {
    OSP_LOG_INFO << "Stopping the Cast Receiver's Mirroring App...";
    static constexpr char kStopMessageTemplate[] =
        R"({"type":"STOP", "requestId":%d, "sessionId":"%s"})";
    std::string stop_json = StringPrintf(
        kStopMessageTemplate, next_request_id_++, app_session_id_.c_str());
    router_.Send(
        VirtualConnection{kPlatformSenderId, kPlatformReceiverId,
                          message_port_.GetSocketId()},
        MakeSimpleUTF8Message(kReceiverNamespace, std::move(stop_json)));
    app_session_id_.clear();
  }

  if (message_port_.GetSocketId() != ToCastSocketId(nullptr)) {
    router_.CloseSocket(message_port_.GetSocketId());
    message_port_.SetSocket({});
  }

  wake_lock_.reset();

  if (shutdown_callback_) {
    const ShutdownCallback callback = std::move(shutdown_callback_);
    callback();
  }
}

}  // namespace openscreen::cast
