// Copyright (c) 2016-2019 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_RUNTIME_RUNTIME_API_H_
#define EXTENSIONS_API_RUNTIME_RUNTIME_API_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_statistics_common.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/prefs/json_pref_store.h"
#include "extensions/browser/extension_function.h"

class ProfileAttributesStorage;

namespace extensions {

class VivaldiRuntimeFeatures;
namespace vivaldi {
namespace runtime_private {
struct ProfileStatEntry;
}
}

typedef struct FeatureEntry {
  FeatureEntry();

  std::string friendly_name;
  std::string description;
  // in-memory active value
  bool enabled;
  // in-memory active value for internal builds
  bool internal_enabled = false;
  // The value set in the config file
  bool default_enabled;
  // Like default, except for internal (and sopranos builds)
  bool internal_default_enabled = false;
  // true if set by cmd line, value is fixed.
  bool force_value = false;
} * FeatureEntryPtr;

typedef std::map<std::string, FeatureEntryPtr> FeatureEntryMap;

class VivaldiRuntimeFeaturesFactory : public ProfileAttributesStorage::Observer,
                                      public BrowserContextKeyedServiceFactory {
 public:
  static VivaldiRuntimeFeatures* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static VivaldiRuntimeFeaturesFactory* GetInstance();

  static const bool kServiceRedirectedInIncognito = true;

  // ProfileAttributesStorage::Observer:
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWasRemoved(const base::FilePath& profile_path,
    const base::string16& profile_name) override;
  void OnProfileNameChanged(const base::FilePath& profile_path,
    const base::string16& old_profile_name) override;
  void OnProfileAuthInfoChanged(const base::FilePath& profile_path) override;
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;
  void OnProfileHighResAvatarLoaded(
    const base::FilePath& profile_path) override;
  void OnProfileSigninRequiredChanged(
    const base::FilePath& profile_path) override;
  void OnProfileIsOmittedChanged(const base::FilePath& profile_path) override;

 private:
  friend struct base::DefaultSingletonTraits<VivaldiRuntimeFeaturesFactory>;

  VivaldiRuntimeFeaturesFactory();
  ~VivaldiRuntimeFeaturesFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  void UpdateProfiles();
};

class VivaldiRuntimeFeatures : public KeyedService {
 public:
  explicit VivaldiRuntimeFeatures(Profile*);
  VivaldiRuntimeFeatures();
  ~VivaldiRuntimeFeatures() override;

  // Call to check if a named feature is enabled.
  static bool IsEnabled(content::BrowserContext* browser_context,
                        const std::string& feature_name);

  FeatureEntry* FindNamedFeature(const std::string& feature_name);
  const FeatureEntryMap& GetAllFeatures();

 private:
  void LoadRuntimeFeatures();
  bool GetFlags(FeatureEntryMap* flags);

  Profile* profile_;
  scoped_refptr<JsonPrefStore> store_;
  FeatureEntryMap flags_;

  base::WeakPtrFactory<VivaldiRuntimeFeatures> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiRuntimeFeatures);
};

class RuntimePrivateExitFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.exit", RUNTIME_EXIT)

  RuntimePrivateExitFunction() = default;

 private:
  ~RuntimePrivateExitFunction() override = default;
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(RuntimePrivateExitFunction);
};

class RuntimePrivateGetAllFeatureFlagsFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.getAllFeatureFlags",
                             RUNTIME_SETFEATUREENABLED)

  RuntimePrivateGetAllFeatureFlagsFunction() = default;

 private:
  ~RuntimePrivateGetAllFeatureFlagsFunction() override = default;

  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(RuntimePrivateGetAllFeatureFlagsFunction);
};

class RuntimePrivateSetFeatureEnabledFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.setFeatureEnabled",
                             RUNTIME_GETALLFEATUREFLAGS)

  RuntimePrivateSetFeatureEnabledFunction() = default;

 private:
  ~RuntimePrivateSetFeatureEnabledFunction() override = default;

  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(RuntimePrivateSetFeatureEnabledFunction);
};

class RuntimePrivateIsGuestSessionFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.isGuestSession",
                             RUNTIME_ISGUESTSESSION)

  RuntimePrivateIsGuestSessionFunction() = default;

 private:
  ~RuntimePrivateIsGuestSessionFunction() override = default;

  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(RuntimePrivateIsGuestSessionFunction);
};

class RuntimePrivateHasGuestSessionFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.hasGuestSession",
                             RUNTIME_HASGUESTSESSION)

  RuntimePrivateHasGuestSessionFunction() = default;

 private:
  ~RuntimePrivateHasGuestSessionFunction() override = default;

  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(RuntimePrivateHasGuestSessionFunction);
};

class RuntimePrivateSwitchToGuestSessionFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.switchToGuestSession",
                             RUNTIME_SWITCHTOGUESTSESSION)

  RuntimePrivateSwitchToGuestSessionFunction() = default;

 private:
  ~RuntimePrivateSwitchToGuestSessionFunction() override = default;

  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(RuntimePrivateSwitchToGuestSessionFunction);
};

class RuntimePrivateCloseGuestSessionFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.closeGuestSession",
                             RUNTIME_CLOSEGUESTSESSION)

  RuntimePrivateCloseGuestSessionFunction() = default;

 private:
  ~RuntimePrivateCloseGuestSessionFunction() override = default;

  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(RuntimePrivateCloseGuestSessionFunction);
};

class RuntimePrivateOpenProfileSelectionWindowFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.openProfileSelectionWindow",
                             RUNTIME_OPENPROFILESELECTIONWINDOW)

  RuntimePrivateOpenProfileSelectionWindowFunction() = default;

 private:
  ~RuntimePrivateOpenProfileSelectionWindowFunction() override = default;

  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(RuntimePrivateOpenProfileSelectionWindowFunction);
};

class RuntimePrivateGetUserProfilesFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.getUserProfiles",
                             RUNTIME_GETUSERPROFILES)

  RuntimePrivateGetUserProfilesFunction() = default;

 private:
  ~RuntimePrivateGetUserProfilesFunction() override = default;

  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(RuntimePrivateGetUserProfilesFunction);
};

class RuntimePrivateOpenNamedProfileFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.openNamedProfile",
                             RUNTIME_OPENNAMEDPROFILE)

  RuntimePrivateOpenNamedProfileFunction() = default;

 private:
  ~RuntimePrivateOpenNamedProfileFunction() override = default;

  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(RuntimePrivateOpenNamedProfileFunction);
};

class RuntimePrivateCloseActiveProfileFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.closeActiveProfile",
                             RUNTIME_CLOSEACTIVEPROFILE)

  RuntimePrivateCloseActiveProfileFunction() = default;

 private:
  ~RuntimePrivateCloseActiveProfileFunction() override = default;

  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(RuntimePrivateCloseActiveProfileFunction);
};

class RuntimePrivateGetUserProfileImagesFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.getUserProfileImages",
                             RUNTIME_GETUSERPROFILEIMAGES)

  RuntimePrivateGetUserProfileImagesFunction() = default;

 private:
  ~RuntimePrivateGetUserProfileImagesFunction() override = default;

  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(RuntimePrivateGetUserProfileImagesFunction);
};

class RuntimePrivateUpdateActiveProfileFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.updateActiveProfile",
                             RUNTIME_UPDATEACTIVEPROFILE)

  RuntimePrivateUpdateActiveProfileFunction() = default;

 private:
  ~RuntimePrivateUpdateActiveProfileFunction() override = default;

  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(RuntimePrivateUpdateActiveProfileFunction);
};

class RuntimePrivateGetProfileDefaultsFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.getProfileDefaults",
                             RUNTIME_GETPROFILEDEFAULTS)

  RuntimePrivateGetProfileDefaultsFunction() = default;

 private:
  ~RuntimePrivateGetProfileDefaultsFunction() override = default;

  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(RuntimePrivateGetProfileDefaultsFunction);
};

class RuntimePrivateCreateProfileFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.createProfile",
                             RUNTIME_CREATEPROFILE)

  RuntimePrivateCreateProfileFunction() = default;

 private:
  ~RuntimePrivateCreateProfileFunction() override = default;

  void OnProfileCreated(bool create_shortcut,
                        Profile* profile,
                        Profile::CreateStatus status);
  void CreateShortcutAndShowSuccess(bool create_shortcut, Profile* profile);
  void OnBrowserReadyCallback(Profile* profile,
                              Profile::CreateStatus profile_create_status);
  void OpenNewWindowForProfile(Profile* profile, Profile::CreateStatus status);

  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(RuntimePrivateCreateProfileFunction);
};

class RuntimePrivateGetProfileStatisticsFunction
    : public UIThreadExtensionFunction {
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

  DISALLOW_COPY_AND_ASSIGN(RuntimePrivateGetProfileStatisticsFunction);
};

class RuntimePrivateDeleteProfileFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.deleteProfile",
                             RUNTIME_DELETEPROFILE)

  RuntimePrivateDeleteProfileFunction() = default;

 private:
  ~RuntimePrivateDeleteProfileFunction() override = default;

  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(RuntimePrivateDeleteProfileFunction);
};

class RuntimePrivateHasDesktopShortcutFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.hasDesktopShortcut",
                             RUNTIME_HASDESKTOPSHORTCUT)

  RuntimePrivateHasDesktopShortcutFunction() = default;

 private:
  ~RuntimePrivateHasDesktopShortcutFunction() override = default;

  ResponseAction Run() override;

  void OnHasProfileShortcuts(bool has_shortcuts);

  DISALLOW_COPY_AND_ASSIGN(RuntimePrivateHasDesktopShortcutFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_RUNTIME_RUNTIME_API_H_
