// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/helper.h"

#include "base/path_service.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/util_constants.h"

#include "base/command_line.h"

namespace installer {

base::FilePath GetChromeInstallPath(bool system_install,
                                    BrowserDistribution* dist) {
  base::FilePath install_path;

  // Vivaldi customization. Use the install path provided by the command line switch: --vivaldi-install-dir=<filepath>
  const base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kVivaldiInstallDir)) {
    install_path = command_line.GetSwitchValuePath(switches::kVivaldiInstallDir).Append(kInstallBinaryDir);
    return install_path;
  }
#if defined(_WIN64)
  // TODO(wfh): Place Chrome binaries into DIR_PROGRAM_FILESX86 until the code
  // to support moving the binaries is added.
  int key =
      system_install ? base::DIR_PROGRAM_FILESX86 : base::DIR_LOCAL_APP_DATA;
#else
  int key = system_install ? base::DIR_PROGRAM_FILES : base::DIR_LOCAL_APP_DATA;
#endif
  if (PathService::Get(key, &install_path)) {
    install_path = install_path.Append(dist->GetInstallSubDir());
    install_path = install_path.Append(kInstallBinaryDir);
  }
  return install_path;
}

}  // namespace installer.
