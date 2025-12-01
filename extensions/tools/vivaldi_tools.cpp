// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/tools/vivaldi_tools.h"

#include <memory>
#include <string>
#include <utility>

#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "components/zoom/zoom_controller.h"
#include "extensions/api/bookmarks/bookmarks_private_api.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/common/command.h"
#include "extensions/vivaldi_browser_component_wrapper.h"
#include "prefs/vivaldi_gen_prefs.h"
#include "prefs/vivaldi_pref_names.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/accelerators/command_constants.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/vivaldi_browser_window.h"

namespace vivaldi {

// These symbols do not not exist in chrome or the definition differs
// from vivaldi.
const char* kVivaldiKeyEsc = "Esc";
const char* kVivaldiKeyDel = "Del";
const char* kVivaldiKeyIns = "Ins";
const char* kVivaldiKeyPgUp = "Pageup";
const char* kVivaldiKeyPgDn = "Pagedown";
const char* kVivaldiKeyMultiply = "*";
const char* kVivaldiKeyDivide = "/";
const char* kVivaldiKeySubtract = "-";
const char* kVivaldiKeyPeriod = ".";
const char* kVivaldiKeyComma = ",";
const char* kVivaldiKeyBackslash = "\\";


// Local copy of similar function in blink and aura (former is in a module we
// can not link it and latter is not used by Mac).
int WebEventModifiersToEventFlags(int modifiers) {
  int flags = 0;
  if (modifiers & blink::WebInputEvent::kShiftKey) {
    flags |= ui::EF_SHIFT_DOWN;
  }
  if (modifiers & blink::WebInputEvent::kControlKey) {
    flags |= ui::EF_CONTROL_DOWN;
  }
  if (modifiers & blink::WebInputEvent::kAltKey) {
    flags |= ui::EF_ALT_DOWN;
  }
  if (modifiers & blink::WebInputEvent::kMetaKey) {
    flags |= ui::EF_COMMAND_DOWN;
  }
  if (modifiers & blink::WebInputEvent::kAltGrKey) {
    flags |= ui::EF_ALTGR_DOWN;
  }
  if (modifiers & blink::WebInputEvent::kNumLockOn) {
    flags |= ui::EF_NUM_LOCK_ON;
  }
  if (modifiers & blink::WebInputEvent::kCapsLockOn) {
    flags |= ui::EF_CAPS_LOCK_ON;
  }
  if (modifiers & blink::WebInputEvent::kScrollLockOn) {
    flags |= ui::EF_SCROLL_LOCK_ON;
  }
  if (modifiers & blink::WebInputEvent::kLeftButtonDown) {
    flags |= ui::EF_LEFT_MOUSE_BUTTON;
  }
  if (modifiers & blink::WebInputEvent::kMiddleButtonDown) {
    flags |= ui::EF_MIDDLE_MOUSE_BUTTON;
  }
  if (modifiers & blink::WebInputEvent::kRightButtonDown) {
    flags |= ui::EF_RIGHT_MOUSE_BUTTON;
  }
  if (modifiers & blink::WebInputEvent::kBackButtonDown) {
    flags |= ui::EF_BACK_MOUSE_BUTTON;
  }
  if (modifiers & blink::WebInputEvent::kForwardButtonDown) {
    flags |= ui::EF_FORWARD_MOUSE_BUTTON;
  }
  if (modifiers & blink::WebInputEvent::kIsAutoRepeat) {
    flags |= ui::EF_IS_REPEAT;
  }
  if (modifiers & blink::WebInputEvent::kIsTouchAccessibility) {
    flags |= ui::EF_TOUCH_ACCESSIBILITY;
  }
  return flags;
}

ui::KeyboardCode GetFunctionKey(std::string token) {
  int size = token.size();
  if (size >= 2 && std::toupper(token[0]) == 'F') {
    if (size == 2 && token[1] >= '1' && token[1] <= '9')
      return static_cast<ui::KeyboardCode>(ui::VKEY_F1 + (token[1] - '1'));
    if (size == 3 && token[1] == '1' && token[2] >= '0' && token[2] <= '9')
      return static_cast<ui::KeyboardCode>(ui::VKEY_F10 + (token[2] - '0'));
    if (size == 3 && token[1] == '2' && token[2] >= '0' && token[2] <= '4')
      return static_cast<ui::KeyboardCode>(ui::VKEY_F20 + (token[2] - '0'));
  }
  return ui::VKEY_UNKNOWN;
}

std::string GetMacOSEmailLinkShortcut(Profile* profile) {
  // VB-107999 User configurable shortcut for share Email Link menuitem
  if (!profile) {
    return "";
  }

  PrefService* prefs = profile->GetPrefs();
  auto& vivaldi_actions = prefs->GetList(vivaldiprefs::kActions);
  const base::Value::Dict* dict = vivaldi_actions[0].GetIfDict();
  const base::Value::Dict* command =
      dict->FindDict("COMMAND_EMAIL_LINK_OVERRIDE");

  if (command) {
    const base::Value::List* shortcuts = command->FindList("shortcuts");
    if (shortcuts) {
      for (auto keycombo = shortcuts->begin(); keycombo != shortcuts->end();
           ++keycombo) {
        // We can only assign one keycombo to main menu shortcut, choose the
        // first one
        if (keycombo->is_string()) {
          return keycombo->GetString();
        }
      }
    }
  }
  return "";
}

ui::Accelerator VivaldiShortcut2Accelerator(const std::string& shortcut) {
  std::vector<std::string> tokens = base::SplitString(
    shortcut, "+", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (tokens.size() == 0) {
    return ui::Accelerator();
  }

  int modifiers = ui::EF_NONE;
  ui::KeyboardCode key = ui::VKEY_UNKNOWN;
  for (size_t i = 0; i < tokens.size(); i++) {
    if (tokens[i] == "ctrl") {
      modifiers |= ui::EF_CONTROL_DOWN;
    } else if (tokens[i] == "alt") {
      modifiers |= ui::EF_ALT_DOWN;
    } else if (tokens[i] == "shift") {
      modifiers |= ui::EF_SHIFT_DOWN;
    } else if (tokens[i] == "meta") {
      modifiers |= ui::EF_COMMAND_DOWN;
    } else if (key == ui::VKEY_UNKNOWN) {
      if (tokens[i] == ui::kKeyUp) {
        key = ui::VKEY_UP;
      } else if (tokens[i] == ui::kKeyDown) {
        key = ui::VKEY_DOWN;
      } else if (tokens[i] == ui::kKeyLeft) {
        key = ui::VKEY_LEFT;
      } else if (tokens[i] == ui::kKeyRight) {
        key = ui::VKEY_RIGHT;
      } else if (tokens[i] == ui::kKeyIns) {
        key = ui::VKEY_INSERT;
      } else if (tokens[i] == ui::kKeyDel) {
        key = ui::VKEY_DELETE;
      } else if (tokens[i] == ui::kKeyHome) {
        key = ui::VKEY_HOME;
      } else if (tokens[i] == ui::kKeyEnd) {
        key = ui::VKEY_END;
      } else if (tokens[i] == ui::kKeyPgUp) {
        key = ui::VKEY_PRIOR;
      } else if (tokens[i] == ui::kKeyPgDwn) {
        key = ui::VKEY_NEXT;
      } else if (tokens[i] == ui::kKeySpace) {
        key = ui::VKEY_SPACE;
      } else if (tokens[i] == ui::kKeyTab) {
        key = ui::VKEY_TAB;
      } else if (tokens[i] == kVivaldiKeyPeriod) {
        key = ui::VKEY_OEM_PERIOD;
      } else if (tokens[i] == kVivaldiKeyComma) {
        key = ui::VKEY_OEM_COMMA;
      } else if (tokens[i] == kVivaldiKeyBackslash) {
        key = ui::VKEY_OEM_5;
      } else if (tokens[i] == kVivaldiKeyEsc) {
        key = ui::VKEY_ESCAPE;
      } else if (tokens[i] == kVivaldiKeyDel) {
        key = ui::VKEY_DELETE;
      } else if (tokens[i] == kVivaldiKeyIns) {
        key = ui::VKEY_ESCAPE;
      } else if (tokens[i] == kVivaldiKeyPgUp) {
        key = ui::VKEY_PRIOR;
      } else if (tokens[i] == kVivaldiKeyPgDn) {
        key = ui::VKEY_NEXT;
      } else if (tokens[i] == kVivaldiKeyMultiply) {
        key = ui::VKEY_MULTIPLY;
      } else if (tokens[i] == kVivaldiKeyDivide) {
        key = ui::VKEY_DIVIDE;
      } else if (tokens[i] == kVivaldiKeySubtract) {
        key = ui::VKEY_SUBTRACT;
      } else if (tokens[i].size() == 0) {
        // Nasty workaround. The parser does not handle "++"
        key = ui::VKEY_ADD;
      } else if (tokens[i].size() == 1 && std::toupper(tokens[i][0]) >= 'A' &&
                std::toupper(tokens[i][0]) <= 'Z') {
        key = static_cast<ui::KeyboardCode>(ui::VKEY_A + (std::toupper(tokens[i][0]) - 'A'));
      } else if (tokens[i].size() == 1 && tokens[i][0] >= '0' &&
                 tokens[i][0] <= '9') {
        key = static_cast<ui::KeyboardCode>(ui::VKEY_0 + (tokens[i][0] - '0'));
      } else if ((key = GetFunctionKey(tokens[i])) != ui::VKEY_UNKNOWN) {
      }
    }
  }
  if (key == ui::VKEY_UNKNOWN) {
    return ui::Accelerator();
  } else {
    return ui::Accelerator(key, modifiers);
  }
}

// Based on extensions/command.cc
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
ui::Accelerator ParseShortcut(const std::string& accelerator,
                              bool should_parse_media_keys) {
  std::vector<std::string> tokens = base::SplitString(
      accelerator, "+", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (tokens.size() == 0) {
    return ui::Accelerator();
  }

  int modifiers = ui::EF_NONE;
  ui::KeyboardCode key = ui::VKEY_UNKNOWN;
  for (size_t i = 0; i < tokens.size(); i++) {
    if (tokens[i] == ui::kKeyCtrl) {
      modifiers |= ui::EF_CONTROL_DOWN;
    } else if (tokens[i] == ui::kKeyAlt) {
      modifiers |= ui::EF_ALT_DOWN;
    } else if (tokens[i] == ui::kKeyShift) {
      modifiers |= ui::EF_SHIFT_DOWN;
    } else if (tokens[i] == ui::kKeyCommand) {
      modifiers |= ui::EF_COMMAND_DOWN;
    } else if (key == ui::VKEY_UNKNOWN) {
      if (tokens[i] == ui::kKeyUp) {
        key = ui::VKEY_UP;
      } else if (tokens[i] == ui::kKeyDown) {
        key = ui::VKEY_DOWN;
      } else if (tokens[i] == ui::kKeyLeft) {
        key = ui::VKEY_LEFT;
      } else if (tokens[i] == ui::kKeyRight) {
        key = ui::VKEY_RIGHT;
      } else if (tokens[i] == ui::kKeyIns) {
        key = ui::VKEY_INSERT;
      } else if (tokens[i] == ui::kKeyDel) {
        key = ui::VKEY_DELETE;
      } else if (tokens[i] == ui::kKeyHome) {
        key = ui::VKEY_HOME;
      } else if (tokens[i] == ui::kKeyEnd) {
        key = ui::VKEY_END;
      } else if (tokens[i] == ui::kKeyPgUp) {
        key = ui::VKEY_PRIOR;
      } else if (tokens[i] == ui::kKeyPgDwn) {
        key = ui::VKEY_NEXT;
      } else if (tokens[i] == ui::kKeySpace) {
        key = ui::VKEY_SPACE;
      } else if (tokens[i] == ui::kKeyTab) {
        key = ui::VKEY_TAB;
      } else if (tokens[i] == kVivaldiKeyPeriod) {
        key = ui::VKEY_OEM_PERIOD;
      } else if (tokens[i] == kVivaldiKeyComma) {
        key = ui::VKEY_OEM_COMMA;
      } else if (tokens[i] == kVivaldiKeyBackslash) {
        key = ui::VKEY_OEM_5;
      } else if (tokens[i] == kVivaldiKeyEsc) {
        key = ui::VKEY_ESCAPE;
      } else if (tokens[i] == kVivaldiKeyDel) {
        key = ui::VKEY_DELETE;
      } else if (tokens[i] == kVivaldiKeyIns) {
        key = ui::VKEY_ESCAPE;
      } else if (tokens[i] == kVivaldiKeyPgUp) {
        key = ui::VKEY_PRIOR;
      } else if (tokens[i] == kVivaldiKeyPgDn) {
        key = ui::VKEY_NEXT;
      } else if (tokens[i] == kVivaldiKeyMultiply) {
        key = ui::VKEY_MULTIPLY;
      } else if (tokens[i] == kVivaldiKeyDivide) {
        key = ui::VKEY_DIVIDE;
      } else if (tokens[i] == kVivaldiKeySubtract) {
        key = ui::VKEY_SUBTRACT;
      } else if (tokens[i].size() == 0) {
        // Nasty workaround. The parser does not handle "++"
        key = ui::VKEY_ADD;
      } else if (tokens[i] == ui::kKeyMediaNextTrack &&
                 should_parse_media_keys) {
        key = ui::VKEY_MEDIA_NEXT_TRACK;
      } else if (tokens[i] == ui::kKeyMediaPlayPause &&
                 should_parse_media_keys) {
        key = ui::VKEY_MEDIA_PLAY_PAUSE;
      } else if (tokens[i] == ui::kKeyMediaPrevTrack &&
                 should_parse_media_keys) {
        key = ui::VKEY_MEDIA_PREV_TRACK;
      } else if (tokens[i] == ui::kKeyMediaStop &&
                 should_parse_media_keys) {
        key = ui::VKEY_MEDIA_STOP;
      } else if (tokens[i].size() == 1 && tokens[i][0] >= 'A' &&
                 tokens[i][0] <= 'Z') {
        key = static_cast<ui::KeyboardCode>(ui::VKEY_A + (tokens[i][0] - 'A'));
      } else if (tokens[i].size() == 1 && tokens[i][0] >= '0' &&
                 tokens[i][0] <= '9') {
        key = static_cast<ui::KeyboardCode>(ui::VKEY_0 + (tokens[i][0] - '0'));
      } else if ((key = GetFunctionKey(tokens[i])) != ui::VKEY_UNKNOWN) {
      } else {
        // Keep this for now.
        // printf("unknown key length=%lu, string=%s\n", tokens[i].size(),
        // tokens[i].c_str()); for(int j=0; j<(int)tokens[i].size(); j++) {
        //  printf("%02x ", tokens[i][j]);
        //}
        // printf("\n");
      }
    }
  }
  if (key == ui::VKEY_UNKNOWN) {
    return ui::Accelerator();
  } else {
    return ui::Accelerator(key, modifiers);
  }
}

void BroadcastEvent(const std::string& eventname,
                    base::Value::List args,
                    content::BrowserContext* context) {
  if (!context)
    return;
  auto event = std::make_unique<extensions::Event>(
      extensions::events::VIVALDI_EXTENSION_EVENT, eventname, std::move(args));
  extensions::EventRouter* event_router = extensions::EventRouter::Get(context);
  if (event_router) {
    event_router->BroadcastEvent(std::move(event));
  }
}

void BroadcastEventToAllProfiles(const std::string& eventname,
                                 base::Value::List args_list) {
  std::vector<Profile*> active_profiles =
      VivaldiBrowserComponentWrapper::GetInstance()
          ->GetLoadedProfiles();
  for (size_t i = 0; i < active_profiles.size(); ++i) {
    Profile* profile = active_profiles[i];
    DCHECK(profile);
    base::Value::List args;
    if (i + 1 == active_profiles.size()) {
      args = std::move(args_list);
    } else {
      for (const auto& v : args_list)
        args.Append(v.Clone());
    }
    BroadcastEvent(eventname, std::move(args), profile);
  }
}

// Start chromium copied code
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

base::Time GetTime(double ms_from_epoch) {
  return (ms_from_epoch == 0)
             ? base::Time::UnixEpoch()
             : base::Time::FromMillisecondsSinceUnixEpoch(ms_from_epoch);
}

gfx::PointF FromUICoordinates(content::WebContents* web_contents,
                              const gfx::PointF& p) {
  // Account for the zoom factor in the UI.
  zoom::ZoomController* zoom_controller =
      web_contents ? zoom::ZoomController::FromWebContents(web_contents)
                   : nullptr;
  if (!zoom_controller)
    return p;
  double zoom_factor =
      blink::ZoomLevelToZoomFactor(zoom_controller->GetZoomLevel());
  return gfx::PointF(p.x() * zoom_factor, p.y() * zoom_factor);
}

void FromUICoordinates(content::WebContents* web_contents, gfx::RectF* rect) {
  // Account for the zoom factor in the UI.
  zoom::ZoomController* zoom_controller =
      web_contents ? zoom::ZoomController::FromWebContents(web_contents)
                   : nullptr;
  if (!zoom_controller)
    return;
  double zoom_factor =
      blink::ZoomLevelToZoomFactor(zoom_controller->GetZoomLevel());
  rect->Scale(zoom_factor);
}

gfx::PointF ToUICoordinates(content::WebContents* web_contents,
                            const gfx::PointF& p) {
  // Account for the zoom factor in the UI.
  zoom::ZoomController* zoom_controller =
      web_contents ? zoom::ZoomController::FromWebContents(web_contents)
                   : nullptr;
  if (!zoom_controller)
    return p;
  double zoom_factor =
      blink::ZoomLevelToZoomFactor(zoom_controller->GetZoomLevel());
  return gfx::PointF(p.x() / zoom_factor, p.y() / zoom_factor);
}

// VB-116765. The original KeyCodeToName was borrowed from chromium since it's a
// private function. It does translate strings to their native language however.
// We just want english.
std::string KeyCodeToName(ui::KeyboardCode key_code) {
  switch (key_code) {
    case ui::VKEY_TAB:
      return "Tab";
    case ui::VKEY_RETURN:
      return "Enter";
    case ui::VKEY_SPACE:
      return "Space";
    case ui::VKEY_PRIOR:
      return "PageUp";
    case ui::VKEY_NEXT:
      return "PageDown";
    case ui::VKEY_END:
      return "End";
    case ui::VKEY_HOME:
      return "Home";
    case ui::VKEY_INSERT:
      return "Insert";
    case ui::VKEY_DELETE:
      return "Delete";
    case ui::VKEY_LEFT:
      return "Left";
    case ui::VKEY_RIGHT:
      return "Right";
    case ui::VKEY_UP:
      return "Up";
    case ui::VKEY_DOWN:
      return "Down";
    case ui::VKEY_ESCAPE:
      return "Esc";
    case ui::VKEY_BACK:
      return "Backspace";
    case ui::VKEY_OEM_COMMA:
      return "Comma";
    case ui::VKEY_OEM_PERIOD:
      return "Period";
    case ui::VKEY_MEDIA_NEXT_TRACK:
      return "MediaNextTrack";
    case ui::VKEY_MEDIA_PLAY_PAUSE:
      return "MediaPlayPause";
    case ui::VKEY_MEDIA_PREV_TRACK:
      return "MediaPreviousTrack";
    case ui::VKEY_MEDIA_STOP:
      return "MediaStop";
    default:
      return "";
  }
}

std::string ShortcutTextFromEvent(const input::NativeWebKeyboardEvent& event) {
  return ::vivaldi::ShortcutText(
      event.windows_key_code,
      WebEventModifiersToEventFlags(event.GetModifiers()), event.dom_code);
}

std::string ShortcutText(int windows_key_code, int modifiers, int dom_code) {
  // We'd just use Accelerator::GetShortcutText to get the shortcut text but
  // it translates the modifiers when the system language is set to
  // non-English (since it's used for display). We can't match something
  // like 'Strg+G' however, so we do the modifiers manually.
  //
  // Since chromium 136, GetShortcutText() will also translate things like Tab
  // to the browser UI language, so it should really only be used on numbers and
  // letters. We let KeyCodeToName handle other cases.
  //
  // AcceleratorToString gets the shortcut text, but doesn't localize
  // like Accelerator::GetShortcutText() does, so it's suitable for us.
  // It doesn't handle all keys, however, and doesn't work with ctrl+alt
  // shortcuts so we're left with doing a little tweaking.
  ui::KeyboardCode key_code = static_cast<ui::KeyboardCode>(windows_key_code);
  ui::Accelerator accelerator =
      ui::Accelerator(key_code, 0, ui::Accelerator::KeyState::PRESSED);

  // This order should match the order in
  // normalizeShortcut(...) in KeyShortcut.js
  std::string shortcutText = "";
  if (modifiers & ui::EF_CONTROL_DOWN) {
    shortcutText += "Ctrl+";
  }
  if (modifiers & ui::EF_ALT_DOWN) {
    shortcutText += "Alt+";
  }
  if (modifiers & ui::EF_SHIFT_DOWN) {
    shortcutText += "Shift+";
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
    base::snprintf(buf, 8, "Numpad%d", windows_key_code - ui::VKEY_NUMPAD0);
    shortcutText += buf;

    // This is equivalent to js event.code and deals with a few
    // MacOS keyboard shortcuts like cmd+alt+n that fall through
    // in some languages, i.e. AcceleratorToString returns a blank.
    // Cmd+Alt shortcuts seem to be the only case where this fallback
    // is required.
#if BUILDFLAG(IS_MAC)
  } else if (modifiers & input::NativeWebKeyboardEvent::kAltKey &&
             modifiers & input::NativeWebKeyboardEvent::kMetaKey) {
    shortcutText +=
        ui::DomCodeToUsLayoutCharacter(static_cast<ui::DomCode>(dom_code), 0);
#endif  // OS_MAC
  } else {
    // With chrome 67 accelerator.GetShortcutText() will return Mac specific
    // symbols (like '⎋' for escape). All is private so we bypass that by
    // testing with KeyCodeToName first.
    std::string shortcut = KeyCodeToName(key_code);
    if (shortcut.empty()) {
      shortcut = base::UTF16ToUTF8(accelerator.GetShortcutText());
    }
    shortcutText += shortcut;
  }
  return shortcutText;
}

/*
Structure is as follows:

"profile_image_path": {
  { "profile_path": "<path 1>", "image_path": "<image path>" },
  { "profile_path": "<path 2>", "image_path": "<image path>" },
  ...
}
*/

const char kProfilePathKey[] = "profile_path";
const char kImagePathKey[] = "image_path";

std::string GetImagePathFromProfilePath(const std::string& preferences_path,
                                        const std::string& profile_path) {
  std::string path;
  PrefService* prefs = g_browser_process->local_state();
  const base::Value& list = prefs->GetValue(preferences_path);

  if (list.is_list()) {
    for (auto& item : list.GetList()) {
      if (item.is_dict()) {
        const std::string* value = item.GetDict().FindString(kProfilePathKey);
        if (value && *value == profile_path) {
          const std::string* image_path =
              item.GetDict().FindString(kImagePathKey);
          if (image_path) {
            return *image_path;
          }
        }
      }
    }
  }
  return path;
}

void SetImagePathForProfilePath(const std::string& preferences_path,
                                const std::string& avatar_path,
                                const std::string& profile_path) {
  std::string path;
  bool updated = false;
  PrefService* prefs = g_browser_process->local_state();
  ScopedListPrefUpdate update(prefs, preferences_path);
  auto& update_pref_data = update.Get();

  std::string image_path;
  for (auto& item : update_pref_data) {
    if (item.is_dict()) {
      const std::string* value = item.GetDict().FindString(kProfilePathKey);

      // If it exists already, update it
      if (value && *value == profile_path) {
        if (avatar_path.size()) {
          item.GetDict().Set(kImagePathKey, avatar_path);
        } else {
          // empty path means remove the dictionary.
          item.GetDict().Remove(kImagePathKey);
        }
        updated = true;
        break;
      }
    }
  }
  if (!updated) {
    base::Value dict(base::Value::Type::DICT);
    dict.GetDict().Set(kProfilePathKey, base::Value(profile_path));
    dict.GetDict().Set(kImagePathKey, base::Value(avatar_path));
    update_pref_data.Append(std::move(dict));
  }
}

void RestartBrowser() {
  // Free any open devtools if the user selects Exit from the menu.
  VivaldiBrowserComponentWrapper::GetInstance()->CloseAllDevtools();

  LOG(INFO) << "Restarting Vivaldi";
  VivaldiBrowserComponentWrapper::GetInstance()->AttemptRestart();
}

}  // namespace vivaldi

namespace extensions {

Profile* GetFunctionCallerProfile(ExtensionFunction& fun) {
  ExtensionFunctionDispatcher* dispatcher = fun.dispatcher();
  if (!dispatcher)
    return nullptr;
  content::BrowserContext* browser_context = dispatcher->browser_context();
  if (!browser_context)
    return nullptr;
  return Profile::FromBrowserContext(browser_context);
}

}  // namespace extensions
