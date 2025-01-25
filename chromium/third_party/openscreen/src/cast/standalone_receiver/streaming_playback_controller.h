// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STANDALONE_RECEIVER_STREAMING_PLAYBACK_CONTROLLER_H_
#define CAST_STANDALONE_RECEIVER_STREAMING_PLAYBACK_CONTROLLER_H_

#include <memory>

#include "cast/standalone_receiver/simple_remoting_receiver.h"
#include "cast/streaming/public/receiver_session.h"
#include "platform/api/task_runner.h"

#if defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)
#include "cast/standalone_receiver/sdl_audio_player.h"  // nogncheck
#include "cast/standalone_receiver/sdl_glue.h"          // nogncheck
#include "cast/standalone_receiver/sdl_video_player.h"  // nogncheck
#else
#include "cast/standalone_receiver/dummy_player.h"  // nogncheck
#endif  // defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)

namespace openscreen::cast {

class StreamingPlaybackController final : public ReceiverSession::Client {
 public:
  class Client {
   public:
    virtual void OnPlaybackError(StreamingPlaybackController* controller,
                                 const Error& error) = 0;

   protected:
    virtual ~Client();
  };

#if defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)
  StreamingPlaybackController(TaskRunner& task_runner,
                              StreamingPlaybackController::Client* client);
#else
  explicit StreamingPlaybackController(
      StreamingPlaybackController::Client* client);
#endif  // defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)

  // ReceiverSession::Client overrides.
  void OnNegotiated(const ReceiverSession* session,
                    ReceiverSession::ConfiguredReceivers receivers) override;
  void OnRemotingNegotiated(
      const ReceiverSession* session,
      ReceiverSession::RemotingNegotiation negotiation) override;
  void OnReceiversDestroying(const ReceiverSession* session,
                             ReceiversDestroyingReason reason) override;
  void OnError(const ReceiverSession* session, const Error& error) override;

 private:
  StreamingPlaybackController::Client* client_;

  void Initialize(ReceiverSession::ConfiguredReceivers receivers);

#if defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)
  void HandleKeyboardEvent(const SDL_KeyboardEvent& event);

  TaskRunner& task_runner_;

  // NOTE: member ordering is important, since the sub systems must be
  // first-constructed, last-destroyed. Make sure any new SDL related
  // members are added below the sub systems.
  const ScopedSDLSubSystem<SDL_INIT_AUDIO> sdl_audio_sub_system_;
  const ScopedSDLSubSystem<SDL_INIT_VIDEO> sdl_video_sub_system_;
  SDLEventLoopProcessor sdl_event_loop_;

  SDLWindowUniquePtr window_;
  SDLRendererUniquePtr renderer_;
  std::unique_ptr<SDLAudioPlayer> audio_player_;
  std::unique_ptr<SDLVideoPlayer> video_player_;
  double is_playing_ = true;
#else
  std::unique_ptr<DummyPlayer> audio_player_;
  std::unique_ptr<DummyPlayer> video_player_;
#endif  // defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)

  std::unique_ptr<SimpleRemotingReceiver> remoting_receiver_;
};

}  // namespace openscreen::cast

#endif  // CAST_STANDALONE_RECEIVER_STREAMING_PLAYBACK_CONTROLLER_H_
