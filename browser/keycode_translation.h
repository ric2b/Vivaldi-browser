// Copyright (c) 2016 Vivaldi. All rights reserved.

#ifndef BROWSER_KEYCODE_TRANSLATION_H_
#define BROWSER_KEYCODE_TRANSLATION_H_

#if defined(USE_X11)
#include <X11/Xlib.h>
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#endif

#include "ui/events/event.h"

namespace vivaldi {

void setKeyIdentifierFromXEvent(const ui::KeyEvent& event,
                                char* keyIdentifier,
                                size_t keyIdentifier_len,
                                unsigned short windowsKeyCode);

wchar_t setKeyIdentifierWithWinapi(unsigned short windowsKeyCode);
}

#endif  // BROWSER_KEYCODE_TRANSLATION_H_
