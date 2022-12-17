// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "browser/vivaldi_child_process_flags.h"

#include "base/command_line.h"
#include "base/cxx17_backports.h"
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

  if (vivaldi::IsDebuggingVivaldi()) {
    vivaldi::CommandLineAppendSwitchNoDup(&command_line,
                                          switches::kDebugVivaldi);
  }
}

}  // namespace

void VivaldiAddRendererProcessFlags(content::BrowserContext* browser_context,
                                    base::CommandLine& renderer_command_line) {
  AddBaseSwitches(renderer_command_line);

#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
  static const char* const kSwitchesToCopy[] = {
      switches::kVivaldiEnableIPCDemuxer,
      switches::kVivaldiOldPlatformAudio,
  };
  renderer_command_line.CopySwitchesFrom(
      *base::CommandLine::ForCurrentProcess(), kSwitchesToCopy,
      std::size(kSwitchesToCopy));
#endif
}

void VivaldiAddGpuProcessFlags(base::CommandLine& gpu_command_line) {
  AddBaseSwitches(gpu_command_line);

#if BUILDFLAG(IS_MAC)
  static const char* const kSwitchesToCopy[] = {
      switches::kVivaldiPlatformMedia,
  };
  gpu_command_line.CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                                    kSwitchesToCopy,
                                    std::size(kSwitchesToCopy));
#endif
}

void VivaldiAddUtilityProcessFlags(base::CommandLine& utility_command_line) {
  AddBaseSwitches(utility_command_line);

#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
  static const char* const kSwitchesToCopy[] = {
      switches::kVivaldiOldPlatformAudio,
  };
  utility_command_line.CopySwitchesFrom(
      *base::CommandLine::ForCurrentProcess(), kSwitchesToCopy,
      std::size(kSwitchesToCopy));
#endif
}
