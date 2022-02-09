// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "update_notifier/thirdparty/winsparkle/src/config.h"

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/installer/util/util_constants.h"

#include "installer/util/vivaldi_install_util.h"

namespace vivaldi_update_notifier {

vivaldi::InstallType g_install_type = vivaldi::InstallType::kForCurrentUser;
UpdateMode g_mode = UpdateMode::kNone;
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

bool DoesRunAsSystemService() {
  // This is not yet ready.
  if ((false)) {
    return g_install_type == vivaldi::InstallType::kForAllUsers &&
           g_mode == UpdateMode::kSilentUpdate;
  }
  return false;
}

}  // namespace vivaldi_update_notifier