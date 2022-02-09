// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_CONFIG_H_
#define UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_CONFIG_H_

#include <string>

#include "base/files/file_path.h"
#include "base/version.h"

#include "installer/util/vivaldi_install_util.h"

namespace vivaldi_update_notifier {

enum class UpdateMode {
  kNone,

  // Manual update check with full UI.
  kManualCheck,

  // Notify the user about an available update without downloading it.
  kNotify,

  // Notify the user that an update was downloaded and is ready to be installed.
  kSilentDownload,

  // Fully silent update check without any user interaction.
  kSilentUpdate,

  // Network installation mode.
  kNetworkInstall,
};

constexpr bool WithVersionCheckUI(UpdateMode mode) {
  return mode == UpdateMode::kManualCheck ||
         mode == UpdateMode::kNetworkInstall;
}

constexpr bool WithDownloadUI(UpdateMode mode) {
  return mode != UpdateMode::kSilentDownload &&
         mode != UpdateMode::kSilentUpdate;
}

enum class RegistryItem {
  kDeltaPatchFailed,
  kSkipThisVersion,
};

std::string ReadRegistryIem(RegistryItem item);
void WriteRegistryItem(RegistryItem item, const std::string& value);

// Get the directory containing installation executables.
base::FilePath GetExeDir();

// Get setup.exe path for the current installation if any.
base::FilePath GetSetupExePath();

// Return true if update checks are run as a system service.
bool DoesRunAsSystemService();

extern vivaldi::InstallType g_install_type;

// The update notifier mode.
extern UpdateMode g_mode;

// The directory containing Vivaldi installation. When running updates this is
// deduced from the path of the curren executable.
extern base::FilePath g_install_dir;

#if defined(COMPONENT_BUILD)
// To support running update_notifier for development builds.
extern base::FilePath g_build_dir;
#endif

extern base::Version g_app_version;

}  // namespace vivaldi_update_notifier

#endif  // UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_CONFIG_H_
