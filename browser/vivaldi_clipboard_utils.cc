// Copyright (c) 2016 Vivaldi. All rights reserved.

#include "browser/vivaldi_clipboard_utils.h"

#include "third_party/blink/public/platform/web_keyboard_event.h"
#include "third_party/blink/public/platform/web_mouse_event.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace vivaldi {

namespace clipboard {

bool suppress_selection_write = true;

// Updates 'suppress_selection_write' for each event.
void OnInputEvent(const blink::WebInputEvent& input_event) {
  if (input_event.GetType() == blink::WebInputEvent::kMouseMove) {
    // Never set to true here to allow mouse multiclicking work the best.
    if ((input_event.GetModifiers() & blink::WebInputEvent::kLeftButtonDown))
      suppress_selection_write = false;
  } else if (input_event.GetType() == blink::WebInputEvent::kMouseDown) {
    blink::WebMouseEvent& event = *((blink::WebMouseEvent*)&input_event);
    suppress_selection_write = event.click_count < 2;
  } else if (input_event.GetType() == blink::WebInputEvent::kRawKeyDown &&
             (input_event.GetModifiers() & blink::WebInputEvent::kShiftKey)) {
    blink::WebKeyboardEvent& event = *((blink::WebKeyboardEvent*)&input_event);
    suppress_selection_write = !(event.windows_key_code == ui::VKEY_LEFT ||
                                 event.windows_key_code == ui::VKEY_RIGHT ||
                                 event.windows_key_code == ui::VKEY_UP ||
                                 event.windows_key_code == ui::VKEY_DOWN ||
                                 event.windows_key_code == ui::VKEY_HOME ||
                                 event.windows_key_code == ui::VKEY_END ||
                                 event.windows_key_code == ui::VKEY_PRIOR ||
                                 event.windows_key_code == ui::VKEY_NEXT);
  } else if (input_event.GetType() == blink::WebInputEvent::kRawKeyDown &&
             (input_event.GetModifiers() & blink::WebInputEvent::kControlKey)) {
    blink::WebKeyboardEvent& event = *((blink::WebKeyboardEvent*)&input_event);
    // NOTE(espen). We probably want to make this configurable
    // Ctrl+A: Select All.
    suppress_selection_write = event.windows_key_code != ui::VKEY_A;
  } else if (input_event.GetType() == blink::WebInputEvent::kChar) {
    // Do nothing. Wait for KeyUp to set suppress_selection_write to true.
  } else {
    suppress_selection_write = true;
  }
}

bool SuppressWrite(ui::ClipboardType clipboardType) {
  if (clipboardType == ui::CLIPBOARD_TYPE_SELECTION) {
    return suppress_selection_write;
  }
  return false;
}

}  // namespace clipboard

}  // namespace vivaldi
