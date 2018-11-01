// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef APP_VIVALDI_APPTOOLS_H_
#define APP_VIVALDI_APPTOOLS_H_

#include <set>
#include <string>
#include "base/base_export.h"

namespace base {
class CommandLine;
}
class GURL;

namespace vivaldi {

bool IsVivaldiApp(const std::string& extension_id);

const std::set<std::string>& GetVivaldiExtraLocales();
bool IsVivaldiExtraLocale(const std::string& locale);

bool BASE_EXPORT IsVivaldiRunning();
bool BASE_EXPORT IsDebuggingVivaldi();
bool BASE_EXPORT IsVivaldiRunning(const base::CommandLine& cmd_line);
bool BASE_EXPORT IsDebuggingVivaldi(const base::CommandLine& cmd_line);
void BASE_EXPORT ForceVivaldiRunning(bool status);
bool BASE_EXPORT ForcedVivaldiRunning();

bool BASE_EXPORT IsTabDragInProgress();
void BASE_EXPORT SetTabDragInProgress(bool tab_drag_in_progress);

void BASE_EXPORT CommandLineAppendSwitchNoDup(base::CommandLine* const cmd_line,
                                              const std::string& switch_string);
inline void
CommandLineAppendSwitchNoDup(base::CommandLine& cmd_line,
                             const std::string& switch_string) {
  CommandLineAppendSwitchNoDup(&cmd_line, switch_string);
};

std::string VivaldiLastChange();

GURL GetVivaldiNewTabURL();

// Returns a version string to be displayed in "About Vivaldi" dialog.
std::string GetVivaldiVersionString();

}  // namespace vivaldi

#endif  // APP_VIVALDI_APPTOOLS_H_
