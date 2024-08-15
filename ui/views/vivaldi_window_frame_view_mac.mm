// Copyright 2018 Vivaldi Technologies. All rights reserved.

#include "ui/views/vivaldi_window_frame_view.h"

#import <Cocoa/Cocoa.h>

#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/hit_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/native_frame_view.h"
#include "ui/views/window/non_client_view.h"
#import "ui/gfx/mac/coordinate_conversion.h"

#include "ui/vivaldi_browser_window.h"

namespace {

// Provides metrics consistent with a native frame on Mac. The actual frame is
// drawn by NSWindow.
class VivaldiWindowFrameViewMac : public views::NativeFrameView {
 public:
  VivaldiWindowFrameViewMac(VivaldiBrowserWindow* window);
  ~VivaldiWindowFrameViewMac() override;
  VivaldiWindowFrameViewMac(
      const VivaldiWindowFrameViewMac&) = delete;
  VivaldiWindowFrameViewMac& operator=(
      const VivaldiWindowFrameViewMac&) = delete;

 private:
  // views::NonClientFrameView overrides
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;

  // views::View overrides
  gfx::Size GetMinimumSize() const override;
 private:
  // Indirect owner.
  raw_ptr<VivaldiBrowserWindow> const window_;
};


VivaldiWindowFrameViewMac::VivaldiWindowFrameViewMac(
    VivaldiBrowserWindow* window)
    : views::NativeFrameView(window->GetWidget()),
      window_(window) {}

VivaldiWindowFrameViewMac::~VivaldiWindowFrameViewMac() =
    default;

gfx::Rect VivaldiWindowFrameViewMac::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  NSWindow* ns_window = window_->GetNativeWindow().GetNativeNSWindow();
  gfx::Rect window_bounds = gfx::ScreenRectFromNSRect([ns_window
      frameRectForContentRect:gfx::ScreenRectToNSRect(client_bounds)]);
  // Enforce minimum size (1, 1) in case that |client_bounds| is passed with
  // empty size.
  if (window_bounds.IsEmpty())
    window_bounds.set_size(gfx::Size(1, 1));
  return window_bounds;
}

int VivaldiWindowFrameViewMac::NonClientHitTest(
    const gfx::Point& point) {
  if (!bounds().Contains(point))
    return HTNOWHERE;

  if (window_->IsFullscreen())
    return HTCLIENT;

  // Check for possible draggable region in the client area for the frameless
  // window.
  const SkRegion* draggable_region = window_->draggable_region();
  if (draggable_region && draggable_region->contains(point.x(), point.y()))
    return HTCAPTION;

  return HTCLIENT;
}

gfx::Size VivaldiWindowFrameViewMac::GetMinimumSize() const {
  views::Widget* widget = window_->GetWidget();
  if (!widget) {
    LOG(ERROR) << "GetMinimumSize called with no widget";
    return gfx::Size(1,1);
  }
  gfx::Size min_size = widget->client_view()->GetMinimumSize();
  min_size.SetToMax(gfx::Size(1, 1));
  return min_size;
}

}  // namespace

std::unique_ptr<views::NonClientFrameView> CreateVivaldiWindowFrameView(
    VivaldiBrowserWindow* window) {
  return std::make_unique<VivaldiWindowFrameViewMac>(window);
}
