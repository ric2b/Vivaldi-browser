// Copyright 2020 Vivaldi Technologies. All rights reserved.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_VIVALDI_WINDOW_SIZE_HELPER_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_VIVALDI_WINDOW_SIZE_HELPER_H_

#import <Cocoa/Cocoa.h>

#include "ui/display/display.h"

namespace display {

using Displays = std::vector<Display>;

} // namespace display

namespace vivaldi {

void VerifyWindowSize(NSWindow* window, const display::Displays& old_displays);

}  // namespace vivaldi

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_VIVALDI_WINDOW_SIZE_HELPER_H_
