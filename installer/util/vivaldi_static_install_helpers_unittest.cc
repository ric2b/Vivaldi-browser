// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#include "installer/util/vivaldi_static_install_helpers.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_util_win.h"
#include "base/test/test_file_util.h"
#include "components/version_info/version_info_values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vivaldi {

TEST(VivaldiStaticInstallHelpers, IsSystemInstallExecutable) {
  // Enumerate all cases that may happen for various paths.
  const std::wstring kVivaldi = L"vivaldi.exe";
  const std::wstring kNotificationHelper = L"notification_helper.exe";
  const std::wstring kTestExePaths[] = {
      kVivaldi,
      L"vivaldi_proxy.exe",
      L"update_notifier.exe",
      L"1.1.1.1\\" + kNotificationHelper,
      L"" VIVALDI_VERSION_STRING "\\" + kNotificationHelper,
  };
  base::FilePath dir = base::CreateUniqueTempDirectoryScopedToTest();
  for (const std::wstring& path : kTestExePaths) {
    base::FilePath exe_path = dir.Append(path);
    EXPECT_FALSE(IsSystemInstallExecutable(exe_path.value()));
    EXPECT_FALSE(
        IsSystemInstallExecutable(base::ToUpperASCII(exe_path.value())));
  }
  static const char file_header[] = "Hello, world!";
  EXPECT_TRUE(base::WriteFile(dir.Append(vivaldi::constants::kSystemMarkerFile),
                            file_header));
  for (const std::wstring& path : kTestExePaths) {
    base::FilePath exe_path = dir.Append(path);
    EXPECT_TRUE(IsSystemInstallExecutable(exe_path.value()));
    EXPECT_TRUE(
        IsSystemInstallExecutable(base::ToUpperASCII(exe_path.value())));
  }

  // Check that depending on the executable name we pick up the right directory
  // for the marker.

  EXPECT_FALSE(IsSystemInstallExecutable(
      dir.Append(L"" VIVALDI_VERSION_STRING "\\" + kVivaldi).value()));
  EXPECT_FALSE(
      IsSystemInstallExecutable(dir.Append(kNotificationHelper).value()));

  // Check for robustness.

  EXPECT_FALSE(IsSystemInstallExecutable(L""));
  EXPECT_FALSE(IsSystemInstallExecutable(L"\\"));
  EXPECT_FALSE(IsSystemInstallExecutable(L"\\\\"));
  EXPECT_FALSE(IsSystemInstallExecutable(L"\\x\\"));

  EXPECT_FALSE(IsSystemInstallExecutable(kNotificationHelper));
  EXPECT_FALSE(IsSystemInstallExecutable(kVivaldi));
  EXPECT_FALSE(IsSystemInstallExecutable(L"" VIVALDI_VERSION_STRING "\\" +
                                         kNotificationHelper));
  EXPECT_FALSE(IsSystemInstallExecutable(L"" VIVALDI_VERSION_STRING "\\" + kVivaldi));
}

}  // namespace vivaldi
