// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.

#ifndef INSTALLER_UTIL_VIVALDI_INSTALL_UTIL_H_
#define INSTALLER_UTIL_VIVALDI_INSTALL_UTIL_H_

#include <windows.h>
#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/version.h"

namespace vivaldi {

// Does the post uninstall operations - open the URL to the uninstall survey.
void DoPostUninstallOperations(const base::Version& version);

// Launches process non-elevated even if the caller is elevated,
// using explorer. Reference:
// https://blogs.msdn.microsoft.com/oldnewthing/20131118-00/?p=2643
bool ShellExecuteFromExplorer(const base::FilePath& application_path,
                              const base::string16& parameters,
                              const base::FilePath& directory,
                              const base::string16& operation,
                              int nShowCmd = SW_SHOWDEFAULT);

// Returns the URL with parameters for the 'new features' page.
// The new |version| is appended as a parameter in the URL.
base::string16 GetNewFeaturesUrl(const base::Version& version);
}  // namespace vivaldi

#endif  // INSTALLER_UTIL_VIVALDI_INSTALL_UTIL_H_
