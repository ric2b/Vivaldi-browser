// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_TOOLS_VIVALDI_TOOLS_H_
#define EXTENSIONS_TOOLS_VIVALDI_TOOLS_H_

#include <memory>
#include <string>

#include "base/strings/string16.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "third_party/blink/public/platform/web_float_point.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

class Browser;

namespace content {
class BrowserContext;
class WebContents;
}

namespace base {
class ListValue;
class Time;
}

namespace ui {
class Accelerator;
}

namespace vivaldi {

// Helper functions for menus
ui::KeyboardCode GetFunctionKey(std::string token);
ui::Accelerator ParseShortcut(const std::string& accelerator,
                              bool should_parse_media_keys);

// Helper class for setting and resetting a bookmark upgrade flag.
class SetPartnerUpgrade {
 public:
  SetPartnerUpgrade(content::BrowserContext* context, bool active);
  ~SetPartnerUpgrade();
 private:
  void Set(bool active);
  content::BrowserContext* context_;
};

// Find first available Vivaldi browser.
Browser* FindVivaldiBrowser();

void BroadcastEvent(const std::string& eventname,
                    std::unique_ptr<base::ListValue> args,
                    content::BrowserContext* context);

// Return number of milliseconds for time
double MilliSecondsFromTime(const base::Time& time);

// Return time from milliseconds
base::Time GetTime(double ms_from_epoch);

blink::WebFloatPoint FromUICoordinates(content::WebContents* web_contents,
                                       blink::WebFloatPoint p);

blink::WebFloatPoint ToUICoordinates(content::WebContents* web_contents,
                                     blink::WebFloatPoint p);

base::string16 KeyCodeToName(ui::KeyboardCode key_code);

std::string ShortcutTextFromEvent(const content::NativeWebKeyboardEvent& event);

std::string ShortcutText(int windows_key_code, int modifiers, int dom_code);

}  // namespace vivaldi

#endif  // EXTENSIONS_TOOLS_VIVALDI_TOOLS_H_
