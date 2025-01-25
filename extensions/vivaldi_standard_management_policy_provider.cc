// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include <string>

#include "extensions/vivaldi_standard_management_policy_provider.h"

#include "app/vivaldi_apptools.h"

namespace extensions {

VivaldiStandardManagementPolicyProvider::
    VivaldiStandardManagementPolicyProvider(ExtensionManagement* settings,
                                            Profile* profile)
    : StandardManagementPolicyProvider(settings, profile) {}

VivaldiStandardManagementPolicyProvider::
    ~VivaldiStandardManagementPolicyProvider() {}

bool VivaldiStandardManagementPolicyProvider::UserMayLoad(
    const Extension* extension,
    std::u16string* error) const {
  return StandardManagementPolicyProvider::UserMayLoad(extension, error);
}

bool VivaldiStandardManagementPolicyProvider::UserMayInstall(
    const Extension* extension,
    std::u16string* error) const {
  return StandardManagementPolicyProvider::UserMayInstall(extension, error);
}

bool VivaldiStandardManagementPolicyProvider::UserMayModifySettings(
    const Extension* extension,
    std::u16string* error) const {
  if (vivaldi::IsVivaldiApp(extension->id())) {
    return false;
  }
  return StandardManagementPolicyProvider::UserMayModifySettings(extension,
                                                                 error);
}

bool VivaldiStandardManagementPolicyProvider::ExtensionMayModifySettings(
    const Extension* source_extension,
    const Extension* extension,
    std::u16string* error) const {
  if (vivaldi::IsVivaldiApp(extension->id())) {
    return false;
  }
  return StandardManagementPolicyProvider::ExtensionMayModifySettings(
      source_extension, extension, error);
}

bool VivaldiStandardManagementPolicyProvider::MustRemainEnabled(
    const Extension* extension,
    std::u16string* error) const {
  if (vivaldi::IsVivaldiApp(extension->id())) {
    return true;
  }
  return StandardManagementPolicyProvider::MustRemainEnabled(extension, error);
}

bool VivaldiStandardManagementPolicyProvider::MustRemainDisabled(
    const Extension* extension,
    disable_reason::DisableReason* reason) const {
  return StandardManagementPolicyProvider::MustRemainDisabled(extension, reason);
}

bool VivaldiStandardManagementPolicyProvider::MustRemainInstalled(
    const Extension* extension,
    std::u16string* error) const {
  if (vivaldi::IsVivaldiApp(extension->id())) {
    return true;
  }
  return StandardManagementPolicyProvider::MustRemainInstalled(extension,
                                                               error);
}

bool VivaldiStandardManagementPolicyProvider::ShouldForceUninstall(
    const Extension* extension,
    std::u16string* error) const {
  return StandardManagementPolicyProvider::ShouldForceUninstall(extension,
                                                                error);
}

}  // namespace extensions
