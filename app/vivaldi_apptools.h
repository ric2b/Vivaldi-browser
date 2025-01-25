// Copyright (c) 2015-2021 Vivaldi Technologies AS. All rights reserved

#ifndef APP_VIVALDI_APPTOOLS_H_
#define APP_VIVALDI_APPTOOLS_H_

#include <string>
#include <string_view>

#include "base/base_export.h"
#include "base/callback_list.h"

namespace base {
class CommandLine;
}
namespace content {
class WebContents;
}
class GURL;

namespace vivaldi {

// Implementation is in vivaldi_running.cc
BASE_EXPORT base::CallbackListSubscription AddExtDataUpdatedCallback(
    base::RepeatingCallback<void(content::WebContents*)>);
BASE_EXPORT base::RepeatingCallbackList<void(content::WebContents*)>&
GetExtDataUpdatedCallbackList();
BASE_EXPORT base::RepeatingCallbackList<void()>& GetSystemColorsUpdatedCallbackList();
BASE_EXPORT base::CallbackListSubscription SystemColorsUpdatedCallback(
    base::RepeatingCallback<void()> system_colors_callback);


bool BASE_EXPORT IsVivaldiApp(std::string_view extension_id);
bool BASE_EXPORT IsVivaldiUrl(const std::string& url_str);

bool BASE_EXPORT IsVivaldiExtraLocale(std::string_view locale);

bool BASE_EXPORT IsVivaldiRunning();
bool BASE_EXPORT IsVivaldiRunning(const base::CommandLine& cmd_line);
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
}

GURL GetVivaldiNewTabURL();

// Windows-specific flag to inform UI about cancelled drag.
extern bool BASE_EXPORT g_cancelled_drag;

}  // namespace vivaldi

#endif  // APP_VIVALDI_APPTOOLS_H_
