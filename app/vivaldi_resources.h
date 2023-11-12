// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef APP_VIVALDI_RESOURCES_H_
#define APP_VIVALDI_RESOURCES_H_

#include "build/build_config.h"

#include "vivaldi/grit/vivaldi_native_resources.h"
#include "vivaldi/app/grit/vivaldi_native_strings.h"

#if !BUILDFLAG(IS_IOS)
#include "vivaldi/grit/vivaldi_native_unscaled.h"
#endif

#endif  // APP_VIVALDI_RESOURCES_H_
