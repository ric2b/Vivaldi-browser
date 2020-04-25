// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include <memory>
#include <string>
#include <utility>

#include "extensions/tools/vivaldi_tools.h"

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/public/common/page_zoom.h"
#include "components/zoom/zoom_controller.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/extensions/command.h"
#include "extensions/api/bookmarks/bookmarks_private_api.h"
#include "extensions/browser/event_router.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/vivaldi_browser_window.h"
#include "chrome/browser/profiles/profile.h"

namespace vivaldi {


SetPartnerUpgrade::SetPartnerUpgrade(content::BrowserContext* context,
                                     bool active)
  :context_(context) {
  Set(active);
}

SetPartnerUpgrade::~SetPartnerUpgrade() {
  Set(false);
}

void SetPartnerUpgrade::Set(bool active) {
  extensions::VivaldiBookmarksAPI* api =
        extensions::VivaldiBookmarksAPI::GetFactoryInstance()->GetIfExists(
            context_);
  if (api) {
    api->SetPartnerUpgradeActive(active);
  }
}


// Do not use this function in new code. It does not work with multiple
// profiles.
Browser* FindVivaldiBrowser() {
  BrowserList* browser_list_impl = BrowserList::GetInstance();
  if (browser_list_impl && browser_list_impl->size() > 0)
    return browser_list_impl->get(0);
  return NULL;
}

void BroadcastEvent(const std::string& eventname,
                    std::unique_ptr<base::ListValue> args,
                    content::BrowserContext* context) {
  std::unique_ptr<extensions::Event> event(new extensions::Event(
    extensions::events::VIVALDI_EXTENSION_EVENT, eventname, std::move(args)));
  extensions::EventRouter* event_router = extensions::EventRouter::Get(context);
  if (event_router) {
    event_router->BroadcastEvent(std::move(event));
  }
}  // namespace

// Start chromium copied code
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

double MilliSecondsFromTime(const base::Time& time) {
  return 1000 * time.ToDoubleT();
}

base::Time GetTime(double ms_from_epoch) {
  double seconds_from_epoch = ms_from_epoch / 1000.0;
  return (seconds_from_epoch == 0)
    ? base::Time::UnixEpoch()
    : base::Time::FromDoubleT(seconds_from_epoch);
}

blink::WebFloatPoint FromUICoordinates(content::WebContents* web_contents,
                                       blink::WebFloatPoint p) {
  // Account for the zoom factor in the UI.
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);
  if (!zoom_controller)
    return p;
  double zoom_factor =
      content::ZoomLevelToZoomFactor(zoom_controller->GetZoomLevel());
  return blink::WebFloatPoint(p.x * zoom_factor, p.y * zoom_factor);
}

blink::WebFloatPoint ToUICoordinates(content::WebContents* web_contents,
                                     blink::WebFloatPoint p) {
  // Account for the zoom factor in the UI.
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);
  if (!zoom_controller)
    return p;
  double zoom_factor =
      content::ZoomLevelToZoomFactor(zoom_controller->GetZoomLevel());
  return blink::WebFloatPoint(p.x / zoom_factor, p.y / zoom_factor);
}

#if defined(OS_MACOSX)

base::string16 KeyCodeToName(ui::KeyboardCode key_code) {
  int string_id = 0;
  switch (key_code) {
    case ui::VKEY_TAB:
      string_id = IDS_APP_TAB_KEY;
      break;
    case ui::VKEY_RETURN:
      string_id = IDS_APP_ENTER_KEY;
      break;
    case ui::VKEY_SPACE:
      string_id = IDS_APP_SPACE_KEY;
      break;
    case ui::VKEY_PRIOR:
      string_id = IDS_APP_PAGEUP_KEY;
      break;
    case ui::VKEY_NEXT:
      string_id = IDS_APP_PAGEDOWN_KEY;
      break;
    case ui::VKEY_END:
      string_id = IDS_APP_END_KEY;
      break;
    case ui::VKEY_HOME:
      string_id = IDS_APP_HOME_KEY;
      break;
    case ui::VKEY_INSERT:
      string_id = IDS_APP_INSERT_KEY;
      break;
    case ui::VKEY_DELETE:
      string_id = IDS_APP_DELETE_KEY;
      break;
    case ui::VKEY_LEFT:
      string_id = IDS_APP_LEFT_ARROW_KEY;
      break;
    case ui::VKEY_RIGHT:
      string_id =IDS_APP_RIGHT_ARROW_KEY;
      break;
    case ui::VKEY_UP:
      string_id = IDS_APP_UP_ARROW_KEY;
      break;
    case ui::VKEY_DOWN:
      string_id = IDS_APP_DOWN_ARROW_KEY;
      break;
    case ui::VKEY_ESCAPE:
      string_id = IDS_APP_ESC_KEY;
      break;
    case ui::VKEY_BACK:
      string_id = IDS_APP_BACKSPACE_KEY;
      break;
    case ui::VKEY_F1:
      string_id = IDS_APP_F1_KEY;
      break;
    case ui::VKEY_F11:
      string_id = IDS_APP_F11_KEY;
      break;
    case ui::VKEY_OEM_COMMA:
      string_id = IDS_APP_COMMA_KEY;
      break;
    case ui::VKEY_OEM_PERIOD:
      string_id = IDS_APP_PERIOD_KEY;
      break;
    case ui::VKEY_MEDIA_NEXT_TRACK:
      string_id = IDS_APP_MEDIA_NEXT_TRACK_KEY;
      break;
    case ui::VKEY_MEDIA_PLAY_PAUSE:
      string_id = IDS_APP_MEDIA_PLAY_PAUSE_KEY;
      break;
    case ui::VKEY_MEDIA_PREV_TRACK:
      string_id = IDS_APP_MEDIA_PREV_TRACK_KEY;
      break;
    case ui::VKEY_MEDIA_STOP:
      string_id = IDS_APP_MEDIA_STOP_KEY;
      break;
    default:
      break;
  }
  return string_id ? l10n_util::GetStringUTF16(string_id) : base::string16();
}

