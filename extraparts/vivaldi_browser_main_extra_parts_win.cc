// Copyright (c) 2015-2018 Vivaldi Technologies AS. All rights reserved

#include "extraparts/vivaldi_browser_main_extra_parts_win.h"

#include <shlobj.h>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/win/registry.h"
#include "browser/win/vivaldi_utils.h"
#include "chrome/common/chrome_switches.h"

#define GOOGLE_CHROME_BASE_DIRECTORY L"Google\\Chrome\\Application"

VivaldiBrowserMainExtraPartsWin::VivaldiBrowserMainExtraPartsWin() {}

VivaldiBrowserMainExtraPartsWin::~VivaldiBrowserMainExtraPartsWin() {}

std::unique_ptr<VivaldiBrowserMainExtraParts>
VivaldiBrowserMainExtraParts::Create() {
  return std::make_unique<VivaldiBrowserMainExtraPartsWin>();
}

// Overridden from ChromeBrowserMainExtraParts:
void VivaldiBrowserMainExtraPartsWin::PostEarlyInitialization() {
  VivaldiBrowserMainExtraParts::PostEarlyInitialization();
}
