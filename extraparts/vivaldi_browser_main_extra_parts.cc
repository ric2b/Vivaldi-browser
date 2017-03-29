// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extraparts/vivaldi_browser_main_extra_parts.h"

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/translate/core/common/translate_switches.h"
#include "content/public/common/content_switches.h"

VivaldiBrowserMainExtraParts::VivaldiBrowserMainExtraParts() {
}

VivaldiBrowserMainExtraParts::~VivaldiBrowserMainExtraParts() {
}

// Overridden from ChromeBrowserMainExtraParts:
void VivaldiBrowserMainExtraParts::PostEarlyInitialization() {
  // andre@vivaldi.com HACK while not having all the permission dialogs in
  // place.
  base::CommandLine::ForCurrentProcess()->AppendSwitchNoDup(
      switches::kAlwaysAuthorizePlugins);

  // testing NPAPI, webview changes must be kept up-to-date
  base::CommandLine::ForCurrentProcess()->AppendSwitchNoDup("enable-npapi");

  base::CommandLine::ForCurrentProcess()->AppendSwitchNoDup(
      translate::switches::kDisableTranslate);


#if !defined(OFFICIAL_BUILD)
  // The mail client uses https://w3c.github.io/IndexedDB/#dom-idbindex-getall
  base::CommandLine::ForCurrentProcess()->AppendSwitchNoDup(
      switches::kEnableExperimentalWebPlatformFeatures);
#endif

#if defined(OS_MACOSX)
  PostEarlyInitializationMac();
#endif
#if defined(OS_WIN)
  PostEarlyInitializationWin();
#endif
#if defined(OS_LINUX)
  PostEarlyInitializationLinux();
#endif
}

void VivaldiBrowserMainExtraParts::PostProfileInit() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (profile->GetPrefs()->GetBoolean(prefs::kSmoothScrollingEnabled)) {
    base::CommandLine::ForCurrentProcess()->AppendSwitchNoDup(
        switches::kEnableSmoothScrolling);
  }
}
