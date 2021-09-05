// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef UI_VIEWS_CONTROLS_VIDEO_PROGRESS_H_
#define UI_VIEWS_CONTROLS_VIDEO_PROGRESS_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/i18n/time_formatting.h"
#include "base/optional.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/views/view.h"

namespace views {
class ProgressBar;
class Label;
}  // namespace views

namespace vivaldi {

// Progress bar is a control that indicates progress visually.
class VIEWS_EXPORT VideoProgress: public views::View {
 public:
  METADATA_HEADER(VideoProgress);

  VideoProgress();
  ~VideoProgress() override;

  void ToggleVisibility(bool is_visible);
  void SetForegroundColor(SkColor color);
  void SetBackgroundColor(SkColor color);
  void UpdateProgress(const media_session::MediaPosition& media_position);

  // View overrides
  bool OnMousePressed(const ui::MouseEvent& event) override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  base::string16 GetTooltipText(const gfx::Point& p) const override;

  void set_callback(
      base::RepeatingCallback<void(double, double)> seek_callback) {
    seek_callback_ = std::move(seek_callback);
  }

  bool HandleGestureEvent(ui::GestureEvent* event);

private:
  void SetBarProgress(double progress);
  void SetProgressTime(const base::string16& time);
  void SetDuration(const base::string16& duration);
  bool GetStringFromPosition(base::DurationFormatWidth time_format,
                             base::TimeDelta position,
                             base::string16& time) const;
  base::string16 StripHour(base::string16& time) const;

  void HandleSeeking(const gfx::Point& location);

  views::ProgressBar* progress_bar_;
  views::Label* progress_time_;
  views::Label* duration_;
  base::TimeDelta duration_delta_;
  // Do we allow clicking to seek. Might be false for indetermined duration
  // videos such as streams.
  bool allows_click_ = true;

  base::RepeatingCallback<void(double, double)> seek_callback_;

  // Timer to continually update the progress.
  base::RepeatingTimer update_progress_timer_;

  DISALLOW_COPY_AND_ASSIGN(VideoProgress);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_VIDEO_PROGRESS_H_
