// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_RUNTIME_RUNTIME_API_H
#define EXTENSIONS_RUNTIME_RUNTIME_API_H

#include "extensions/browser/extension_function.h"

namespace extensions {

class RuntimePrivateExitFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.exit", RUNTIME_EXIT)

 protected:
  ~RuntimePrivateExitFunction () override {}
  ResponseAction Run() override;
};

} // namespace extensions

#endif // EXTENSIONS_RUNTIME_RUNTIME_API_H
