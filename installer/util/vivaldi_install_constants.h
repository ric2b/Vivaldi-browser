// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved.

#ifndef INSTALLER_UTIL_VIVALDI_INSTALL_CONSTANTS_H_
#define INSTALLER_UTIL_VIVALDI_INSTALL_CONSTANTS_H_

namespace vivaldi {
namespace constants {

// Vivaldi registry.
extern const wchar_t kVivaldiDeltaPatchFailed[];
extern const wchar_t kVivaldiKey[];
extern const wchar_t kVivaldiAutoUpdateKey[];
extern const wchar_t kVivaldiPinToTaskbarValue[];

// Vivaldi installer settings from last install.
extern const wchar_t kVivaldiInstallerDestinationFolder[];
extern const wchar_t kVivaldiInstallerInstallType[];
extern const wchar_t kVivaldiInstallerDefaultBrowser[];
extern const wchar_t kVivaldiInstallerRegisterBrowser[];
extern const wchar_t kVivaldiInstallerAdvancedMode[];
extern const wchar_t kVivaldiInstallerDisableStandaloneAutoupdate[];
extern const wchar_t kVivaldiToastActivatorCLSID[];

// Vivaldi paths and filenames
extern const wchar_t kStandaloneMarkerFile[];
extern const wchar_t kSystemMarkerFile[];
extern const wchar_t kVivaldiUpdateNotifierExe[];
extern const wchar_t kVivaldiUpdateNotifierOldExe[];

// Vivaldi installer command line switches.
extern const char kVivaldiInstallDir[];
extern const char kVivaldiLanguage[];
extern const char kVivaldiMini[];
extern const char kVivaldiRegisterStandalone[];
extern const char kVivaldiSilent[];
extern const char kVivaldiStandalone[];
extern const char kVivaldiUnpack[];
extern const char kVivaldiUpdate[];

// Vivaldi installer switches and environment for debugging not supported in
// official builds.
extern const char kVivaldiDebugTargetExe[];
extern const char kDebugSetupCommandEnvironment[];

extern const wchar_t kUninstallSurveyUrl[];

}  // namespace constants
}  // namespace vivaldi

// Switches duplicated as macros to use from mini_installer
#define VIVALDI_INSTALLER_SWITCH_MINI "vivaldi-mini"
#define VIVALDI_INSTALLER_SWITCH_SILENT "vivaldi-silent"
#define VIVALDI_INSTALLER_SWITCH_UNPACK "vivaldi-unpack"

#endif  // INSTALLER_UTIL_VIVALDI_INSTALL_UTIL_H_
