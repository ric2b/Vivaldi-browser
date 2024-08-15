// Copyright (c) 2015-2018 Vivaldi Technologies AS. All rights reserved

#include "extraparts/vivaldi_browser_main_extra_parts_mac.h"

#import <Cocoa/Cocoa.h>

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_constants.h"
#include "base/apple/foundation_util.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/mac_util.h"
#include "browser/vivaldi_app_observer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_features.h"
#include "extraparts/vivaldi_keychain_util.h"

VivaldiBrowserMainExtraPartsMac::VivaldiBrowserMainExtraPartsMac() {}
VivaldiBrowserMainExtraPartsMac::~VivaldiBrowserMainExtraPartsMac() {}

std::unique_ptr<VivaldiBrowserMainExtraParts>
VivaldiBrowserMainExtraParts::Create() {
  return std::make_unique<VivaldiBrowserMainExtraPartsMac>();
}

// Overridden from ChromeBrowserMainExtraParts:
void VivaldiBrowserMainExtraPartsMac::PostEarlyInitialization() {
  VivaldiBrowserMainExtraParts::PostEarlyInitialization();

  // Set option to automatically install updates in the background to true.
  NSString* key =
    [NSString stringWithUTF8String:vivaldi::kSparkleAutoInstallSettingName];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  bool keyDoesNotExist = [defaults objectForKey:key] == nil;
  if (keyDoesNotExist) {
    [defaults setBool:true forKey:key];
  }
 }

void VivaldiBrowserMainExtraPartsMac::PreProfileInit() {
  VivaldiBrowserMainExtraParts::PreProfileInit();
  vivaldi::VivaldiAppObserver::GetFactoryInstance();

  #pragma clang diagnostic ignored "-Wunguarded-availability"
  [NSWindow setAllowsAutomaticWindowTabbing: NO];
}
