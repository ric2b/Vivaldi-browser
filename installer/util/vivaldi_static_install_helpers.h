// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#ifndef INSTALLER_UTIL_VIVALDI_STATIC_INSTALL_HELPERS_H_
#define INSTALLER_UTIL_VIVALDI_STATIC_INSTALL_HELPERS_H_

#include <string>

#include "installer/util/vivaldi_install_constants.h"

// Utilities extending chromium/chrome/install_static/ functionality

namespace vivaldi {

// If Vivaldi installation is standalone, return true and place the User Data
// directory to result. Otherwise return false without touching result.
bool GetStandaloneInstallDataDirectory(std::wstring& result);

// Return true is exe_path belong to a system-level installation. exe_path must
// be an absolute path that uses \ as a path separators.
bool IsSystemInstallExecutable(const std::wstring& exe_path);

}  // namespace vivaldi

#endif  // INSTALLER_UTIL_VIVALDI_STATIC_INSTALL_HELPERS_H_
