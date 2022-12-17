// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "app/vivaldi_apptools.h"

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/vivaldi_switches.h"
#include "build/build_config.h"

namespace vivaldi {

#if BUILDFLAG(IS_WIN)
bool g_cancelled_drag = false;
#endif

namespace {
bool g_checked_vivaldi_status = false;
bool g_vivaldi_is_running = false;
bool g_forced_vivaldi_status = false;
bool g_tab_drag_in_progress = false;

bool TestIsVivaldiRunning(const base::CommandLine& cmd_line) {
  if (cmd_line.HasSwitch(switches::kDisableVivaldi)) {
    return false;
  }
  return true;
}

void CheckVivaldiStatus() {
  if (g_checked_vivaldi_status)
    return;
  if (!base::CommandLine::InitializedForCurrentProcess())
    return;
  g_checked_vivaldi_status = true;
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  DCHECK(cmd_line);
  g_vivaldi_is_running = cmd_line != NULL && TestIsVivaldiRunning(*cmd_line);
}

}  // namespace

void ForceVivaldiRunning(bool status) {
  // g_vivaldi_is_running = status;
  g_checked_vivaldi_status = true;
  g_forced_vivaldi_status = status;
}

bool ForcedVivaldiRunning() {
  return g_forced_vivaldi_status;
}

bool IsVivaldiRunning(const base::CommandLine& cmd_line) {
  if (!TestIsVivaldiRunning(cmd_line))
    return false;
  // We need to check the global command line, too
  return IsVivaldiRunning();
}

bool IsVivaldiRunning() {
  CheckVivaldiStatus();
  return g_vivaldi_is_running;
}

bool IsTabDragInProgress() {
  return g_tab_drag_in_progress;
}

void SetTabDragInProgress(bool tab_drag_in_progress) {
  g_tab_drag_in_progress = tab_drag_in_progress;
}

void CommandLineAppendSwitchNoDup(base::CommandLine* const cmd_line,
                                  const std::string& switch_string) {
  if (!cmd_line->HasSwitch(switch_string))
    cmd_line->AppendSwitchNative(switch_string,
                                 base::CommandLine::StringType());
}

base::RepeatingCallbackList<void(content::WebContents*)>&
GetExtDataUpdatedCallbackList() {
  static base::NoDestructor<
      base::RepeatingCallbackList<void(content::WebContents*)>>
      callback_list;
  return *callback_list;
}

base::CallbackListSubscription AddExtDataUpdatedCallback(
    base::RepeatingCallback<void(content::WebContents*)>
        tab_updated_extdata_callback) {
  return GetExtDataUpdatedCallbackList().Add(
      std::move(tab_updated_extdata_callback));
}

base::RepeatingCallbackList<void()>& GetSystemColorsUpdatedCallbackList() {
  static base::NoDestructor<base::RepeatingCallbackList<void()>> callback_list;
  return *callback_list;
}

base::CallbackListSubscription SystemColorsUpdatedCallback(
    base::RepeatingCallback<void()> system_colors_callback) {
  return GetSystemColorsUpdatedCallbackList().Add(
      std::move(system_colors_callback));
}

}  // namespace vivaldi
