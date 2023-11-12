// Copyright (c) 2016-2019 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_RUNTIME_RUNTIME_API_H_
#define EXTENSIONS_API_RUNTIME_RUNTIME_API_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/singleton.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_statistics_common.h"
#include "extensions/browser/extension_function.h"
#include "extensions/schema/runtime_private.h"

class ProfileAttributesStorage;
class Browser;

namespace extensions {

namespace vivaldi {
namespace runtime_private {
struct ProfileStatEntry;
}
}  // namespace vivaldi

class RuntimeAPI {
 public:
  static void Init();
  static void OnProfileAvatarChanged(Profile* profile);
};

class RuntimePrivateExitFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.exit", RUNTIME_EXIT)

  RuntimePrivateExitFunction() = default;

 private:
  ~RuntimePrivateExitFunction() override = default;
  ResponseAction Run() override;
};

class RuntimePrivateRestartFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.restart", RUNTIME_RESTART_VIVALDI)

  RuntimePrivateRestartFunction() = default;

 private:
  ~RuntimePrivateRestartFunction() override = default;
  ResponseAction Run() override;
};

class RuntimePrivateGetAllFeatureFlagsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.getAllFeatureFlags",
                             RUNTIME_SETFEATUREENABLED)

  RuntimePrivateGetAllFeatureFlagsFunction() = default;

 private:
  ~RuntimePrivateGetAllFeatureFlagsFunction() override = default;

  ResponseAction Run() override;
};

class RuntimePrivateSetFeatureEnabledFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.setFeatureEnabled",
                             RUNTIME_GETALLFEATUREFLAGS)

  RuntimePrivateSetFeatureEnabledFunction() = default;

 private:
  ~RuntimePrivateSetFeatureEnabledFunction() override = default;

  ResponseAction Run() override;
};

class RuntimePrivateIsGuestSessionFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.isGuestSession",
                             RUNTIME_ISGUESTSESSION)

  RuntimePrivateIsGuestSessionFunction() = default;

 private:
  ~RuntimePrivateIsGuestSessionFunction() override = default;

  ResponseAction Run() override;
};

class RuntimePrivateHasGuestSessionFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.hasGuestSession",
                             RUNTIME_HASGUESTSESSION)

  RuntimePrivateHasGuestSessionFunction() = default;

 private:
  ~RuntimePrivateHasGuestSessionFunction() override = default;

  ResponseAction Run() override;
};

class RuntimePrivateSwitchToGuestSessionFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.switchToGuestSession",
                             RUNTIME_SWITCHTOGUESTSESSION)

  RuntimePrivateSwitchToGuestSessionFunction() = default;

 private:
  ~RuntimePrivateSwitchToGuestSessionFunction() override = default;

  ResponseAction Run() override;
};

class RuntimePrivateCloseGuestSessionFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.closeGuestSession",
                             RUNTIME_CLOSEGUESTSESSION)

  RuntimePrivateCloseGuestSessionFunction() = default;

 private:
  ~RuntimePrivateCloseGuestSessionFunction() override = default;

  ResponseAction Run() override;
};

class RuntimePrivateOpenProfileSelectionWindowFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.openProfileSelectionWindow",
                             RUNTIME_OPENPROFILESELECTIONWINDOW)

  RuntimePrivateOpenProfileSelectionWindowFunction() = default;

 private:
  ~RuntimePrivateOpenProfileSelectionWindowFunction() override = default;

  ResponseAction Run() override;
};

class RuntimePrivateGetUserProfilesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.getUserProfiles",
                             RUNTIME_GETUSERPROFILES)

  RuntimePrivateGetUserProfilesFunction() = default;

 private:
  ~RuntimePrivateGetUserProfilesFunction() override = default;

  void ProcessImagesOnWorkerThread(
      std::vector<vivaldi::runtime_private::UserProfile> profiles);
  void FinishProcessImagesOnUIThread(
      std::vector<vivaldi::runtime_private::UserProfile> profiles);

  ResponseAction Run() override;
};

class RuntimePrivateOpenNamedProfileFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.openNamedProfile",
                             RUNTIME_OPENNAMEDPROFILE)

  RuntimePrivateOpenNamedProfileFunction() = default;

 private:
  ~RuntimePrivateOpenNamedProfileFunction() override = default;

  ResponseAction Run() override;
};

class RuntimePrivateCloseActiveProfileFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.closeActiveProfile",
                             RUNTIME_CLOSEACTIVEPROFILE)

  RuntimePrivateCloseActiveProfileFunction() = default;

 private:
  ~RuntimePrivateCloseActiveProfileFunction() override = default;

  ResponseAction Run() override;
};

class RuntimePrivateGetUserProfileImagesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.getUserProfileImages",
                             RUNTIME_GETUSERPROFILEIMAGES)

  RuntimePrivateGetUserProfileImagesFunction() = default;

 private:
  ~RuntimePrivateGetUserProfileImagesFunction() override = default;

  ResponseAction Run() override;
};

class RuntimePrivateUpdateActiveProfileFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.updateActiveProfile",
                             RUNTIME_UPDATEACTIVEPROFILE)

  RuntimePrivateUpdateActiveProfileFunction() = default;

 private:
  ~RuntimePrivateUpdateActiveProfileFunction() override = default;

  ResponseAction Run() override;
};

class RuntimePrivateGetProfileDefaultsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.getProfileDefaults",
                             RUNTIME_GETPROFILEDEFAULTS)

  RuntimePrivateGetProfileDefaultsFunction() = default;

 private:
  ~RuntimePrivateGetProfileDefaultsFunction() override = default;

  ResponseAction Run() override;
};

class RuntimePrivateCreateProfileFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.createProfile",
                             RUNTIME_CREATEPROFILE)

  RuntimePrivateCreateProfileFunction() = default;

 private:
  ~RuntimePrivateCreateProfileFunction() override = default;

  void OnProfileCreated(bool create_shortcut,
                        Profile* profile);
  void CreateShortcutAndShowSuccess(bool create_shortcut, Profile* profile);
  void OnBrowserReadyCallback(Browser* browser);
  void OpenNewWindowForProfile(Profile* profile);

  ResponseAction Run() override;
};

class RuntimePrivateGetProfileStatisticsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.getProfileStatistics",
                             RUNTIME_GETPROFILESTATISTICS)

  RuntimePrivateGetProfileStatisticsFunction();

 private:
  void GatherStatistics(Profile* profile);
  void GetProfileStatsCallback(base::FilePath profile_path,
                               profiles::ProfileCategoryStats result);

  ~RuntimePrivateGetProfileStatisticsFunction() override;

  ResponseAction Run() override;

  std::vector<vivaldi::runtime_private::ProfileStatEntry> results_;
};

class RuntimePrivateDeleteProfileFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.deleteProfile",
                             RUNTIME_DELETEPROFILE)

  RuntimePrivateDeleteProfileFunction() = default;

 private:
  ~RuntimePrivateDeleteProfileFunction() override = default;

  ResponseAction Run() override;
};

class RuntimePrivateHasDesktopShortcutFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.hasDesktopShortcut",
                             RUNTIME_HASDESKTOPSHORTCUT)

  RuntimePrivateHasDesktopShortcutFunction() = default;

 private:
  ~RuntimePrivateHasDesktopShortcutFunction() override = default;

  ResponseAction Run() override;

  void OnHasProfileShortcuts(bool has_shortcuts);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_RUNTIME_RUNTIME_API_H_
