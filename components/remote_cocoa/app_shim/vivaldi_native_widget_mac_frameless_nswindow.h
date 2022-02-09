// Copyright 2020 Vivaldi Technologies. All rights reserved.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_VIVALDI_NATIVE_WIDGET_MAC_FRAMELESS_NSWINDOW_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_VIVALDI_NATIVE_WIDGET_MAC_FRAMELESS_NSWINDOW_H_

#import "components/remote_cocoa/app_shim/native_widget_mac_frameless_nswindow.h"

// Overrides mouseDown to handle double clicking of the titlebar on macOS 10.15+
@interface VivaldiNativeWidgetMacFramelessNSWindow
    : NativeWidgetMacFramelessNSWindow
@end

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_VIVALDI_NATIVE_WIDGET_MAC_FRAMELESS_NSWINDOW_H_
