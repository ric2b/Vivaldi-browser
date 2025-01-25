// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_VIVALDI_STANDARD_MANAGEMENT_POLICY_PROVIDER_H_
#define EXTENSIONS_VIVALDI_STANDARD_MANAGEMENT_POLICY_PROVIDER_H_

#include <string>

#include "chrome/browser/extensions/standard_management_policy_provider.h"

namespace extensions {

class Extension;
class ExtensionManagement;

// Override standard extension management policies for Vivaldi's extension id
class VivaldiStandardManagementPolicyProvider
    : public StandardManagementPolicyProvider {
 public:
  explicit VivaldiStandardManagementPolicyProvider(
      ExtensionManagement* settings,
      Profile* profile);

  ~VivaldiStandardManagementPolicyProvider() override;

  // ManagementPolicy::Provider implementation.
  bool UserMayLoad(const Extension* extension,
                   std::u16string* error) const override;
  bool UserMayInstall(const Extension* extension,
                      std::u16string* error) const override;
  bool UserMayModifySettings(const Extension* extension,
                             std::u16string* error) const override;
  bool ExtensionMayModifySettings(const Extension* source_extension,
                                  const Extension* extension,
                                  std::u16string* error) const override;
  bool MustRemainEnabled(const Extension* extension,
                         std::u16string* error) const override;
  bool MustRemainDisabled(const Extension* extension,
                          disable_reason::DisableReason* reason) const override;
  bool MustRemainInstalled(const Extension* extension,
                           std::u16string* error) const override;
  bool ShouldForceUninstall(const Extension* extension,
                            std::u16string* error) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_STANDARD_MANAGEMENT_POLICY_PROVIDER_H_
