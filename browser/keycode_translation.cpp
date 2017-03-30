// Copyright (c) 2016 Vivaldi. All rights reserved.

#include "browser/keycode_translation.h"

#if defined(USE_X11)
#include <X11/Xlib.h>
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#endif

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace vivaldi {

void setKeyIdentifierFromXEvent(const ui::KeyEvent& event,
                                char* keyIdentifier,
                                size_t keyIdentifier_len,
                                unsigned short windowsKeyCode) {
#if defined(USE_X11)
  uint16_t keyId = windowsKeyCode;

  // NOTE(daniel@vivaldi): Don't convert numbers
  // (in the case of the 'azerty' layout this would be wrong).
  if ((keyId >= '0' && keyId <= '9') || (keyId >= 'A' && keyId <= 'Z')) {
    return;
  } else if (event.native_event() != NULL) {
    XEvent xev = *event.native_event();

    // Holding down ctrl changes the character for some reason.
    // We release all modifiers and get what was really pressed
    xev.xkey.state = 0;

    keyId = ui::GetCharacterFromXEvent(&xev);
    base::snprintf(keyIdentifier, keyIdentifier_len, "U+%04X",
                   toupper(keyId));
  }
#endif
}

wchar_t setKeyIdentifierWithWinapi(unsigned short windowsKeyCode) {
  // NOTE(daniel@vivaldi): This code is copied from accelerator.cc It
  // does the correct conversion from windows_key_code to actual unicode
  // code point (locale specific). This is used by the keyIdentifier field
  // in keyboard events.
  wchar_t keyId = windowsKeyCode;

#if defined(OS_WIN)
  ui::KeyboardCode key_code = (ui::KeyboardCode)windowsKeyCode;
  if ((key_code >= '0' && key_code <= '9') ||
      (key_code >= 'A' && key_code <= 'Z'))
    keyId = windowsKeyCode;
  else
    keyId = LOWORD(::MapVirtualKeyW(key_code, MAPVK_VK_TO_CHAR));
#endif
  return keyId;
}

}  // namespace vivaldi
