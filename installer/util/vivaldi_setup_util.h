// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#ifndef INSTALLER_UTIL_VIVALDI_SETUP_UTIL_H_
#define INSTALLER_UTIL_VIVALDI_SETUP_UTIL_H_

#include "base/files/file_path.h"
#include "base/win/windows_types.h"
#include "chrome/installer/util/util_constants.h"

namespace installer {
class InstallerState;
}

namespace vivaldi {

bool PrepareSetupConfig(HINSTANCE instance);

bool BeginInstallOrUninstall(HINSTANCE instance,
                             const installer::InstallerState& installer_state);

void EndInstallOrUninstall(const installer::InstallerState& installer_state,
                           installer::InstallStatus install_status);

void FinalizeSuccessfullInstall(
    const installer::InstallerState& installer_state,
    installer::InstallStatus install_status);

}  // namespace vivaldi

#endif  // INSTALLER_UTIL_VIVALDI_SETUP_UTIL_H_
