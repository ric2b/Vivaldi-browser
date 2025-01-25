// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_TOOLS_VIVALDI_TOOLS_H_
#define EXTENSIONS_TOOLS_VIVALDI_TOOLS_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "components/input/native_web_keyboard_event.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/gfx/geometry/rect_f.h"

class Browser;
class ExtensionFunction;
class Profile;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace base {
class Time;
}  // namespace base

namespace ui {
class Accelerator;
}

namespace vivaldi {

// Helper functions for menus
ui::KeyboardCode GetFunctionKey(std::string token);
ui::Accelerator ParseShortcut(const std::string& accelerator,
                              bool should_parse_media_keys);

// Find first available Vivaldi browser.
Browser* FindVivaldiBrowser();

void BroadcastEvent(const std::string& eventname,
                    base::Value::List args,
                    content::BrowserContext* context);

void BroadcastEventToAllProfiles(const std::string& eventname,
                                 base::Value::List args = {});

// Return time from milliseconds
base::Time GetTime(double ms_from_epoch);

gfx::PointF FromUICoordinates(content::WebContents* web_contents,
                              const gfx::PointF& p);

void FromUICoordinates(content::WebContents* web_contents, gfx::RectF* rect);

gfx::PointF ToUICoordinates(content::WebContents* web_contents,
                            const gfx::PointF& p);

std::u16string KeyCodeToName(ui::KeyboardCode key_code);

std::string ShortcutTextFromEvent(const input::NativeWebKeyboardEvent& event);

std::string ShortcutText(int windows_key_code, int modifiers, int dom_code);

std::string GetImagePathFromProfilePath(const std::string& preferences_path,
                                        const std::string& profile_path);
void SetImagePathForProfilePath(const std::string& preferences_path,
                                const std::string& avatar_path,
                                const std::string& profile_path);

void RestartBrowser();

}  // namespace vivaldi

namespace extensions {

// Get a profile instance for the script that called the extension function.
// Return null if the profile no longer exists. Use this function instead of
// ExtensionFunction::browser_context() in asynchronous callbacks called after
// ExtensionFunction::Run() returned as at that point browser_context() may
// return a pointer to deleted memory. This happens if the function was called
// from a private window and the user closed that window before the callback was
// called.
//
// TODO(igor@vivaldi.com): After Chromium resolves crbug.com/1200440 consider if
// this will still be necessary.
Profile* GetFunctionCallerProfile(ExtensionFunction& fun);

}  // namespace extensions

#endif  // EXTENSIONS_TOOLS_VIVALDI_TOOLS_H_
