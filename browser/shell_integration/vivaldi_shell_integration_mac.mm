// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "browser/shell_integration/vivaldi_shell_integration.h"

#include <AppKit/AppKit.h>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"

namespace vivaldi {

namespace {

// Copied from chromium/chrome/browser/shell_integration_mac.mm
// Returns the bundle id of the default client application for the given
// scheme or nil on failure.
NSString* GetBundleIdForDefaultAppForScheme(NSString* scheme) {
  NSURL* scheme_url =
      [NSURL URLWithString:[scheme stringByAppendingString:@":"]];

  NSURL* default_app_url =
      [NSWorkspace.sharedWorkspace URLForApplicationToOpenURL:scheme_url];
  if (!default_app_url) {
    return nil;
  }

  NSBundle* default_app_bundle = [NSBundle bundleWithURL:default_app_url];
  return default_app_bundle.bundleIdentifier;
}

}

// Returns true if Opera is the default browser for the current user.
bool IsOperaDefaultBrowser() {
  return [GetBundleIdForDefaultAppForScheme(@"http")
    isEqualToString:@"com.operasoftware.Opera"];
}

// Returns true if Chrome is the default browser for the current user.
bool IsChromeDefaultBrowser() {
  return [GetBundleIdForDefaultAppForScheme(@"http")
    isEqualToString:@"com.google.Chrome"];
}

} //  namespace vivaldi
