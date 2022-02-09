// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "ui/views/video_pip_controller.h"

#include "content/public/browser/media_session.h"
#include "content/public/browser/media_session_service.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "ui/views/controls/video_progress.h"

// MediaWebContentsObserver
// Get position: MediaSessionController::GetPosition(int player_id)
// MediaSessionImpl::AddPlayer with MediaSessionPlayerObserver
// see D:\vivaldi_src_2\chromium\ash\login\ui\lock_screen_media_controls_view.cc
// media_session::mojom::MediaControllerObserver

using media_session::mojom::MediaSessionAction;

namespace vivaldi {

VideoPIPController::VideoPIPController(
    vivaldi::VideoPIPController::Delegate* delegate,
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents), delegate_(delegate) {
  mojo::Remote<media_session::mojom::MediaControllerManager>
      controller_manager_remote;
  content::GetMediaSessionService().BindMediaControllerManager(
      controller_manager_remote.BindNewPipeAndPassReceiver());
  controller_manager_remote->CreateActiveMediaController(
      media_controller_remote_.BindNewPipeAndPassReceiver());

  // Observe the active media controller for changes to playback state and
  // supported actions.
  media_controller_remote_->AddObserver(
      media_controller_observer_receiver_.BindNewPipeAndPassRemote());
}

VideoPIPController::~VideoPIPController() {}

void VideoPIPController::MediaSessionPositionChanged(
    const absl::optional<media_session::MediaPosition>& position) {
  // Follows the typical Chromium pattern to not accept empty positions.
  if (!position.has_value())
    return;

  position_ = position;

  if (delegate_) {
    delegate_->UpdateProgress(position_.value());
  }
}

bool VideoPIPController::SupportsAction(MediaSessionAction action) const {
  return actions_.contains(action);
}

void VideoPIPController::MediaSessionActionsChanged(
    const std::vector<MediaSessionAction>& actions) {
  // Populate |actions_| with the new MediaSessionActions and start listening
  // to necessary media keys.
  actions_ = base::flat_set<media_session::mojom::MediaSessionAction>(actions);
}

void VideoPIPController::WebContentsDestroyed() {
  delegate_ = nullptr;
}

void VideoPIPController::DidUpdateAudioMutingState(bool muted) {
  if (delegate_) {
    delegate_->AudioMutingStateChanged(muted);
  }
}

void VideoPIPController::SeekTo(double current_position, double seek_progress) {
  DCHECK(position_.has_value());

  if (SupportsAction(MediaSessionAction::kSeekTo)) {
    media_controller_remote_->SeekTo(seek_progress * position_->duration());
  } else if (current_position < seek_progress &&
             SupportsAction(MediaSessionAction::kSeekForward)) {
    base::TimeDelta delta =
        base::TimeDelta::FromSecondsD((seek_progress - current_position) *
                                      position_->duration().InSecondsF());
    media_controller_remote_->Seek(delta);
  } else if (current_position > seek_progress &&
             SupportsAction(MediaSessionAction::kSeekBackward)) {
    base::TimeDelta delta =
        base::TimeDelta::FromSecondsD(-(current_position - seek_progress) *
                                      position_->duration().InSecondsF());
    media_controller_remote_->Seek(delta);
  }
}

void VideoPIPController::Seek(int seconds) {
  if (seconds > 0 && SupportsAction(MediaSessionAction::kSeekForward)) {
    base::TimeDelta delta = base::TimeDelta::FromSeconds(seconds);
    media_controller_remote_->Seek(delta);
  } else if (SupportsAction(MediaSessionAction::kSeekBackward)) {
    base::TimeDelta delta = base::TimeDelta::FromSeconds(seconds);
    media_controller_remote_->Seek(delta);
  }
}

}  // namespace vivaldi
