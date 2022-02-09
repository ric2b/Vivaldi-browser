// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "chrome/browser/ui/views/overlay/overlay_window_views.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/mute_button.h"
#include "ui/views/controls/video_progress.h"

// The file contains the vivaldi specific code for the OverlayWindowViews class
// used for the Picture-in-Picture window.

constexpr int kVideoProgressHeight = 8;
constexpr SkColor kProgressBarForeground = gfx::kGoogleBlue300;
constexpr SkColor kProgressBarBackground =
    SkColorSetA(gfx::kGoogleBlue300, 0x4C);  // 30%
constexpr int hSeekInterval = 10;            // seconds
constexpr gfx::Size kVivaldiButtonControlSize(20, 20);
constexpr int kVideoControlsPadding = 5;

class VideoPipControllerDelegate
    : public vivaldi::VideoPIPController::Delegate {
 public:
  VideoPipControllerDelegate(vivaldi::VideoProgress* progress_view,
                             vivaldi::MuteButton* mute_button)
      : vivaldi::VideoPIPController::Delegate(),
        progress_view_(progress_view),
        mute_button_(mute_button) {}
  ~VideoPipControllerDelegate() override {}

  void UpdateProgress(
      const media_session::MediaPosition& media_position) override {
    DCHECK(progress_view_);
    if (progress_view_) {
      progress_view_->UpdateProgress(media_position);
    }
  }
  void AudioMutingStateChanged(bool muted) override {
    DCHECK(mute_button_);
    if (mute_button_) {
      mute_button_->ChangeMode(muted ? vivaldi::MuteButton::Mode::kMute
                                     : vivaldi::MuteButton::Mode::kAudible,
                               false);
    }
  }

 private:
  // ownership tied to the OverlayWindowViews class
  vivaldi::VideoProgress* progress_view_ = nullptr;
  vivaldi::MuteButton* mute_button_ = nullptr;
};

void OverlayWindowViews::HandleVivaldiMuteButton() {
  content::WebContents* contents = controller_->GetWebContents();

  DCHECK_EQ(contents->IsAudioMuted(),
            mute_button_->muted_mode() == vivaldi::MuteButton::Mode::kMute);

  if (contents->IsAudioMuted()) {
    contents->SetAudioMuted(false);
    mute_button_->ChangeMode(vivaldi::MuteButton::Mode::kAudible, false);
  } else {
    contents->SetAudioMuted(true);
    mute_button_->ChangeMode(vivaldi::MuteButton::Mode::kMute, false);
  }
}

void OverlayWindowViews::CreateVivaldiVideoControls() {
  auto mute_button = std::make_unique<vivaldi::MuteButton>(base::BindRepeating(
      [](OverlayWindowViews* overlay) { overlay->HandleVivaldiMuteButton(); },
      base::Unretained(this)));

  mute_button->SetPaintToLayer(ui::LAYER_TEXTURED);
  mute_button->layer()->SetFillsBoundsOpaquely(false);
  mute_button->layer()->SetName("MuteControlsView");
  mute_button_ = GetContentsView()->AddChildView(std::move(mute_button));

  content::WebContents* contents = controller_->GetWebContents();

  mute_button_->ChangeMode(contents->IsAudioMuted()
                               ? vivaldi::MuteButton::Mode::kMute
                               : vivaldi::MuteButton::Mode::kAudible,
                           true);

  auto progress_view = std::make_unique<vivaldi::VideoProgress>();
  progress_view->SetForegroundColor(kProgressBarForeground);
  progress_view->SetBackgroundColor(kProgressBarBackground);
  progress_view->SetPaintToLayer(ui::LAYER_TEXTURED);
  progress_view->layer()->SetFillsBoundsOpaquely(false);
  progress_view->layer()->SetName("VideoProgressControlsView");
  progress_view_ = GetContentsView()->AddChildView(std::move(progress_view));
}

void OverlayWindowViews::UpdateVivaldiControlsVisibility(bool is_visible) {
  if (progress_view_) {
    progress_view_->ToggleVisibility(is_visible);
  }
  if (mute_button_) {
    mute_button_->SetVisible(is_visible);
  }
}

void OverlayWindowViews::UpdateVivaldiControlsBounds(int primary_control_y,
                                                     int margin) {
  int window_width = GetBounds().size().width();
  int offset_left = kVivaldiButtonControlSize.width();

  mute_button_->SetSize(kVivaldiButtonControlSize);
  mute_button_->SetPosition(
      gfx::Point(margin, primary_control_y - (kVideoProgressHeight * 2)));

  progress_view_->SetSize(gfx::Size(
      window_width - (margin * 2) - offset_left - kVideoControlsPadding,
      kVideoProgressHeight));
  progress_view_->SetPosition(
      gfx::Point(margin + offset_left + kVideoControlsPadding,
                 primary_control_y - kVideoProgressHeight));
}

void OverlayWindowViews::CreateVivaldiVideoObserver() {
  video_pip_delegate_ = std::make_unique<VideoPipControllerDelegate>(
      progress_view_, mute_button_);
  video_pip_controller_ = std::make_unique<vivaldi::VideoPIPController>(
      video_pip_delegate_.get(), controller_->GetWebContents());
  // base::Unretained() here because both progress_view_ and
  // video_pip_controller_ lifetimes are tied to |this|.
  progress_view_->set_callback(
      base::BindRepeating(&vivaldi::VideoPIPController::SeekTo,
                          base::Unretained(video_pip_controller_.get())));
}

void OverlayWindowViews::HandleVivaldiKeyboardEvents(ui::KeyEvent* event) {
  int seek_seconds = 0;
  if (event->type() == ui::ET_KEY_PRESSED) {
    if (event->key_code() == ui::VKEY_RIGHT) {
      seek_seconds += hSeekInterval;
      event->SetHandled();
    } else if (event->key_code() == ui::VKEY_LEFT) {
      seek_seconds -= hSeekInterval;
      event->SetHandled();
    }
  }
  // NOTE(pettern@vivaldi.com): Seek interval seems to be restricted depending
  // on the site so we'll just provide a reasonable default here.
  if (seek_seconds != 0) {
    video_pip_controller_->Seek(seek_seconds);
  }
}

void OverlayWindowViews::HandleVivaldiGestureEvent(ui::GestureEvent* event) {
  bool handled = false;
  if (progress_view_) {
    handled = progress_view_->HandleGestureEvent(event);
  }
  if (!handled) {
    HandleVivaldiMuteButton();
  }
}

bool OverlayWindowViews::IsPointInVivaldiControl(const gfx::Point& point) {
  if ((progress_view_ && progress_view_->GetMirroredBounds().Contains(point)) ||
      (mute_button_ && mute_button_->GetMirroredBounds().Contains(point))) {
    return true;
  }
  return false;
}
