// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extraparts/vivaldi_main_delegate.h"

#include "extensions/vivaldi_extensions_client.h"

VivaldiMainDelegate::VivaldiMainDelegate()
 : VivaldiMainDelegate(base::TimeTicks()) {}

VivaldiMainDelegate::VivaldiMainDelegate(base::TimeTicks exe_entry_point_ticks)
 : ChromeMainDelegate(exe_entry_point_ticks) {
  extensions::VivaldiExtensionsClient::RegisterVivaldiExtensionsClient();
}

VivaldiMainDelegate::~VivaldiMainDelegate() {}
