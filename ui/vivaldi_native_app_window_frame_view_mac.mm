// Copyright 2018 Vivaldi Technologies. All rights reserved.

#include "ui/vivaldi_native_app_window_frame_view_mac.h"

VivaldiNativeAppWindowFrameViewMac::VivaldiNativeAppWindowFrameViewMac(
    views::Widget* frame,
    extensions::NativeAppWindow* window)
    : NativeAppWindowFrameViewMac(frame, window) {
}

VivaldiNativeAppWindowFrameViewMac::~VivaldiNativeAppWindowFrameViewMac() {
}

void VivaldiNativeAppWindowFrameViewMac::Layout() {
}