// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#include "extraparts/vivaldi_browser_main_extra_parts_android.h"

#include "browser/vivaldi_default_bookmarks_reader.h"
#include "chrome/browser/first_run/first_run.h"

using vivaldi::first_run::VivaldiDefaultBookmarksReader;

VivaldiBrowserMainExtraPartsAndroid::VivaldiBrowserMainExtraPartsAndroid() {}

VivaldiBrowserMainExtraPartsAndroid::~VivaldiBrowserMainExtraPartsAndroid() {}

void VivaldiBrowserMainExtraPartsAndroid::PostProfileInit() {
  if (first_run::IsChromeFirstRun())
    VivaldiDefaultBookmarksReader::GetInstance()->ReadBookmarks();
}

VivaldiBrowserMainExtraParts* VivaldiBrowserMainExtraParts::Create() {
  return new VivaldiBrowserMainExtraPartsAndroid();
}
