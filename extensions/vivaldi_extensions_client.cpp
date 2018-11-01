// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/vivaldi_extensions_client.h"

#include <memory>
#include <string>
#include <vector>

#include "extensions/common/alias.h"
#include "extensions/common/features/json_feature_provider_source.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/schema/generated_schemas.h"
#include "vivaldi/grit/vivaldi_extension_resources.h"

namespace extensions {

namespace {

std::vector<Alias> GetVivaldiPermissionAliases() {
  // In alias constructor, first value is the alias name; second value is the
  // real name. See also alias.h.
  return std::vector<Alias>();
}
}

static base::LazyInstance<VivaldiExtensionsClient>::DestructorAtExit g_client =
    LAZY_INSTANCE_INITIALIZER;

VivaldiExtensionsClient::VivaldiExtensionsClient()
    : vivaldi_api_permissions_(VivaldiAPIPermissions()) {}

VivaldiExtensionsClient::~VivaldiExtensionsClient() {}

void VivaldiExtensionsClient::Initialize() {
  ChromeExtensionsClient::Initialize();

  // Set up permissions.
  PermissionsInfo::GetInstance()->AddProvider(vivaldi_api_permissions_,
                                              GetVivaldiPermissionAliases());
}

bool VivaldiExtensionsClient::IsAPISchemaGenerated(
    const std::string& name) const {
  return ChromeExtensionsClient::IsAPISchemaGenerated(name) ||
         vivaldi::VivaldiGeneratedSchemas::IsGenerated(name);
}

base::StringPiece VivaldiExtensionsClient::GetAPISchema(
    const std::string& name) const {
  if (vivaldi::VivaldiGeneratedSchemas::IsGenerated(name))
    return vivaldi::VivaldiGeneratedSchemas::Get(name);

  return ChromeExtensionsClient::GetAPISchema(name);
}

std::unique_ptr<JSONFeatureProviderSource>
VivaldiExtensionsClient::CreateAPIFeatureSource() const {
  std::unique_ptr<JSONFeatureProviderSource> source(
      ChromeExtensionsClient::CreateAPIFeatureSource());
  DCHECK(source);

  source->LoadJSON(IDR_VIVALDI_EXTENSION_API_FEATURES);
  return source;
}

/*static*/
ChromeExtensionsClient* VivaldiExtensionsClient::GetInstance() {
  return g_client.Pointer();
}

// NOTE(yngve) Run this function **before**
// ChromeExtensionsClient::GetInstance() is called
// Hack to make sure ChromeExtensionsClient::GetInstance()
// calls the VivaldiExtensionsClient::GetInstance() instead
/*static*/
void VivaldiExtensionsClient::RegisterVivaldiExtensionsClient() {
  ChromeExtensionsClient::RegisterAlternativeGetInstance(
      VivaldiExtensionsClient::GetInstance);
}

}  // namespace extensions
