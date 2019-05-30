// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "browser/shell_integration/vivaldi_shell_integration.h"

#import "third_party/mozilla/NSWorkspace+Utils.h"

namespace vivaldi {

namespace {

// Copied from chromium/chrome/browser/shell_integration_mac.mm
// Returns true if |identifier| is the bundle id of the default browser.
bool IsIdentifierDefaultBrowser(NSString* identifier) {
  NSString* default_browser =
      [[NSWorkspace sharedWorkspace] defaultBrowserIdentifier];
  if (!default_browser)
    return false;

  // We need to ensure we do the comparison case-insensitive as LS doesn't
  // persist the case of our bundle id.
  NSComparisonResult result =
      [default_browser caseInsensitiveCompare:identifier];
  return result == NSOrderedSame;
}

}

// Returns true if Opera is the default browser for the current user.
bool IsOperaDefaultBrowser() {
  return IsIdentifierDefaultBrowser(@"com.operasoftware.Opera");
}

// Returns true if Chrome is the default browser for the current user.
bool IsChromeDefaultBrowser() {
//todo arnar
  return false;
}

} //  namespace vivaldi
