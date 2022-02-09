// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "browser/shell_integration/vivaldi_shell_integration.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"

namespace vivaldi {

namespace {
// Copied from chromium/chrome/browser/shell_integration_linux.cc
const char kXdgSettings[] = "xdg-settings";
const char kXdgSettingsDefaultBrowser[] = "default-web-browser";
}  // namespace

bool IsOperaDefaultBrowser() {
  std::vector<std::string> argv;
  argv.push_back(kXdgSettings);
  argv.push_back("get");
  argv.push_back(kXdgSettingsDefaultBrowser);

  std::string browser;
  // We don't care about the return value here.
  base::GetAppOutput(base::CommandLine(argv), &browser);
  return base::ToLowerASCII(browser).find("opera") != std::string::npos;
}

bool IsChromeDefaultBrowser() {
  // todo arnar
  return false;
}

}  //  namespace vivaldi
