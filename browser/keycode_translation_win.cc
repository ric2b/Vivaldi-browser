// Copyright (c) 2016 Vivaldi. All rights reserved.

#include "browser/keycode_translation.h"

#include <windows.h>

namespace vivaldi {

wchar_t setKeyIdentifierWithWinapi(unsigned short windowsKeyCode) {
  // NOTE(daniel@vivaldi): This code is copied from accelerator.cc It
  // does the correct conversion from windows_key_code to actual unicode
  // code point (locale specific). This is used by the keyIdentifier field
  // in keyboard events.
  wchar_t keyId = windowsKeyCode;

  ui::KeyboardCode key_code = (ui::KeyboardCode)windowsKeyCode;
  if ((key_code >= '0' && key_code <= '9') ||
      (key_code >= 'A' && key_code <= 'Z'))
    keyId = windowsKeyCode;
  else
    keyId = LOWORD(::MapVirtualKeyW(key_code, MAPVK_VK_TO_CHAR));
  return keyId;
}

}  // namespace vivaldi
