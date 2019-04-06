// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_TOOLS_VIVALDI_TOOLS_H_
#define EXTENSIONS_TOOLS_VIVALDI_TOOLS_H_

#include <memory>
#include <string>

class Browser;

namespace content {
class BrowserContext;
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

}  // namespace vivaldi

#endif  // EXTENSIONS_TOOLS_VIVALDI_TOOLS_H_
