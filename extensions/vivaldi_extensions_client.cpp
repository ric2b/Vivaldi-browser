// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/vivaldi_extensions_client.h"

#include "extensions/common/features/json_feature_provider_source.h"
#include "extensions/common/permissions/permissions_info.h"

#include "extensions/schema/generated_schemas.h"
#include "vivaldi/grit/vivaldi_extension_resources.h"

namespace extensions {

static base::LazyInstance<VivaldiExtensionsClient> g_client =
    LAZY_INSTANCE_INITIALIZER;

VivaldiExtensionsClient::VivaldiExtensionsClient()
 : vivaldi_api_permissions_(VivaldiAPIPermissions()) {
}

VivaldiExtensionsClient::~VivaldiExtensionsClient(){
}

void VivaldiExtensionsClient::Initialize() {
  ChromeExtensionsClient::Initialize();

  // Set up permissions.
  PermissionsInfo::GetInstance()->AddProvider(vivaldi_api_permissions_);
}

bool
VivaldiExtensionsClient::IsAPISchemaGenerated(const std::string& name) const {
    return ChromeExtensionsClient::IsAPISchemaGenerated(name) ||
        vivaldi::VivaldiGeneratedSchemas::IsGenerated(name);
}

base::StringPiece
VivaldiExtensionsClient::GetAPISchema(const std::string& name) const {
  if (vivaldi::VivaldiGeneratedSchemas::IsGenerated(name))
    return vivaldi::VivaldiGeneratedSchemas::Get(name);

  return ChromeExtensionsClient::GetAPISchema(name);
}


scoped_ptr<JSONFeatureProviderSource>
VivaldiExtensionsClient::CreateFeatureProviderSource(
    const std::string& name) const {
  scoped_ptr<JSONFeatureProviderSource> source(
      ChromeExtensionsClient::CreateFeatureProviderSource(name));
  DCHECK(source);

  if (name == "api") {
    source->LoadJSON(IDR_VIVALDI_EXTENSION_API_FEATURES);
  } else if (name == "manifest") {
    source->LoadJSON(IDR_VIVALDI_EXTENSION_MANIFEST_FEATURES);
  } else if (name == "permission") {
    source->LoadJSON(IDR_VIVALDI_EXTENSION_PERMISSION_FEATURES);
  } else if (name == "behavior") {
  } else {
    NOTREACHED();
    source.reset();
  }
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

} // namespace extensions
