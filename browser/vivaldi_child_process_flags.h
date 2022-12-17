// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#ifndef BROWSER_VIVALDI_CHILD_PROCESS_FLAGS_H_
#define BROWSER_VIVALDI_CHILD_PROCESS_FLAGS_H_

namespace base {
class CommandLine;
}

namespace content {
class BrowserContext;
}

void VivaldiAddRendererProcessFlags(content::BrowserContext* browser_context,
                                    base::CommandLine& renderer_command_line);

void VivaldiAddGpuProcessFlags(base::CommandLine& gpu_command_line);

void VivaldiAddUtilityProcessFlags(base::CommandLine& utility_command_line);

#endif  // BROWSER_VIVALDI_CHILD_PROCESS_FLAGS_H_
