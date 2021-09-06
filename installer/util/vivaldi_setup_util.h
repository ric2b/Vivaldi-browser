// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#ifndef INSTALLER_UTIL_VIVALDI_SETUP_UTIL_H_
#define INSTALLER_UTIL_VIVALDI_SETUP_UTIL_H_

#include "base/files/file_path.h"
#include "base/win/windows_types.h"
#include "chrome/installer/util/util_constants.h"
#include "base/version.h"

#include "installer/util/vivaldi_install_constants.h"
#include "installer/util/vivaldi_install_util.h"

// Vivaldi-specific code for setup.exe

class WorkItemList;

namespace installer {

class InstallerState;
struct InstallParams;

}  // namespace installer

namespace vivaldi {

bool PrepareSetupConfig(HINSTANCE instance);

bool BeginInstallOrUninstall(HINSTANCE instance,
                             const installer::InstallerState& installer_state);

// Do Vivaldi-specific registration. Return false if registration should be
// skipped.
bool PrepareRegistration(const installer::InstallerState& installer_state);

void EndInstallOrUninstall(const installer::InstallerState& installer_state,
                           installer::InstallStatus install_status);

void FinalizeSuccessfullInstall(
    const installer::InstallerState& installer_state,
    installer::InstallStatus install_status);

void AddVivaldiSpecificWorkItems(const installer::InstallParams& install_params,
                                 WorkItemList* install_list);

void UnregisterUpdateNotifier(const installer::InstallerState& installer_state);

// Shows a modal messagebox with the installer result localized string.
void ShowInstallerResultMessage(int string_resource_id);

// Does the post uninstall operations - open the URL to the uninstall survey.
void DoPostUninstallOperations(const base::Version& version);

// Helpers to check various Vivaldi-specific command-line flags.
bool IsInstallUpdate();
bool IsInstallStandalone();
bool IsInstallRegisterStandalone();
bool IsInstallSilentUpdate();

#if !defined(OFFICIAL_BUILD)
// Path to use for all following setup.exe invocations instead of the setup.exe
// from the installation directory.
extern base::FilePath* debug_subprocesses_exe;
#endif

}  // namespace vivaldi

#endif  // INSTALLER_UTIL_VIVALDI_SETUP_UTIL_H_
