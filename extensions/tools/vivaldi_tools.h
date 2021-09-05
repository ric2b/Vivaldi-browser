// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_TOOLS_VIVALDI_TOOLS_H_
#define EXTENSIONS_TOOLS_VIVALDI_TOOLS_H_

#include <memory>
#include <string>

#include "base/strings/string16.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/gfx/geometry/rect_f.h"

class Browser;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace base {
class ListValue;
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
                    std::unique_ptr<base::ListValue> args,
                    content::BrowserContext* context);

void BroadcastEventToAllProfiles(
    const std::string& eventname,
    std::unique_ptr<base::ListValue> args = nullptr);

// Return number of milliseconds for time
double MilliSecondsFromTime(const base::Time& time);

// Return time from milliseconds
base::Time GetTime(double ms_from_epoch);

gfx::PointF FromUICoordinates(content::WebContents* web_contents,
                              const gfx::PointF& p);

void FromUICoordinates(content::WebContents* web_contents, gfx::RectF* rect);

gfx::PointF ToUICoordinates(content::WebContents* web_contents,
                            const gfx::PointF& p);

base::string16 KeyCodeToName(ui::KeyboardCode key_code);

std::string ShortcutTextFromEvent(const content::NativeWebKeyboardEvent& event);

std::string ShortcutText(int windows_key_code, int modifiers, int dom_code);

std::string GetImagePathFromProfilePath(const std::string& preferences_path,
                                        const std::string& profile_path);
void SetImagePathForProfilePath(const std::string& preferences_path,
                                const std::string& avatar_path,
                                const std::string& profile_path);

}  // namespace vivaldi

#endif  // EXTENSIONS_TOOLS_VIVALDI_TOOLS_H_
