// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_CONFIG_H_
#define UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_CONFIG_H_

#include <string>

#include "base/files/file_path.h"
#include "url/gurl.h"

#include "installer/util/vivaldi_install_util.h"

namespace winsparkle {

// Move-only configuration object.
struct Config {
  Config();
  ~Config();
  Config(Config&&);
  Config& operator=(Config&&) = default;
  DISALLOW_COPY_AND_ASSIGN(Config);

  std::string language;
  GURL appcast_url;
  std::wstring registry_path;
  std::wstring app_name;
  std::wstring app_version;

  // Directory with the current executable.
  base::FilePath exe_dir;

  base::FilePath GetSetupExe(const std::string& version = std::string()) const;
};

// Get global configuration singleton.
const Config& GetConfig();

// Initialize global configuration.
void InitConfig(Config config);

extern vivaldi::InstallType g_install_type;

// True if using a network installer mode.
extern bool g_install_mode;

// True when running a manual update check. The flag must be accessed only from
// the main thread as it can change if the user requested a manual check when
// the automated check already runs.
extern bool g_manual_check;

// True in the silent update mode.
extern bool g_silent_update;

}  // namespace winsparkle

#endif  // UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_CONFIG_H_
