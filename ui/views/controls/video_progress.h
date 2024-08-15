// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef UI_VIEWS_CONTROLS_VIDEO_PROGRESS_H_
#define UI_VIEWS_CONTROLS_VIDEO_PROGRESS_H_

#include "base/compiler_specific.h"
#include "base/timer/timer.h"
#include "base/i18n/time_formatting.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class ProgressBar;
class Label;
}  // namespace views

namespace vivaldi {

// Progress bar is a control that indicates progress visually.
class VIEWS_EXPORT VideoProgress : public views::View {
  METADATA_HEADER(VideoProgress, views::View)
 public:

  VideoProgress();
  ~VideoProgress() override;
  VideoProgress(const VideoProgress&) = delete;
  VideoProgress& operator=(const VideoProgress&) = delete;

  void ToggleVisibility(bool is_visible);
  void SetForegroundColor(SkColor color);
  void SetBackgroundColor(SkColor color);
  void UpdateProgress(const media_session::MediaPosition& media_position);

  // View overrides
  bool OnMousePressed(const ui::MouseEvent& event) override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  std::u16string GetTooltipText(const gfx::Point& p) const override;
  ui::Cursor GetCursor(const ui::MouseEvent& event) override;


  void set_callback(
      base::RepeatingCallback<void(double, double)> seek_callback) {
    seek_callback_ = std::move(seek_callback);
  }

  bool HandleGestureEvent(ui::GestureEvent* event);

 private:
  void SetBarProgress(double progress);
  void SetProgressTime(const std::u16string& time);
  void SetDuration(const std::u16string& duration);
  bool GetStringFromPosition(base::DurationFormatWidth time_format,
                             base::TimeDelta position,
                             std::u16string& time) const;
  std::u16string StripHour(std::u16string& time) const;

  void HandleSeeking(const gfx::Point& location);

  raw_ptr<views::ProgressBar> progress_bar_;
  raw_ptr<views::Label> progress_time_;
  raw_ptr<views::Label> duration_;
  base::TimeDelta duration_delta_;
  // Do we allow clicking to seek. Might be false for indetermined duration
  // videos such as streams.
  bool allows_click_ = true;

  base::RepeatingCallback<void(double, double)> seek_callback_;

  // Timer to continually update the progress.
  base::RepeatingTimer update_progress_timer_;
};

}  // namespace vivaldi

#endif  // UI_VIEWS_CONTROLS_VIDEO_PROGRESS_H_
