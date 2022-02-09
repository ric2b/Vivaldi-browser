// Copyright 2020 Vivaldi Technologies. All rights reserved.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_VIVALDI_WINDOW_SIZE_HELPER_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_VIVALDI_WINDOW_SIZE_HELPER_H_

#import <Cocoa/Cocoa.h>

#include "ui/display/display.h"

namespace vivaldi {

void VerifyWindowSize(NSWindow* window, const display::Display& old_display);

}  // namespace vivaldi

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_VIVALDI_WINDOW_SIZE_HELPER_H_