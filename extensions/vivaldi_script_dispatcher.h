// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include <set>
#include <string>
#include <utility>
#include <vector>

namespace extensions {
class ModuleSystem;
class ScriptContext;
} // namespace extensions

namespace vivaldi {
void VivaldiAddScriptResources(std::vector<std::pair<std::string, int> > &resources);
void VivaldiAddRequiredModules(extensions::ScriptContext* context, extensions::ModuleSystem* module_system);
} // namespace vivaldi
