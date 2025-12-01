// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "ui/views/profile_picker_frame.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/hit_test.h"
#include "app/vivaldi_apptools.h"

namespace vivaldi {

namespace {
static constexpr int kCornerSize = 16;
static constexpr int kBorderSize = 5;
} // namespace

PickerFrame::PickerFrame(ProfilePickerView* profile_picker)
  : profile_picker_(profile_picker) {
    CHECK(vivaldi::IsVivaldiRunning());
}

gfx::Rect PickerFrame::GetBoundsForClientView() const {
  return GetLocalBounds();
}

int PickerFrame::NonClientHitTest(const gfx::Point& point) {
  auto* widget = profile_picker_->GetWidget();

  const SkRegion* draggable_region = profile_picker_->GetDraggableRegion();
  if (draggable_region && draggable_region->contains(point.x(), point.y()))
    return HTCAPTION;

  if (widget) {
    bool can_ever_resize = widget->widget_delegate()
                               ? widget->widget_delegate()->CanResize()
                               : false;

    int resize_border =
        (widget->IsMaximized() || widget->IsFullscreen()) ? 0 : kBorderSize;

    int resize_corner_size = kCornerSize;

    int frame_component = GetHTComponentForFrame(
        point, gfx::Insets(resize_border), resize_corner_size,
        resize_corner_size, can_ever_resize);
    if (frame_component != HTNOWHERE) {
      return frame_component;
    }

    int client_component = widget->client_view()->NonClientHitTest(point);
    if (client_component != HTNOWHERE)
      return client_component;
  }

  return HTNOWHERE;
}

void PickerFrame::OnPaint(gfx::Canvas* canvas) {}

gfx::Size PickerFrame::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  views::Widget* widget = profile_picker_->GetWidget();
  if (!widget)
    return gfx::Size();
  gfx::Size pref = widget->client_view()->GetPreferredSize();
  gfx::Rect bounds(0, 0, pref.width(), pref.height());
  return widget->non_client_view()
      ->GetWindowBoundsForClientBounds(bounds)
      .size();
}

gfx::Rect PickerFrame::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  return client_bounds;
}

gfx::Size PickerFrame::GetMinimumSize() const {
  return gfx::Size(400, 300);
}

} // namespace vivaldi
