// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "chrome/browser/ui/views/overlay/overlay_window_views.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/video_progress.h"

// The file contains the vivaldi specific code for the OverlayWindowViews class
// used for the Picture-in-Picture window.

constexpr int kVideoProgressHeight = 24;
constexpr SkColor kProgressBarForeground = gfx::kGoogleBlue300;
constexpr SkColor kProgressBarBackground =
    SkColorSetA(gfx::kGoogleBlue300, 0x4C);  // 30%
constexpr int hSeekInterval = 10; // seconds

void OverlayWindowViews::CreateVivaldiVideoControls() {
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
}

void OverlayWindowViews::UpdateVivaldiControlsBounds(int primary_control_y,
                                                     int margin) {
  int window_width = GetBounds().size().width();
  progress_view_->SetSize(
      gfx::Size(window_width - (margin * 2), kVideoProgressHeight));
  progress_view_->SetPosition(
      gfx::Point(margin, primary_control_y - kVideoProgressHeight));
}

void OverlayWindowViews::CreateVivaldiVideoObserver() {
  video_progress_observer_ = std::make_unique<vivaldi::VideoProgressObserver>(
      progress_view_, controller_->GetInitiatorWebContents());
  // base::Unretained() here because both progress_view_ and
  // video_progress_observer_ lifetime is tied to |this|.
  progress_view_->set_callback(
    base::BindRepeating(&vivaldi::VideoProgressObserver::SeekTo,
                 base::Unretained(video_progress_observer_.get())));
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
    video_progress_observer_->Seek(seek_seconds);
  }
}