#endif // OS_MACOS

std::string ShortcutTextFromEvent(
    const content::NativeWebKeyboardEvent& event) {
  return ::vivaldi::ShortcutText(
      event.windows_key_code,
      ui::WebEventModifiersToEventFlags(event.GetModifiers()), event.dom_code);
}

std::string ShortcutText(int windows_key_code, int modifiers, int dom_code) {
  // We'd just use Accelerator::GetShortcutText to get the shortcut text but
  // it translates the modifiers when the system language is set to
  // non-English (since it's used for display). We can't match something
  // like 'Strg+G' however, so we do the modifiers manually.
  //
  // AcceleratorToString gets the shortcut text, but doesn't localize
  // like Accelerator::GetShortcutText() does, so it's suitable for us.
  // It doesn't handle all keys, however, and doesn't work with ctrl+alt
  // shortcuts so we're left with doing a little tweaking.
  ui::KeyboardCode key_code =
      static_cast<ui::KeyboardCode>(windows_key_code);
  ui::Accelerator accelerator =
      ui::Accelerator(key_code, 0, ui::Accelerator::KeyState::PRESSED);

  std::string shortcutText = "";
  if (modifiers & ui::EF_CONTROL_DOWN) {
    shortcutText += "Ctrl+";
  }
  if (modifiers & ui::EF_SHIFT_DOWN) {
    shortcutText += "Shift+";
  }
  if (modifiers & ui::EF_ALT_DOWN) {
    shortcutText += "Alt+";
  }
  if (modifiers & ui::EF_COMMAND_DOWN) {
    shortcutText += "Meta+";
  }

  std::string key_from_accelerator =
      extensions::Command::AcceleratorToString(accelerator);
  if (!key_from_accelerator.empty()) {
    shortcutText += key_from_accelerator;
  } else if (windows_key_code >= ui::VKEY_F1 &&
             windows_key_code <= ui::VKEY_F24) {
    char buf[4];
    base::snprintf(buf, 4, "F%d", windows_key_code - ui::VKEY_F1 + 1);
    shortcutText += buf;

  } else if (windows_key_code >= ui::VKEY_NUMPAD0 &&
             windows_key_code <= ui::VKEY_NUMPAD9) {
    char buf[8];
    base::snprintf(buf, 8, "Numpad%d",
                   windows_key_code - ui::VKEY_NUMPAD0);
    shortcutText += buf;

  // Enter is somehow not covered anywhere else.
  } else if (windows_key_code == ui::VKEY_RETURN) {
    shortcutText += "Enter";
  // GetShortcutText doesn't translate numbers and digits but
  // 'does' translate backspace
  } else if (windows_key_code == ui::VKEY_BACK) {
    shortcutText += "Backspace";
  // Escape was being translated as well in some languages
  } else if (windows_key_code == ui::VKEY_ESCAPE) {
    shortcutText += "Esc";
  } else {
#if defined(OS_MACOSX)
  // This is equivalent to js event.code and deals with a few
  // MacOS keyboard shortcuts like cmd+alt+n that fall through
  // in some languages, i.e. AcceleratorToString returns a blank.
  // Cmd+Alt shortcuts seem to be the only case where this fallback
  // is required.
  if (modifiers & content::NativeWebKeyboardEvent::kAltKey &&
      modifiers & content::NativeWebKeyboardEvent::kMetaKey) {
    shortcutText += ui::DomCodeToUsLayoutCharacter(
        static_cast<ui::DomCode>(dom_code), 0);
  } else {
    // With chrome 67 accelerator.GetShortcutText() will return Mac specific
    // symbols (like '⎋' for escape). All is private so we bypass that by
    // testing with KeyCodeToName first.
    base::string16 shortcut = KeyCodeToName(key_code);
    if (shortcut.empty())
      shortcut = accelerator.GetShortcutText();
    shortcutText += base::UTF16ToUTF8(shortcut);
  }
#else
    shortcutText += base::UTF16ToUTF8(accelerator.GetShortcutText());
#endif // OS_MACOSX
  }
  return shortcutText;
}

}  // namespace vivaldi
