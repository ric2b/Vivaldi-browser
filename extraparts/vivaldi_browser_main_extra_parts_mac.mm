// Copyright (c) 2015-2018 Vivaldi Technologies AS. All rights reserved

#include "extraparts/vivaldi_browser_main_extra_parts_mac.h"

#include "app/vivaldi_apptools.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "browser/vivaldi_app_observer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_features.h"
#include "extraparts/maybe_setup_vivaldi_keychain.h"

VivaldiBrowserMainExtraPartsMac::VivaldiBrowserMainExtraPartsMac() {}
VivaldiBrowserMainExtraPartsMac::~VivaldiBrowserMainExtraPartsMac() {}

VivaldiBrowserMainExtraParts* VivaldiBrowserMainExtraParts::Create() {
  return new VivaldiBrowserMainExtraPartsMac();
}

bool VivaldiBrowserMainExtraPartsMac::checkVersionPath(std::string location,
                                                     std::string pepperPath,
                                                     std::string* versionPath) {
  if (base::PathExists(base::FilePath(location))) {
    base::FileEnumerator path_enum(base::FilePath(location), false,
                                   base::FileEnumerator::DIRECTORIES);
    for (base::FilePath name = path_enum.Next(); !name.empty();
         name = path_enum.Next()) {
      if (base::PathExists(name.Append(pepperPath))) {
        *versionPath = name.Append(pepperPath).MaybeAsASCII();
        return true;
      }
    }
  }
  return false;
}

// Overridden from ChromeBrowserMainExtraParts:
void VivaldiBrowserMainExtraPartsMac::PostEarlyInitialization() {
  VivaldiBrowserMainExtraParts::PostEarlyInitialization();

  if (vivaldi::IsVivaldiRunning())
    vivaldi::MaybeSetupVivaldiKeychain();

  // Add Flash - check default adobe install path
  std::string pepperFlashPath =
      "/Library/Internet Plug-Ins/PepperFlashPlayer/PepperFlashPlayer.plugin";
  if (!base::PathExists(base::FilePath(pepperFlashPath))) {
    // look for chrome bundled pepper flash
    // Pre macOS 10.13 location
    std::string location =
        "/Applications/Google Chrome.app/Contents/Versions/";
    std::string pepperPath = "Google Chrome Framework.framework/"
        "Internet Plug-Ins/PepperFlash/PepperFlashPlayer.plugin";

    std::string versionPath;
    if (checkVersionPath(location, pepperPath, &versionPath)) {
      pepperFlashPath = versionPath;
    }

    if (!base::PathExists(base::FilePath(pepperFlashPath))) {
      // Post macOS 10.13 location
      NSArray* theDirs = [[NSFileManager defaultManager]
                          URLsForDirectory:NSApplicationSupportDirectory
                          inDomains:NSUserDomainMask];
      std::string appSupportDir = [[theDirs objectAtIndex:0]
                                   fileSystemRepresentation];

      location = appSupportDir + "/Google/Chrome/PepperFlash";
      pepperPath = "PepperFlashPlayer.plugin";

      if (checkVersionPath(location, pepperPath, &versionPath)) {
        pepperFlashPath = versionPath;
      }
    }
  }

  if (base::PathExists(base::FilePath(pepperFlashPath))) {
    base::CommandLine::ForCurrentProcess()->AppendSwitchNative(
                                    switches::kPpapiFlashPath, pepperFlashPath);
  }
 }

void VivaldiBrowserMainExtraPartsMac::PreProfileInit() {
  VivaldiBrowserMainExtraParts::PreProfileInit();
  vivaldi::VivaldiAppObserver::GetFactoryInstance();

#if defined(MAC_OS_X_VERSION_10_12)
  if (base::mac::IsAtLeastOS10_12()) {
    #pragma clang diagnostic ignored "-Wunguarded-availability"
    [NSWindow setAllowsAutomaticWindowTabbing: NO];
  }
#endif
}
