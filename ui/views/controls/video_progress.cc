// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved
//
// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/video_progress.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"

namespace vivaldi {

namespace {
constexpr SkColor kTimeColor = gfx::kGoogleGrey200;
constexpr int kProgressTimeFontSize = 11;
constexpr int kProgressBarHeight = 7;
}  // namespace

VideoProgress::VideoProgress() {
  views::FlexLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout_manager->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCollapseMargins(true);

  gfx::Font default_font;
  int font_size_delta = kProgressTimeFontSize - default_font.GetFontSize();
  gfx::Font font = default_font.Derive(font_size_delta, gfx::Font::NORMAL,
                                       gfx::Font::Weight::NORMAL);
  gfx::FontList font_list(font);

  auto progress_time = std::make_unique<views::Label>(u"0:00:00");
  progress_time->SetFontList(font_list);
  progress_time->SetEnabledColor(kTimeColor);
  progress_time->SetAutoColorReadabilityEnabled(false);
  progress_time->SetPaintToLayer(ui::LAYER_TEXTURED);
  progress_time->SetBackgroundColor(
      SkColorSetA(gfx::kGoogleBlue300, 0x00));  // fully transparent
  progress_time->layer()->SetName("VideoProgressTimeView");
  progress_time->layer()->SetFillsBoundsOpaquely(false);
  progress_time->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred));
  progress_time_ = AddChildView(std::move(progress_time));

  auto progress_bar = std::make_unique<views::ProgressBar>();
  progress_bar->SetPreferredHeight(kProgressBarHeight);
  progress_bar->SetPreferredCornerRadii(
      gfx::RoundedCornersF(kProgressBarHeight / 2));
  progress_bar->SetPaintToLayer(ui::LAYER_TEXTURED);
  progress_bar->layer()->SetName("VideoProgressControlsView");
  progress_bar->layer()->SetFillsBoundsOpaquely(false);
  progress_bar->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded));
  progress_bar->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 4, 0, 4));
  progress_bar_ = AddChildView(std::move(progress_bar));

  auto duration = std::make_unique<views::Label>(u"0:00:00");
  duration->SetFontList(font_list);
  duration->SetEnabledColor(SK_ColorWHITE);
  duration->SetAutoColorReadabilityEnabled(false);
  duration->SetPaintToLayer(ui::LAYER_TEXTURED);
  duration->SetBackgroundColor(
      SkColorSetA(gfx::kGoogleBlue300, 0x00));  // fully transparent
  duration->layer()->SetName("VideoProgressTimeDurationView");
  duration->layer()->SetFillsBoundsOpaquely(false);
  duration->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred));
  duration_ = AddChildView(std::move(duration));
}

VideoProgress::~VideoProgress() = default;

void VideoProgress::ToggleVisibility(bool is_visible) {
  layer()->SetVisible(is_visible);
  SetEnabled(is_visible);
  SetVisible(is_visible);
}

bool VideoProgress::GetStringFromPosition(base::DurationFormatWidth time_format,
                                          base::TimeDelta position,
                                          std::u16string& time) const {
  bool time_received =
      base::TimeDurationFormatWithSeconds(position, time_format, &time);

  return time_received;
}

std::u16string VideoProgress::StripHour(std::u16string& time) const {
  base::ReplaceFirstSubstringAfterOffset(&time, 0, u"0:", u"");

  return time;
}

