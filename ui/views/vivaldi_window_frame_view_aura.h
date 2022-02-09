// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIVALDI_WINDOW_FRAME_VIEW_AURA_H_
#define UI_VIEWS_VIVALDI_WINDOW_FRAME_VIEW_AURA_H_

#include <string>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/non_client_view.h"

class VivaldiNativeAppWindowViews;

namespace gfx {
class Canvas;
class Point;
}  // namespace gfx

namespace ui {
class Event;
}

namespace views {
class ImageButton;
class Widget;
}  // namespace views

// VivaldiWindowFrameViewAura is used to draw frames for app windows when a
// non standard frame is needed. This occurs if there is no frame needed, or
// if there is a frame color.
//
// The implementation was copied from
// chromium/apps/ui/views/app_window_frame_view.*
class VivaldiWindowFrameViewAura : public views::NonClientFrameView {
 public:
  static const char kViewClassName[];

  // VivaldiWindowFrameViewAura is used to draw frames for app windows when a
  // non standard frame is needed. This occurs if there is no frame needed, or
  // if there is a frame color.
  VivaldiWindowFrameViewAura(VivaldiNativeAppWindowViews* views);
  ~VivaldiWindowFrameViewAura() override;
  VivaldiWindowFrameViewAura(const VivaldiWindowFrameViewAura&) = delete;
  VivaldiWindowFrameViewAura& operator=(const VivaldiWindowFrameViewAura&) =
      delete;

  int resize_inside_bounds_size() const { return resize_inside_bounds_size_; }

 private:
  views::Widget* ViewsWidget() const;

  // views::NonClientFrameView implementation.
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override;
  void ResetWindowControls() override {}
  void UpdateWindowIcon() override {}
  void UpdateWindowTitle() override {}
  void SizeConstraintsChanged() override;

  // views::View implementation.
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  const char* GetClassName() const override;
  void OnPaint(gfx::Canvas* canvas) override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;

  VivaldiNativeAppWindowViews* const views_;

  // Allow resize for clicks this many pixels inside the bounds.
  int resize_inside_bounds_size_ = 5;

  // Size in pixels of the lower-right corner resize handle.
  int resize_area_corner_size_ = 16;
};

#endif  // UI_VIEWS_VIVALDI_WINDOW_FRAME_VIEW_AURA_H_
