// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/events/vivaldi_ui_events.h"

#include "components/input/native_web_keyboard_event.h"
#include "extensions/schema/tabs_private.h"
#include "extensions/tools/vivaldi_tools.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace extensions {

namespace tabs_private = vivaldi::tabs_private;

// static
void VivaldiUIEvents::SendKeyboardShortcutEvent(
    SessionID::id_type window_id,
    content::BrowserContext* browser_context,
    const input::NativeWebKeyboardEvent& event,
    bool is_auto_repeat,
    bool forced_browser_priority) {
  // We don't allow AltGr keyboard shortcuts
  if (event.GetModifiers() & blink::WebInputEvent::kAltGrKey)
    return;
  // Don't send if event contains only modifiers.
  int key_code = event.windows_key_code;
  if (key_code == ui::VKEY_CONTROL || key_code == ui::VKEY_SHIFT ||
      key_code == ui::VKEY_MENU) {
    return;
  }
  if (event.GetType() == blink::WebInputEvent::Type::kKeyUp)
    return;

  std::string shortcut_text = ::vivaldi::ShortcutTextFromEvent(event);

  // If the event wasn't prevented we'll get a rawKeyDown event. In some
  // exceptional cases we'll never get that, so we let these through
  // unconditionally
  std::vector<std::string> exceptions = {"Up", "Down", "Shift+Delete",
                                         "Meta+Shift+V", "Esc"};
  bool is_exception = std::find(exceptions.begin(), exceptions.end(),
                                shortcut_text) != exceptions.end();
  if (event.GetType() == blink::WebInputEvent::Type::kRawKeyDown ||
      is_exception) {
    ::vivaldi::BroadcastEvent(tabs_private::OnKeyboardShortcut::kEventName,
                              tabs_private::OnKeyboardShortcut::Create(
                                  window_id, shortcut_text, is_auto_repeat,
                                  event.from_devtools, forced_browser_priority),
                              browser_context);
  }
}

}  // namespace extensions
