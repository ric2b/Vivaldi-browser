// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "update_notifier/thirdparty/winsparkle/src/config.h"

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/installer/util/util_constants.h"

#include "installer/util/vivaldi_install_util.h"

namespace winsparkle {

vivaldi::InstallType g_install_type = vivaldi::InstallType::kForCurrentUser;
bool g_install_mode = false;
bool g_manual_check = false;
bool g_silent_update = false;
base::FilePath g_install_dir;
#if defined(COMPONENT_BUILD)
base::FilePath g_build_dir;
#endif
std::string g_language_code;
base::Version g_app_version;

namespace {

const wchar_t* GetRegistryItemName(RegistryItem key) {
  switch (key) {
    case RegistryItem::kDeltaPatchFailed:
      return vivaldi::constants::kVivaldiDeltaPatchFailed;
    case RegistryItem::kSkipThisVersion:
      return L"SkipThisVersion";
  }
}

}  // namespace

std::string ReadRegistryIem(RegistryItem item) {
  base::win::RegKey key = vivaldi::OpenRegistryKeyToRead(
      HKEY_CURRENT_USER, vivaldi::constants::kVivaldiAutoUpdateKey);
  std::wstring value =
      vivaldi::ReadRegistryString(GetRegistryItemName(item), key);
  return base::WideToUTF8(value);
}

void WriteRegistryItem(RegistryItem item, const std::string& value) {
  base::win::RegKey key = vivaldi::OpenRegistryKeyToWrite(
      HKEY_CURRENT_USER, vivaldi::constants::kVivaldiAutoUpdateKey);
  std::wstring wide_value = base::UTF8ToWide(value);
  vivaldi::WriteRegistryString(GetRegistryItemName(item), wide_value, key);
}

base::FilePath GetSetupExePath() {
  base::FilePath path = GetExeDir()
                            .AppendASCII(g_app_version.GetString())
                            .Append(installer::kInstallerDir)
                            .Append(installer::kSetupExe);
  return path;
}

base::FilePath GetExeDir() {
#if defined(COMPONENT_BUILD)
  if (!g_build_dir.empty())
    return g_build_dir;
#endif
  if (g_install_dir.empty())
    return base::FilePath();
  return g_install_dir.Append(installer::kInstallBinaryDir);
}

bool IsUsingTaskScheduler() {
  // Windows Task Scheduler entries are shared among all users. Although we use
  // a suffix based on the installation hash for task names, it is not enough
  // for system installs if multiple users want to receive update notifications.
  // Ultimately for system installs we should use a system account and silent
  // update shared by all users, but for now we keep the old auto-start based
  // system for system installs.
  return g_install_type == vivaldi::InstallType::kForCurrentUser;
}

}  // namespace winsparkle