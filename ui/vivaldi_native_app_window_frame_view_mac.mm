// Copyright 2018 Vivaldi Technologies. All rights reserved.

#include "ui/vivaldi_native_app_window_frame_view_mac.h"

#import <Cocoa/Cocoa.h>

#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/hit_test.h"
#include "ui/views/widget/widget.h"
#include "ui/vivaldi_native_app_window_views.h"
#include "ui/vivaldi_browser_window.h"

#import "ui/gfx/mac/coordinate_conversion.h"

VivaldiNativeAppWindowFrameViewMac::VivaldiNativeAppWindowFrameViewMac(
    VivaldiNativeAppWindowViews* views)
    : views::NativeFrameView(views->widget()),
      views_(views) {}

VivaldiNativeAppWindowFrameViewMac::~VivaldiNativeAppWindowFrameViewMac() =
    default;

void VivaldiNativeAppWindowFrameViewMac::Layout() {
  NonClientFrameView::Layout();
}

gfx::Rect VivaldiNativeAppWindowFrameViewMac::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  NSWindow* ns_window = GetWidget()->GetNativeWindow().GetNativeNSWindow();
  gfx::Rect window_bounds = gfx::ScreenRectFromNSRect([ns_window
      frameRectForContentRect:gfx::ScreenRectToNSRect(client_bounds)]);
  // Enforce minimum size (1, 1) in case that |client_bounds| is passed with
  // empty size.
  if (window_bounds.IsEmpty())
    window_bounds.set_size(gfx::Size(1, 1));
  return window_bounds;
}

int VivaldiNativeAppWindowFrameViewMac::NonClientHitTest(
    const gfx::Point& point) {
  if (!bounds().Contains(point))
    return HTNOWHERE;

  if (GetWidget()->IsFullscreen())
    return HTCLIENT;

  // Check for possible draggable region in the client area for the frameless
  // window.
  SkRegion* draggable_region = views_->draggable_region();
  if (draggable_region && draggable_region->contains(point.x(), point.y()))
    return HTCAPTION;

  return HTCLIENT;
}
