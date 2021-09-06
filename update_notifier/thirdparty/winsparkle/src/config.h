// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_CONFIG_H_
#define UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_CONFIG_H_

#include <string>

#include "base/files/file_path.h"
#include "base/version.h"

#include "installer/util/vivaldi_install_util.h"

namespace winsparkle {

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

bool IsUsingTaskScheduler();

extern vivaldi::InstallType g_install_type;

// True if using a network installer mode.
extern bool g_install_mode;

// True when running a manual update check. The flag must be accessed only from
// the main thread as it can change if the user requested a manual check when
// the automated check already runs.
extern bool g_manual_check;

// True in the silent update mode.
extern bool g_silent_update;

// The directory containing Vivaldi installation. When running updates this is
// deduced from the path of the curren executable.
extern base::FilePath g_install_dir;

#if defined(COMPONENT_BUILD)
// To support running update_notifier for development builds.
extern base::FilePath g_build_dir;
#endif

extern base::Version g_app_version;

}  // namespace winsparkle

#endif  // UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_CONFIG_H_
