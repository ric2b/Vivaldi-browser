// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef BROWSER_INIT_SPARKLE_H_
#define BROWSER_INIT_SPARKLE_H_

#include "url/gurl.h"

namespace init_sparkle {

GURL GetAppcastUrl();

#if defined(OS_MAC)

void Initialize();

#endif
}  // namespace init_sparkle

#endif  // BROWSER_INIT_SPARKLE_H_
