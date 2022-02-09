// Copyright 2018 Vivaldi Technologies. All rights reserved.

#ifndef UI_VIVALDI_NATIVE_APP_WINDOW_FRAME_VIEW_MAC_H_
#define UI_VIVALDI_NATIVE_APP_WINDOW_FRAME_VIEW_MAC_H_

#include "ui/views/window/native_frame_view.h"

class VivaldiNativeAppWindowViews;

// Provides metrics consistent with a native frame on Mac. The actual frame is
// drawn by NSWindow.
class VivaldiNativeAppWindowFrameViewMac : public views::NativeFrameView {
 public:
  VivaldiNativeAppWindowFrameViewMac(VivaldiNativeAppWindowViews* views_);
  ~VivaldiNativeAppWindowFrameViewMac() override;
  VivaldiNativeAppWindowFrameViewMac(
      const VivaldiNativeAppWindowFrameViewMac&) = delete;
  VivaldiNativeAppWindowFrameViewMac& operator=(
      const VivaldiNativeAppWindowFrameViewMac&) = delete;

  void Layout() override;

  // NonClientFrameView:
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;

 private:
  // Owner.
  VivaldiNativeAppWindowViews* const views_;
};

#endif  // UI_VIVALDI_NATIVE_APP_WINDOW_FRAME_VIEW_MAC_H_
