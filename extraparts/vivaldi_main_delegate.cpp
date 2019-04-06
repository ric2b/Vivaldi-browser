// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extraparts/vivaldi_main_delegate.h"

#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/vivaldi_extensions_client.h"
#endif

VivaldiMainDelegate::VivaldiMainDelegate()
    : VivaldiMainDelegate(base::TimeTicks()) {}

VivaldiMainDelegate::VivaldiMainDelegate(base::TimeTicks exe_entry_point_ticks)
    : ChromeMainDelegate(exe_entry_point_ticks) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::VivaldiExtensionsClient::RegisterVivaldiExtensionsClient();
#endif
}

VivaldiMainDelegate::~VivaldiMainDelegate() {}
