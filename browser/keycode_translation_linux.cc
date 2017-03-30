// Copyright (c) 2016 Vivaldi. All rights reserved.

#include "browser/keycode_translation.h"

#include <X11/Xlib.h>
#include "ui/events/keycodes/keyboard_code_conversion_x.h"

namespace vivaldi {

void setKeyIdentifierFromXEvent(const ui::KeyEvent& event,
                                char* keyIdentifier,
                                size_t keyIdentifier_len,
                                unsigned short windowsKeyCode) {
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
}

}  // namespace vivaldi
