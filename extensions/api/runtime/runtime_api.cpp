// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/runtime/runtime_api.h"

#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/browser_commands.h"


namespace extensions {

// Action function used by RuntimePrivateExitFunction ::Run()
void PerformApplicationShutdown();

ExtensionFunction::ResponseAction RuntimePrivateExitFunction ::Run() {
  PerformApplicationShutdown();
  return RespondNow(NoArguments());
}

} // namespace extensions
