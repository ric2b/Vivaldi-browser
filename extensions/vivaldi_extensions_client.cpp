// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/vivaldi_extensions_client.h"

#include <memory>
#include <string>
#include <vector>

#include "extensions/common/alias.h"
#include "extensions/common/extensions_api_provider.h"
#include "extensions/common/features/json_feature_provider_source.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/permissions/vivaldi_api_permissions.h"
#include "extensions/schema/generated_schemas.h"
#include "vivaldi/grit/vivaldi_extension_resources.h"

namespace extensions {

namespace {

std::vector<Alias> GetVivaldiPermissionAliases() {
  // In alias constructor, first value is the alias name; second value is the
  // real name. See also alias.h.
  return std::vector<Alias>();
}

class VivaldiExtensionsAPIProvider : public ExtensionsAPIProvider {
 public:
  VivaldiExtensionsAPIProvider() {}
  ~VivaldiExtensionsAPIProvider() override {}

  // ExtensionsAPIProvider:
  void AddAPIFeatures(FeatureProvider* provider) override {}

  void AddManifestFeatures(FeatureProvider* provider) override {}

  void AddPermissionFeatures(FeatureProvider* provider) override {}

  void AddBehaviorFeatures(FeatureProvider* provider) override {}

  void AddAPIJSONSources(JSONFeatureProviderSource* json_source) override {
    json_source->LoadJSON(IDR_VIVALDI_EXTENSION_API_FEATURES);
  }

  bool IsAPISchemaGenerated(const std::string& name) override {
    return vivaldi::VivaldiGeneratedSchemas::IsGenerated(name);
  }

  base::StringPiece GetAPISchema(const std::string& name) override {
    return vivaldi::VivaldiGeneratedSchemas::Get(name);
  }

  void AddPermissionsProviders(PermissionsInfo* permissions_info) override {
    permissions_info->AddProvider(vivaldi_api_permissions_,
                                  GetVivaldiPermissionAliases());
  }

  void RegisterManifestHandlers() override {}

 private:
  const VivaldiAPIPermissions vivaldi_api_permissions_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiExtensionsAPIProvider);
};

}  // namespace

VivaldiExtensionsClient::VivaldiExtensionsClient() {
  AddAPIProvider(std::make_unique<VivaldiExtensionsAPIProvider>());
}

VivaldiExtensionsClient::~VivaldiExtensionsClient() {}

}  // namespace extensions
