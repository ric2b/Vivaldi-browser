// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "base/containers/flat_set.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace vivaldi {
class VideoProgress;

class VideoProgressObserver
    : public content::WebContentsObserver,
      public media_session::mojom::MediaControllerObserver {
 public:
  ~VideoProgressObserver() override;
  VideoProgressObserver(vivaldi::VideoProgress* progress,
                        content::WebContents* web_contents);

  void SeekTo(double current_position, double seek_progress);

  // Seek forward or backwards by the given seconds
  void Seek(int seconds);
  base::Optional<media_session::MediaPosition> GetPosition() {
    return position_;
  }
  bool SupportsAction(media_session::mojom::MediaSessionAction action) const;

  // media_session::mojom::MediaControllerObserver:
  void MediaSessionPositionChanged(
      const base::Optional<media_session::MediaPosition>& position) override;
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& action)
      override;
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr session_info) override {}
  void MediaSessionMetadataChanged(
      const base::Optional<media_session::MediaMetadata>&) override {}
  void MediaSessionChanged(
      const base::Optional<base::UnguessableToken>& request_id) override {}

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

 private:
  // Used to control the active session.
  mojo::Remote<media_session::mojom::MediaController> media_controller_remote_;
  base::Optional<media_session::MediaPosition> position_;
  mojo::Receiver<media_session::mojom::MediaControllerObserver>
      media_controller_observer_receiver_{this};
  vivaldi::VideoProgress* progress_ = nullptr;

  // Used to check which actions are currently supported.
  base::flat_set<media_session::mojom::MediaSessionAction> actions_;

  DISALLOW_COPY_AND_ASSIGN(VideoProgressObserver);
};

}  // namespace vivaldi
