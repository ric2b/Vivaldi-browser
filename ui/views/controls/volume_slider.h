// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#ifndef UI_VIEWS_CONTROLS_VOLUME_SLIDER_H_
#define UI_VIEWS_CONTROLS_VOLUME_SLIDER_H_

#include "ui/views/controls/slider.h"

namespace vivaldi {

class VIEWS_EXPORT VolumeSlider : public views::Slider {
 public:
  enum class Mode { kMute = 0, kAudible };

  // The button's class name.
  static const char kViewClassName[];

  VolumeSlider(views::SliderListener* listener = nullptr);
  ~VolumeSlider() override;
  VolumeSlider(const VolumeSlider&) = delete;
  VolumeSlider& operator=(const VolumeSlider&) = delete;

  // ui::Views
  ui::Cursor GetCursor(const ui::MouseEvent& event) override;
};

}  // namespace vivaldi

#endif  // UI_VIEWS_CONTROLS_VOLUME_SLIDER_H_
