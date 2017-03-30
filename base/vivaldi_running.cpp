// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "app/vivaldi_apptools.h"

#include "base/command_line.h"
#include "base/vivaldi_switches.h"

namespace vivaldi {

namespace {
bool g_checked_vivaldi_status = false;
bool g_vivaldi_is_running = false;
bool g_debugging_vivaldi = false;

void CheckVivaldiStatus() {
  if (g_checked_vivaldi_status)
    return;
  g_checked_vivaldi_status = true;
  base::CommandLine *cmd_line = base::CommandLine::ForCurrentProcess();
  DCHECK(cmd_line);
  g_vivaldi_is_running = IsVivaldiRunning(*cmd_line);
  g_debugging_vivaldi = IsDebuggingVivaldi(*cmd_line);
}
}

bool IsVivaldiRunning(const base::CommandLine &cmd_line) {
  if (cmd_line.HasSwitch(switches::kDisableVivaldi) ||
    cmd_line.HasSwitch("app")) // so we don't load the Vivaldi app
    return false;
  return true;
}

bool IsDebuggingVivaldi(const base::CommandLine &cmd_line) {
  return cmd_line.HasSwitch(switches::kDebugVivaldi);
}

bool IsVivaldiRunning(){
  CheckVivaldiStatus();
  return g_vivaldi_is_running;
}

bool IsDebuggingVivaldi(){
  CheckVivaldiStatus();
  return g_debugging_vivaldi;
}

} // namespace vivaldi
