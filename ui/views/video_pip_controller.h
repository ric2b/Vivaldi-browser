// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "base/containers/flat_set.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/views/controls/slider.h"
#include "content/browser/picture_in_picture/picture_in_picture_session.h"

namespace vivaldi {
class VideoProgress;
class MuteButton;

class VideoPIPController
    : public content::WebContentsObserver,
      public media_session::mojom::MediaControllerObserver,
      public views::SliderListener {
 public:
  class Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}

    // Update the progress.
    virtual void UpdateProgress(
        const media_session::MediaPosition& media_position) = 0;

    // Mute state for the whole WebContents.
    virtual void AudioMutingStateChanged(bool muted) = 0;
  };

  ~VideoPIPController() override;
  VideoPIPController(vivaldi::VideoPIPController::Delegate* delegate,
                     content::WebContents* web_contents);
  VideoPIPController(const VideoPIPController&) = delete;
  VideoPIPController& operator=(const VideoPIPController&) = delete;

  void SeekTo(double current_position, double seek_progress);

  // Seek forward or backwards by the given seconds
  void Seek(int seconds);
  std::optional<media_session::MediaPosition> GetPosition() {
    return position_;
  }

  void SetVolume(float volume_multiplier);

  bool SupportsAction(media_session::mojom::MediaSessionAction action) const;

  // media_session::mojom::MediaControllerObserver:
  void MediaSessionPositionChanged(
      const std::optional<media_session::MediaPosition>& position) override;
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& action)
      override;
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr session_info) override {}
  void MediaSessionMetadataChanged(
      const std::optional<media_session::MediaMetadata>&) override {}
  void MediaSessionChanged(
      const std::optional<base::UnguessableToken>& request_id) override {}

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;
  void DidUpdateAudioMutingState(bool muted) override;

  void SliderValueChanged(views::Slider* sender,
                                  float value,
                                  float old_value,
                                  views::SliderChangeReason reason) override;
  // Invoked when a drag starts or ends (more specifically, when the mouse
  // button is pressed or released).
  void SliderDragStarted(views::Slider* sender) override;
  void SliderDragEnded(views::Slider* sender) override;

 private:
  // Used to control the active session.
  mojo::Remote<media_session::mojom::MediaController> media_controller_remote_;
  std::optional<media_session::MediaPosition> position_;
  mojo::Receiver<media_session::mojom::MediaControllerObserver>
      media_controller_observer_receiver_{this};
  raw_ptr<Delegate> delegate_ = nullptr;

  // Used to check which actions are currently supported.
  base::flat_set<media_session::mojom::MediaSessionAction> actions_;
};

}  // namespace vivaldi
