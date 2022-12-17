// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/vivaldi_window_frame_view_aura.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "chrome/grit/theme_resources.h"
#include "extensions/common/draggable_region.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/strings/grit/ui_strings.h"  // Accessibility names
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/vivaldi_native_app_window_views.h"

const char VivaldiWindowFrameViewAura::kViewClassName[] =
    "browser/ui/views/extensions/VivaldiWindowFrameViewAura";

VivaldiWindowFrameViewAura::VivaldiWindowFrameViewAura(
    VivaldiNativeAppWindowViews* views)
    : views_(views) {}

VivaldiWindowFrameViewAura::~VivaldiWindowFrameViewAura() = default;

views::Widget* VivaldiWindowFrameViewAura::ViewsWidget() const {
  return views_->widget();
}

// views::NonClientFrameView implementation.

gfx::Rect VivaldiWindowFrameViewAura::GetBoundsForClientView() const {
  return bounds();
}

gfx::Rect VivaldiWindowFrameViewAura::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  gfx::Rect window_bounds = client_bounds;
#if BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)
  // Get the difference between the widget's client area bounds and window
  // bounds, and grow |window_bounds| by that amount.
  gfx::Insets native_frame_insets =
      ViewsWidget()->GetClientAreaBoundsInScreen().InsetsFrom(
          ViewsWidget()->GetWindowBoundsInScreen());
  window_bounds.Inset(native_frame_insets);
#endif
  // Enforce minimum size (1, 1) in case that client_bounds is passed with
  // empty size. This could occur when the frameless window is being
  // initialized.
  if (window_bounds.IsEmpty()) {
    window_bounds.set_width(1);
    window_bounds.set_height(1);
  }
  return window_bounds;
}

int VivaldiWindowFrameViewAura::NonClientHitTest(const gfx::Point& point) {
  if (ViewsWidget()->IsFullscreen())
    return HTCLIENT;

  gfx::Rect expanded_bounds = bounds();
  // Points outside the (possibly expanded) bounds can be discarded.
  if (!expanded_bounds.Contains(point))
    return HTNOWHERE;

  // Check the frame first, as we allow a small area overlapping the contents
  // to be used for resize handles.
  bool can_ever_resize = ViewsWidget()->widget_delegate()
                             ? ViewsWidget()->widget_delegate()->CanResize()
                             : false;
  // Don't allow overlapping resize handles when the window is maximized or
  // fullscreen, as it can't be resized in those states.
  int resize_border =
      (ViewsWidget()->IsMaximized() || ViewsWidget()->IsFullscreen())
          ? 0
          : resize_inside_bounds_size_;
  int frame_component = GetHTComponentForFrame(
      point, gfx::Insets(resize_border), resize_area_corner_size_,
      resize_area_corner_size_, can_ever_resize);
  if (frame_component != HTNOWHERE)
    return frame_component;

  // Check for possible draggable region in the client area for the frameless
  // window.
  SkRegion* draggable_region = views_->draggable_region();
  if (draggable_region && draggable_region->contains(point.x(), point.y()))
    return HTCAPTION;

  int client_component = ViewsWidget()->client_view()->NonClientHitTest(point);
  if (client_component != HTNOWHERE)
    return client_component;

  // Caption is a safe default.
  return HTCAPTION;
}

void VivaldiWindowFrameViewAura::GetWindowMask(const gfx::Size& size,
                                               SkPath* window_mask) {
  // We got nothing to say about no window mask.
}

void VivaldiWindowFrameViewAura::SizeConstraintsChanged() {}

gfx::Size VivaldiWindowFrameViewAura::CalculatePreferredSize() const {
  gfx::Size pref = ViewsWidget()->client_view()->GetPreferredSize();
  gfx::Rect bounds(0, 0, pref.width(), pref.height());
  return ViewsWidget()
      ->non_client_view()
      ->GetWindowBoundsForClientBounds(bounds)
      .size();
}

void VivaldiWindowFrameViewAura::Layout() {
  NonClientFrameView::Layout();
}

void VivaldiWindowFrameViewAura::OnPaint(gfx::Canvas* canvas) {}

const char* VivaldiWindowFrameViewAura::GetClassName() const {
  return kViewClassName;
}

gfx::Size VivaldiWindowFrameViewAura::GetMinimumSize() const {
  gfx::Size min_size = ViewsWidget()->client_view()->GetMinimumSize();
  min_size.SetToMax(gfx::Size(1, 1));
  return min_size;
}

gfx::Size VivaldiWindowFrameViewAura::GetMaximumSize() const {
  gfx::Size max_size = ViewsWidget()->client_view()->GetMaximumSize();

  // Add to the client maximum size the height of any title bar and borders.
  gfx::Size client_size = GetBoundsForClientView().size();
  if (max_size.width())
    max_size.Enlarge(width() - client_size.width(), 0);
  if (max_size.height())
    max_size.Enlarge(0, height() - client_size.height());

  return max_size;
}
