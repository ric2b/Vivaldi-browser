// Copyright (c) 2015-2018 Vivaldi Technologies AS. All rights reserved

#include "extraparts/vivaldi_browser_main_extra_parts_linux.h"

VivaldiBrowserMainExtraPartsLinux::VivaldiBrowserMainExtraPartsLinux() {}

VivaldiBrowserMainExtraPartsLinux::~VivaldiBrowserMainExtraPartsLinux() {}

VivaldiBrowserMainExtraParts* VivaldiBrowserMainExtraParts::Create() {
  return new VivaldiBrowserMainExtraPartsLinux();
}