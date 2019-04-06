// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "app/vivaldi_apptools.h"

#include "base/command_line.h"
#include "base/vivaldi_switches.h"

namespace vivaldi {

namespace {
bool g_checked_vivaldi_status = false;
bool g_vivaldi_is_running = false;
bool g_debugging_vivaldi = false;
bool g_forced_vivaldi_status = false;
bool g_tab_drag_in_progress = false;
bool g_block_next_context_menu = false;

void CheckVivaldiStatus() {
  if (g_checked_vivaldi_status)
    return;
  g_checked_vivaldi_status = true;
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  DCHECK(cmd_line);
  g_vivaldi_is_running = IsVivaldiRunning(*cmd_line);
  g_debugging_vivaldi = IsDebuggingVivaldi(*cmd_line);
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
  if (cmd_line.HasSwitch(switches::kDisableVivaldi) ||
      cmd_line.HasSwitch("app"))  // so we don't load the Vivaldi app
    return false;
  return true;
}

bool IsDebuggingVivaldi(const base::CommandLine& cmd_line) {
  return cmd_line.HasSwitch(switches::kDebugVivaldi);
}

bool IsVivaldiRunning() {
  CheckVivaldiStatus();
  return g_vivaldi_is_running;
}

bool IsDebuggingVivaldi() {
  CheckVivaldiStatus();
  return g_debugging_vivaldi;
}

bool IsTabDragInProgress() {
  return g_tab_drag_in_progress;
}

void SetTabDragInProgress(bool tab_drag_in_progress) {
  g_tab_drag_in_progress = tab_drag_in_progress;
}

bool GetBlockNextContextMenu() {
  return g_block_next_context_menu;
}

void SetBlockNextContextMenu(bool block_next_context_menu) {
  g_block_next_context_menu = block_next_context_menu;
}

void CommandLineAppendSwitchNoDup(base::CommandLine* const cmd_line,
                                  const std::string& switch_string) {
  if (!cmd_line->HasSwitch(switch_string))
    cmd_line->AppendSwitchNative(switch_string,
                                 base::CommandLine::StringType());
}

}  // namespace vivaldi
