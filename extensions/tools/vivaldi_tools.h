// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_TOOLS_VIVALDI_TOOLS_H_
#define EXTENSIONS_TOOLS_VIVALDI_TOOLS_H_

#include <memory>
#include <string>

#include "third_party/blink/public/platform/web_float_point.h"

class Browser;

namespace content {
class BrowserContext;
class WebContents;
}

namespace base {
class ListValue;
class Time;
}

namespace vivaldi {

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

}  // namespace vivaldi

#endif  // EXTENSIONS_TOOLS_VIVALDI_TOOLS_H_
