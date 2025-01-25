// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved.

#ifndef CONTENT_BROWSER_ZYGOTE_HOST_VIVALDI_ZYGOTE_HOST_LINUX_UTILS_H_
#define CONTENT_BROWSER_ZYGOTE_HOST_VIVALDI_ZYGOTE_HOST_LINUX_UTILS_H_

#include "sandbox/linux/services/flatpak_sandbox.h"

#include "base/process/process.h"

namespace base {
struct LaunchOptions;
}  // namespace base

namespace vivaldi {

base::Process LaunchFlatpakZygote(const base::CommandLine &cmd_line,
                                  base::LaunchOptions* options);

}  // namespace vivaldi

#endif  // CONTENT_BROWSER_ZYGOTE_HOST_VIVALDI_ZYGOTE_HOST_LINUX_UTILS_H_
