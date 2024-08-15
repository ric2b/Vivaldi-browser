// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/vivaldi_window_frame_view.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "chrome/grit/theme_resources.h"
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
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/non_client_view.h"

#include "ui/vivaldi_browser_window.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace {

class VivaldiWindowFrameViewAura : public views::NonClientFrameView {
 public:
  // VivaldiWindowFrameViewAura is used to draw frames for app windows when a
  // non standard frame is needed. This occurs if there is no frame needed, or
  // if there is a frame color.
  VivaldiWindowFrameViewAura(VivaldiBrowserWindow* window);
  ~VivaldiWindowFrameViewAura() override;
  VivaldiWindowFrameViewAura(const VivaldiWindowFrameViewAura&) = delete;
  VivaldiWindowFrameViewAura& operator=(const VivaldiWindowFrameViewAura&) =
      delete;

 private:
  // views::NonClientFrameView overrides

  int NonClientHitTest(const gfx::Point& point) override;
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  void OnPaint(gfx::Canvas* canvas) override;
  void SizeConstraintsChanged() override;
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override;
  void UpdateWindowIcon() override {}
  void UpdateWindowTitle() override {}

  // views::View implementation.
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;

  raw_ptr<VivaldiBrowserWindow> window_;
};

VivaldiWindowFrameViewAura::VivaldiWindowFrameViewAura(
    VivaldiBrowserWindow* window)
    : window_(window) {}

VivaldiWindowFrameViewAura::~VivaldiWindowFrameViewAura() = default;

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
  if (views::Widget* widget = window_->GetWidget()) {
    gfx::Insets native_frame_insets =
        widget->GetClientAreaBoundsInScreen().InsetsFrom(
            widget->GetWindowBoundsInScreen());
    window_bounds.Inset(native_frame_insets);
  }
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
  views::Widget* widget = window_->GetWidget();
  if (!widget)
    return HTNOWHERE;

  if (widget->IsFullscreen())
    return HTCLIENT;

  gfx::Rect expanded_bounds = bounds();
  // Points outside the (possibly expanded) bounds can be discarded.
  if (!expanded_bounds.Contains(point))
    return HTNOWHERE;

#if BUILDFLAG(IS_WIN)
  if (window_->GetMaximizeButtonBounds().Contains(point)) {
    if (base::win::GetVersion() >= base::win::Version::WIN11) {
      return HTMAXBUTTON;
    }
  }
#endif

  // Check the frame first, as we allow a small area overlapping the contents
  // to be used for resize handles.
  bool can_ever_resize = widget->widget_delegate()
                             ? widget->widget_delegate()->CanResize()
                             : false;
  // Don't allow overlapping resize handles when the window is maximized or
  // fullscreen, as it can't be resized in those states.
  int resize_border = (widget->IsMaximized() || widget->IsFullscreen())
                          ? 0
                          : window_->resize_inside_bounds_size();
  int resize_corner_size = window_->resize_area_corner_size();
  int frame_component = GetHTComponentForFrame(
      point, gfx::Insets(resize_border), resize_corner_size, resize_corner_size,
      can_ever_resize);
  if (frame_component != HTNOWHERE)
    return frame_component;

  // Check for possible draggable region in the client area for the frameless
  // window.
  const SkRegion* draggable_region = window_->draggable_region();
  if (draggable_region && draggable_region->contains(point.x(), point.y()))
    return HTCAPTION;

  int client_component = widget->client_view()->NonClientHitTest(point);
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

gfx::Size VivaldiWindowFrameViewAura::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  views::Widget* widget = window_->GetWidget();
  if (!widget)
    return gfx::Size();
  gfx::Size pref = widget->client_view()->GetPreferredSize();
  gfx::Rect bounds(0, 0, pref.width(), pref.height());
  return widget->non_client_view()
      ->GetWindowBoundsForClientBounds(bounds)
      .size();
}

void VivaldiWindowFrameViewAura::OnPaint(gfx::Canvas* canvas) {}

gfx::Size VivaldiWindowFrameViewAura::GetMinimumSize() const {
  views::Widget* widget = window_->GetWidget();
  if (!widget) {
    LOG(ERROR) << "GetMinimumSize called with no widget";
    return gfx::Size(1,1);
  }
  gfx::Size min_size = widget->client_view()->GetMinimumSize();
  min_size.SetToMax(gfx::Size(1, 1));
  return min_size;
}

gfx::Size VivaldiWindowFrameViewAura::GetMaximumSize() const {
  views::Widget* widget = window_->GetWidget();
  if (!widget)
    return gfx::Size();
  gfx::Size max_size = widget->client_view()->GetMaximumSize();

  // Add to the client maximum size the height of any title bar and borders.
  gfx::Size client_size = GetBoundsForClientView().size();
  if (max_size.width())
    max_size.Enlarge(width() - client_size.width(), 0);
  if (max_size.height())
    max_size.Enlarge(0, height() - client_size.height());

  return max_size;
}

}  // namespace

std::unique_ptr<views::NonClientFrameView> CreateVivaldiWindowFrameView(
    VivaldiBrowserWindow* window) {
  return std::make_unique<VivaldiWindowFrameViewAura>(window);
}
