// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "browser/vivaldi_child_process_flags.h"

#include "base/command_line.h"
#include "build/build_config.h"

#include "app/vivaldi_apptools.h"
#include "base/vivaldi_switches.h"

namespace {

void AddBaseSwitches(base::CommandLine& command_line) {
  if (vivaldi::IsVivaldiRunning()) {
    vivaldi::CommandLineAppendSwitchNoDup(&command_line,
                                          switches::kRunningVivaldi);
  } else {
    vivaldi::CommandLineAppendSwitchNoDup(&command_line,
                                          switches::kDisableVivaldi);
  }
}

}  // namespace

void VivaldiAddRendererProcessFlags(content::BrowserContext* browser_context,
                                    base::CommandLine& renderer_command_line) {
  AddBaseSwitches(renderer_command_line);
}

void VivaldiAddGpuProcessFlags(base::CommandLine& gpu_command_line) {
  AddBaseSwitches(gpu_command_line);
}

void VivaldiAddUtilityProcessFlags(base::CommandLine& utility_command_line) {
  AddBaseSwitches(utility_command_line);
}
