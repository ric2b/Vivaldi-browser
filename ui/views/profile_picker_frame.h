// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved
//
#ifndef UI_VIEW_PROFILE_PICKER_FRAME_H_
#define UI_VIEW_PROFILE_PICKER_FRAME_H_

#include "ui/views/window/non_client_view.h"

class ProfilePickerView;

namespace vivaldi {

class PickerFrame : public views::NonClientFrameView {
 public:
  PickerFrame(ProfilePickerView* profile_picker);

 protected:
  // NonClientFrameView implementation
  gfx::Rect GetBoundsForClientView() const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void OnPaint(gfx::Canvas* canvas) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  gfx::Size GetMinimumSize() const override;

 private:
  raw_ptr<ProfilePickerView> profile_picker_;
};

}
#endif
