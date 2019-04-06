// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/vivaldi_extensions_init.h"

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "extensions/api/generated_api_registration.h"
#include "extensions/browser/extension_function_registry.h"

namespace extensions {

static base::LazyInstance<BrowserContextKeyedAPIFactory<VivaldiExtensionInit>>::
    DestructorAtExit g_factory = LAZY_INSTANCE_INITIALIZER;

VivaldiExtensionInit::VivaldiExtensionInit(content::BrowserContext* context) {
  ExtensionFunctionRegistry* registry =
      &ExtensionFunctionRegistry::GetInstance();

  // Generated APIs from Vivaldi.
  extensions::vivaldi::VivaldiGeneratedFunctionRegistry::RegisterAll(registry);
}

VivaldiExtensionInit::~VivaldiExtensionInit() {}

VivaldiExtensionInit* VivaldiExtensionInit::Get(
    content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<VivaldiExtensionInit>::Get(context);
}

BrowserContextKeyedAPIFactory<VivaldiExtensionInit>*
VivaldiExtensionInit::GetFactoryInstance() {
  return g_factory.Pointer();
}

}  // namespace extensions
