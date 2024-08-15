// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_VIVALDI_SCRIPT_DISPATCHER_H_
#define EXTENSIONS_VIVALDI_SCRIPT_DISPATCHER_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/script_context.h"

namespace extensions {
class ModuleSystem;
class ScriptContext;
class ResourceBundleSourceMap;
}  // namespace extensions

namespace vivaldi {

void VivaldiRegisterNativeHandler(extensions::ModuleSystem* module_system,
                                  extensions::ScriptContext* context);
void VivaldiAddScriptResources(extensions::ResourceBundleSourceMap* source_map);
void VivaldiAddRequiredModules(extensions::ScriptContext* context,
                               extensions::ModuleSystem* module_system);
}  // namespace vivaldi

#endif  // EXTENSIONS_VIVALDI_SCRIPT_DISPATCHER_H_
