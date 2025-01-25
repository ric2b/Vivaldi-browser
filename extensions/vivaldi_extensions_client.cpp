// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/vivaldi_extensions_client.h"

#include <memory>
#include <string>
#include <vector>

#include "chrome/common/apps/platform_apps/chrome_apps_api_provider.h"
#include "extensions/common/alias.h"
#include "extensions/common/extensions_api_provider.h"
#include "extensions/common/features/json_feature_provider_source.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/permissions/vivaldi_api_permissions.h"
#include "extensions/schema/generated_schemas.h"
#include "vivaldi/grit/vivaldi_extension_resources.h"

namespace extensions {

namespace {

class VivaldiExtensionsAPIProvider : public ExtensionsAPIProvider {
 public:
  VivaldiExtensionsAPIProvider() = default;
  ~VivaldiExtensionsAPIProvider() override = default;
  VivaldiExtensionsAPIProvider(const VivaldiExtensionsAPIProvider&) = delete;
  VivaldiExtensionsAPIProvider& operator=(const VivaldiExtensionsAPIProvider&) =
      delete;

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

  std::string_view GetAPISchema(const std::string& name) override {
    return vivaldi::VivaldiGeneratedSchemas::Get(name);
  }

  void RegisterPermissions(PermissionsInfo* permissions_info) override {
    permissions_info->RegisterPermissions(
        vivaldi_api_permissions::GetPermissionInfos(),
        base::span<const extensions::Alias>());
  }

  void RegisterManifestHandlers() override {}
};

}  // namespace

VivaldiExtensionsClient::VivaldiExtensionsClient() {
  AddAPIProvider(std::make_unique<VivaldiExtensionsAPIProvider>());
  AddAPIProvider(std::make_unique<chrome_apps::ChromeAppsAPIProvider>());
}

VivaldiExtensionsClient::~VivaldiExtensionsClient() {}

}  // namespace extensions
