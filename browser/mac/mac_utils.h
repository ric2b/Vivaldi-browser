// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef VIVALDI_BROWSER_MAC_MAC_UTILS_H
#define VIVALDI_BROWSER_MAC_MAC_UTILS_H

#import <Cocoa/Cocoa.h>

namespace vivaldi {

bool CanEventExecuteCommand(NSEvent* event);

}  // namespace vivaldi


#endif  // VIVALDI_BROWSER_MAC_MAC_UTILS_H
