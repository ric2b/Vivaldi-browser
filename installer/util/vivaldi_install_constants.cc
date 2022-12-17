// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#include "installer/util/vivaldi_install_constants.h"

namespace vivaldi {
namespace constants {

#define VIVALDI_REGISTRY_KEY L"Software\\Vivaldi"

const wchar_t kVivaldiDeltaPatchFailed[] = L"DeltaPatchFailed";
const wchar_t kVivaldiKey[] = VIVALDI_REGISTRY_KEY;
const wchar_t kVivaldiAutoUpdateKey[] = VIVALDI_REGISTRY_KEY "\\AutoUpdate";
const wchar_t kVivaldiPinToTaskbarValue[] = L"EnablePinToTaskbar";
const wchar_t kVivaldiToastActivatorCLSID[] = VIVALDI_REGISTRY_KEY "\\ToastActivatorCLSID";

// Vivaldi installer settings from last install.
const wchar_t kVivaldiInstallerDestinationFolder[] = L"DestinationFolder";
const wchar_t kVivaldiInstallerInstallType[] = L"InstallType";
const wchar_t kVivaldiInstallerDefaultBrowser[] = L"DefaultBrowser";
const wchar_t kVivaldiInstallerRegisterBrowser[] = L"RegisterBrowser";
const wchar_t kVivaldiInstallerAdvancedMode[] = L"AdvancedMode";
const wchar_t kVivaldiInstallerDisableStandaloneAutoupdate[] =
    L"DisableStandaloneAutoupdate";

// Vivaldi paths and filenames

// Marker file which existence indicates a standalone installation.
const wchar_t kStandaloneMarkerFile[] = L"stp.viv";
const wchar_t kSystemMarkerFile[] = L"sys.viv";
const wchar_t kVivaldiUpdateNotifierExe[] = L"update_notifier.exe";
const wchar_t kVivaldiUpdateNotifierOldExe[] = L"update_notifier.old";

// Vivaldi installer command line switches.

// Use the given installation directory overriding the value from the registry.
const char kVivaldiInstallDir[] = "vivaldi-install-dir";

// Language to use during the installation.
const char kVivaldiLanguage[] = "vivaldi-language";

// Installer runs from the mini installer.
const char kVivaldiMini[] = VIVALDI_INSTALLER_SWITCH_MINI;

// Attempt to register a browser as a default even for a stand-alone
// installation.
const char kVivaldiRegisterStandalone[] = "vivaldi-register-standalone";

// The update should run in the background with no interaction with the user.
const char kVivaldiSilent[] = VIVALDI_INSTALLER_SWITCH_SILENT;

// Install a stand-alone browser.
const char kVivaldiStandalone[] = "vivaldi-standalone";

// The installer or mini_installer shall just unpack/uncompress files without
// installing anything.
const char kVivaldiUnpack[] = VIVALDI_INSTALLER_SWITCH_UNPACK;

// The installation is to update the existing browser.
const char kVivaldiUpdate[] = "vivaldi-update";

#if !defined(OFFICIAL_BUILD)

// Pointer to the real exe to copy or use as a base path when the current
// executable is a development build unrelated to the installation.
const char kVivaldiDebugTargetExe[] = "vivaldi-debug-target-exe";

const char kDebugSetupCommandEnvironment[] = "VIVALDI_DEBUG_SETUP_COMMAND";
#endif

const wchar_t kUninstallSurveyUrl[] =
    L"https://vivaldi.com/uninstall/feedback?hl=$1";

}  // namespace constants
}  // namespace vivaldi