void VideoProgress::UpdateProgress(
    const media_session::MediaPosition& media_position) {
  // If the media is paused and |update_progress_timer_| is still running, stop
  // the timer.
  if (media_position.playback_rate() == 0 && update_progress_timer_.IsRunning())
    update_progress_timer_.Stop();

  base::TimeDelta current_position = media_position.GetPosition();

  duration_delta_ = media_position.duration();

  double progress =
      current_position.InSecondsF() / duration_delta_.InSecondsF();
  SetBarProgress(progress);

  // Time formatting can't yet represent durations greater than 24 hours in
  // base::DURATION_WIDTH_NUMERIC format.
  base::DurationFormatWidth time_format = duration_delta_ >= base::Days(1)
          ? base::DURATION_WIDTH_NARROW
          : base::DURATION_WIDTH_NUMERIC;

  std::u16string elapsed_time;
  bool elapsed_time_received =
      GetStringFromPosition(time_format, current_position, elapsed_time);

  std::u16string total_time;
  bool total_time_received =
      GetStringFromPosition(time_format, duration_delta_, total_time);

  if (elapsed_time_received && total_time_received) {
    // If |duration| is less than an hour, we don't want to show  "0:" hours on
    // the progress times.
    if (duration_delta_ < base::Hours(1)) {
      elapsed_time = StripHour(elapsed_time);
      total_time = StripHour(total_time);
    }
    allows_click_ = true;

    // A duration higher than a day is likely a fake number given to
    // undetermined durations on eg. twitch.tv, so ignore it.
    if (duration_delta_ < base::Days(1)) {
      SetProgressTime(elapsed_time);
      SetDuration(total_time);
    } else {
      allows_click_ = false;
    }
  }

  if (media_position.playback_rate() != 0) {
    base::TimeDelta update_frequency =
        base::Seconds(
        std::abs(1 / media_position.playback_rate()));
    update_progress_timer_.Start(
        FROM_HERE, update_frequency,
        base::BindRepeating(&VideoProgress::UpdateProgress,
                            base::Unretained(this), media_position));
  }
}

void VideoProgress::SetForegroundColor(SkColor color) {
  progress_bar_->SetForegroundColor(color);
}

void VideoProgress::SetBackgroundColor(SkColor color) {
  progress_bar_->SetBackgroundColor(color);
}

bool VideoProgress::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton() &&
      progress_bar_->GetMirroredBounds().Contains(event.location())) {
    HandleSeeking(event.location());
    return true;
  }
  return false;
}

bool VideoProgress::HandleGestureEvent(ui::GestureEvent* event) {
  if (progress_bar_->GetMirroredBounds().Contains(event->location())) {
    HandleSeeking(event->location());
    event->SetHandled();
    return true;
  }
  return false;
}

views::View* VideoProgress::GetTooltipHandlerForPoint(const gfx::Point& point) {
  if (progress_bar_->bounds().Contains(point)) {
    return this;
  }
  return nullptr;
}

std::u16string VideoProgress::GetTooltipText(const gfx::Point& p) const {
  std::u16string time;
  if (allows_click_ && progress_bar_->bounds().Contains(p)) {
    base::DurationFormatWidth time_format = duration_delta_ >= base::Days(1)
            ? base::DURATION_WIDTH_NARROW
            : base::DURATION_WIDTH_NUMERIC;

    gfx::Point location_in_bar(p);

    ConvertPointToTarget(this, progress_bar_, &location_in_bar);

    double progress =
        static_cast<double>(location_in_bar.x()) / progress_bar_->width();

    base::TimeDelta delta =
        base::Seconds(duration_delta_.InSecondsF() * progress);
    bool has_time = GetStringFromPosition(time_format, delta, time);
    if (has_time && duration_delta_ < base::Hours(1)) {
      time = StripHour(time);
    }
  }
  return time;
}

void VideoProgress::SetBarProgress(double progress) {
  progress_bar_->SetValue(progress);
}

void VideoProgress::SetProgressTime(const std::u16string& time) {
  progress_time_->SetText(time);
}

void VideoProgress::SetDuration(const std::u16string& duration) {
  duration_->SetText(duration);
}

void VideoProgress::HandleSeeking(const gfx::Point& location) {
  DCHECK(!seek_callback_.is_null());

  if (!allows_click_) {
    return;
  }
  gfx::Point location_in_bar(location);
  ConvertPointToTarget(this, progress_bar_, &location_in_bar);

  double current_position = progress_bar_->GetValue();
  double seek_to_progress =
      static_cast<double>(location_in_bar.x()) / progress_bar_->width();
  seek_callback_.Run(current_position, seek_to_progress);
}

ui::Cursor VideoProgress::GetCursor(const ui::MouseEvent& event) {
  return ui::mojom::CursorType::kProgress;
}

BEGIN_METADATA(VideoProgress)
END_METADATA

}  // namespace vivaldi
