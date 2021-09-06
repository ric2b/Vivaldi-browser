// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "update_notifier/thirdparty/winsparkle/src/config.h"

#include "base/check.h"
#include "base/no_destructor.h"

namespace winsparkle {

vivaldi::InstallType g_install_type = vivaldi::InstallType::kForCurrentUser;
bool g_install_mode = false;
bool g_manual_check = false;
bool g_silent_update = false;

namespace {

const wchar_t kInstaller[] = L"Installer";
const wchar_t kSetupExe[] = L"setup.exe";

Config& GetMutableConfig() {
  static base::NoDestructor<Config> config;
  return *config;
}

}  // namespace

Config::Config() = default;
Config::~Config() = default;
Config::Config(Config&& config) = default;

base::FilePath Config::GetSetupExe(const std::string& version) const {
  base::FilePath path = exe_dir;
  if (version.empty()) {
    path = path.Append(app_version);
  } else {
    base::FilePath version_subdir = base::FilePath::FromUTF8Unsafe(version);

    // Just in case version_subdir contains things like ../
    if (version_subdir.ReferencesParent()) {
      path = path.Append(L"unknown");
    } else {
      path = path.Append(version_subdir);
    }
  }
  path = path.Append(kInstaller);
  path = path.Append(kSetupExe);
  return path;
}

const Config& GetConfig() {
  return GetMutableConfig();
}

void InitConfig(Config config) {
  DCHECK(!config.language.empty());
  DCHECK(config.appcast_url.is_valid());
  DCHECK(!config.registry_path.empty());
  DCHECK(!config.app_name.empty());
  DCHECK(!config.app_version.empty());

  Config& global_config = GetMutableConfig();

  // InitConfig() can only be called once.
  DCHECK(global_config.appcast_url.is_empty());
  GetMutableConfig() = std::move(config);
}

}  // namespace winsparkle