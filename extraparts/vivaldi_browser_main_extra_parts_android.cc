// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#include "extraparts/vivaldi_browser_main_extra_parts_android.h"

#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile_manager.h"

#include "browser/vivaldi_default_bookmarks.h"

VivaldiBrowserMainExtraPartsAndroid::VivaldiBrowserMainExtraPartsAndroid() =
    default;

VivaldiBrowserMainExtraPartsAndroid::~VivaldiBrowserMainExtraPartsAndroid() =
    default;

void VivaldiBrowserMainExtraPartsAndroid::PostProfileInit() {
  VivaldiBrowserMainExtraParts::PostProfileInit();

  if (first_run::IsChromeFirstRun()) {
    Profile* profile = ProfileManager::GetLastUsedProfile();
    DCHECK(profile);
    vivaldi_default_bookmarks::UpdatePartners(profile);
  }
}

std::unique_ptr<VivaldiBrowserMainExtraParts>
VivaldiBrowserMainExtraParts::Create() {
  return std::make_unique<VivaldiBrowserMainExtraPartsAndroid>();
}
