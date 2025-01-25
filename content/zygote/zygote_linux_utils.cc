// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include "chromium/content/zygote/zygote_linux.h"
#include "chromium/sandbox/policy/linux/sandbox_linux.h"

namespace content {

bool Zygote::UsingFlatpakSandbox() const {
  return sandbox_flags_ & sandbox::policy::SandboxLinux::kFlatpak;
}

} // namespace content
