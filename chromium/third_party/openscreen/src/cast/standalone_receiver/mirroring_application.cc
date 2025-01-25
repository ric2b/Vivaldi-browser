// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_receiver/mirroring_application.h"

#include <utility>

#include "build/build_config.h"
#include "cast/common/public/cast_streaming_app_ids.h"
#include "cast/common/public/message_port.h"
#include "cast/streaming/message_fields.h"
#include "cast/streaming/public/constants.h"
#include "cast/streaming/public/environment.h"
#include "cast/streaming/public/receiver_constraints.h"
#include "cast/streaming/public/receiver_session.h"
#include "platform/api/task_runner.h"
#include "util/osp_logging.h"

namespace openscreen::cast {

const char kMirroringDisplayName[] = "Chrome Mirroring";
const char kRemotingRpcNamespace[] = "urn:x-cast:com.google.cast.remoting";

MirroringApplication::MirroringApplication(TaskRunner& task_runner,
                                           const IPAddress& interface_address,
                                           ApplicationAgent& agent)
    : task_runner_(task_runner),
      interface_address_(interface_address),
      app_ids_(GetCastStreamingAppIds()),
      agent_(agent) {
  agent_.RegisterApplication(this);
}

MirroringApplication::~MirroringApplication() {
  agent_.UnregisterApplication(this);  // ApplicationAgent may call Stop().
  OSP_CHECK(!current_session_);
}

const std::vector<std::string>& MirroringApplication::GetAppIds() const {
  return app_ids_;
}

bool MirroringApplication::Launch(const std::string& app_id,
                                  const Json::Value& app_params,
                                  MessagePort* message_port) {
  if (!IsCastStreamingAppId(app_id) || !message_port || current_session_) {
    return false;
  }

#if BUILDFLAG(IS_APPLE)
  wake_lock_ = ScopedWakeLock::Create(task_runner_);
#endif  // BUILDFLAG(IS_APPLE)
  environment_ = std::make_unique<Environment>(
      &Clock::now, task_runner_,
      IPEndpoint{interface_address_, kDefaultCastStreamingPort});
#if defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)
  controller_ =
      std::make_unique<StreamingPlaybackController>(task_runner_, this);
#else
  controller_ = std::make_unique<StreamingPlaybackController>(this);
#endif  // defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)
  ReceiverConstraints constraints;
  constraints.video_codecs.insert(constraints.video_codecs.begin(),
                                  {VideoCodec::kAv1, VideoCodec::kVp9});
  constraints.remoting = std::make_unique<RemotingConstraints>();
  current_session_ = std::make_unique<ReceiverSession>(
      *controller_, *environment_, *message_port, std::move(constraints));
  return true;
}

std::string MirroringApplication::GetSessionId() {
  return current_session_ ? current_session_->session_id() : std::string();
}

std::string MirroringApplication::GetDisplayName() {
  return current_session_ ? kMirroringDisplayName : std::string();
}

std::vector<std::string> MirroringApplication::GetSupportedNamespaces() {
  return {kCastWebrtcNamespace, kRemotingRpcNamespace};
}

void MirroringApplication::Stop() {
  current_session_.reset();
  controller_.reset();
  environment_.reset();
  wake_lock_.reset();
}

void MirroringApplication::OnPlaybackError(StreamingPlaybackController*,
                                           const Error& error) {
  OSP_LOG_ERROR << "[MirroringApplication] " << error;
  agent_.StopApplicationIfRunning(this);  // ApplicationAgent calls Stop().
}

}  // namespace openscreen::cast
