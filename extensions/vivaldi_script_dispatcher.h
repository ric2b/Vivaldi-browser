// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_VIVALDI_SCRIPT_DISPATCHER_H_
#define EXTENSIONS_VIVALDI_SCRIPT_DISPATCHER_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

namespace extensions {
class ModuleSystem;
class ScriptContext;
}  // namespace extensions

namespace vivaldi {
void VivaldiAddScriptResources(
    std::vector<std::pair<const char*, int> >* resources);
void VivaldiAddRequiredModules(extensions::ScriptContext* context,
                               extensions::ModuleSystem* module_system);
}  // namespace vivaldi

#endif  // EXTENSIONS_VIVALDI_SCRIPT_DISPATCHER_H_
