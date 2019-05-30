// Copyright 2018 Vivaldi Technologies. All rights reserved.

#ifndef UI_VIVALDI_NATIVE_APP_WINDOW_FRAME_VIEW_MAC_H_
#define UI_VIVALDI_NATIVE_APP_WINDOW_FRAME_VIEW_MAC_H_

#include "chrome/browser/ui/views/apps/native_app_window_frame_view_mac.h"

class Widget;

// Provides metrics consistent with a native frame on Mac. The actual frame is
// drawn by NSWindow.
class VivaldiNativeAppWindowFrameViewMac : public NativeAppWindowFrameViewMac {
 public:
  VivaldiNativeAppWindowFrameViewMac(views::Widget* frame,
                              extensions::NativeAppWindow* window);
  ~VivaldiNativeAppWindowFrameViewMac() override;

  void Layout() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(VivaldiNativeAppWindowFrameViewMac);
};

#endif  // UI_VIVALDI_NATIVE_APP_WINDOW_FRAME_VIEW_MAC_H_
