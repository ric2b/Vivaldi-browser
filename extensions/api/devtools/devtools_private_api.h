// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_DEVTOOLS_DEVTOOLS_PRIVATE_API_H_
#define EXTENSIONS_API_DEVTOOLS_DEVTOOLS_PRIVATE_API_H_

#include "extensions/browser/extension_function.h"
#include "extensions/schema/devtools_private.h"

namespace extensions {

class DevtoolsPrivateGetDockingStateSizesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("devtoolsPrivate.getDockingStateSizes",
                             DEVTOOLSPRIVATE_GETDOCKINGSTATESIZES)

  DevtoolsPrivateGetDockingStateSizesFunction() = default;

 private:
  ~DevtoolsPrivateGetDockingStateSizesFunction() override = default;

  ResponseAction Run() override;
};

class DevtoolsPrivateCloseDevtoolsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("devtoolsPrivate.closeDevtools",
                             DEVTOOLSPRIVATE_CLOSEDEVTOOLS)

  DevtoolsPrivateCloseDevtoolsFunction() = default;

 private:
  ~DevtoolsPrivateCloseDevtoolsFunction() override = default;

  ResponseAction Run() override;
};

class DevtoolsPrivateToggleDevtoolsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("devtoolsPrivate.toggleDevtools",
                             DEVTOOLSPRIVATE_TOGGLEDEVTOOLS)
  DevtoolsPrivateToggleDevtoolsFunction() = default;

 private:
  ~DevtoolsPrivateToggleDevtoolsFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_API_DEVTOOLS_DEVTOOLS_PRIVATE_API_H_
