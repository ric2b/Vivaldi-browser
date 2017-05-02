// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef BROWSER_INIT_SPARKLE_H_
#define BROWSER_INIT_SPARKLE_H_

#include "base/callback.h"

namespace base {
class CommandLine;
}

namespace vivaldi {
void InitializeSparkle(const base::CommandLine& command_line,
                       base::Callback<bool()> should_check_update_callback);
}  // namespace vivaldi

#endif  // BROWSER_INIT_SPARKLE_H_
