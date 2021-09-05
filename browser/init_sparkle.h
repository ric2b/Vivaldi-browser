// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef BROWSER_INIT_SPARKLE_H_
#define BROWSER_INIT_SPARKLE_H_

#include "url/gurl.h"

namespace base {
class CommandLine;
}

namespace init_sparkle {

struct Config {
  GURL appcast_url;
  bool with_custom_url = false;
};

Config GetConfig(const base::CommandLine& command_line);

#if defined(OS_MAC)

void Initialize(const base::CommandLine& command_line);

#endif
}  // namespace vivaldi

#endif  // BROWSER_INIT_SPARKLE_H_
