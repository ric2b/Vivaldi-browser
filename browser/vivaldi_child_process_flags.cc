// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "browser/vivaldi_child_process_flags.h"

#include "base/command_line.h"
#include "base/cxx17_backports.h"
#include "build/build_config.h"

#include "app/vivaldi_apptools.h"
#include "base/vivaldi_switches.h"
#include "browser/vivaldi_runtime_feature.h"

namespace {

void AddBaseSwitches(base::CommandLine& command_line) {
  // TODO(igor@vivaldi.com): Consider passing --disable-vivaldi to the GPU
  // process and move the corresponding code here.
  if (vivaldi::IsDebuggingVivaldi()) {
    vivaldi::CommandLineAppendSwitchNoDup(&command_line,
                                          switches::kDebugVivaldi);
  }
}

void CopyBrowserSwitches(const char* const switches[],
                         size_t count,
                         base::CommandLine& command_line) {
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  command_line.CopySwitchesFrom(browser_command_line, switches, count);
}

}  // namespace

void VivaldiAddRendererProcessFlags(content::BrowserContext* browser_context,
                                    base::CommandLine& renderer_command_line) {
  if (vivaldi::IsVivaldiRunning())
    vivaldi::CommandLineAppendSwitchNoDup(&renderer_command_line,
                                          switches::kRunningVivaldi);
  else
    vivaldi::CommandLineAppendSwitchNoDup(&renderer_command_line,
                                          switches::kDisableVivaldi);

  AddBaseSwitches(renderer_command_line);

#if !BUILDFLAG(IS_ANDROID)
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  if (browser_command_line.HasSwitch(switches::kVivaldiDisableIPCDemuxer) ||
      vivaldi_runtime_feature::IsEnabled(browser_context,
                                         "disable_ipc_demuxer")) {
    renderer_command_line.AppendSwitch(switches::kVivaldiDisableIPCDemuxer);
  }
#endif
}

void VivaldiAddGpuProcessFlags(base::CommandLine& gpu_command_line) {
  AddBaseSwitches(gpu_command_line);

  static const char* const kSwitchesToCopy[] = {
      switches::kVivaldiPlatformMedia,
  };
  CopyBrowserSwitches(kSwitchesToCopy, std::size(kSwitchesToCopy),
                      gpu_command_line);
}
