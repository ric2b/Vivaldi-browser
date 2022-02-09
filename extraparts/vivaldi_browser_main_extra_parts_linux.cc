// Copyright (c) 2015-2018 Vivaldi Technologies AS. All rights reserved

#include "extraparts/vivaldi_browser_main_extra_parts_linux.h"

VivaldiBrowserMainExtraPartsLinux::VivaldiBrowserMainExtraPartsLinux() {}

VivaldiBrowserMainExtraPartsLinux::~VivaldiBrowserMainExtraPartsLinux() {}

std::unique_ptr<VivaldiBrowserMainExtraParts>
VivaldiBrowserMainExtraParts::Create() {
  return std::make_unique<VivaldiBrowserMainExtraPartsLinux>();
}