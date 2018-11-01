// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_DEVTOOLS_DEVTOOLS_PRIVATE_API_H_
#define EXTENSIONS_API_DEVTOOLS_DEVTOOLS_PRIVATE_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "extensions/schema/devtools_private.h"

namespace extensions {

class DevtoolsPrivateGetDockingStateSizesFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("devtoolsPrivate.getDockingStateSizes",
                             DEVTOOLSPRIVATE_GETDOCKINGSTATESIZES);

  DevtoolsPrivateGetDockingStateSizesFunction() = default;

 protected:
  ~DevtoolsPrivateGetDockingStateSizesFunction() override = default;

 private:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(DevtoolsPrivateGetDockingStateSizesFunction);
};

class DevtoolsPrivateCloseDevtoolsFunction
  : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("devtoolsPrivate.closeDevtools",
  DEVTOOLSPRIVATE_CLOSEDEVTOOLS);

  DevtoolsPrivateCloseDevtoolsFunction() = default;

 protected:
  ~DevtoolsPrivateCloseDevtoolsFunction() override = default;

 private:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(DevtoolsPrivateCloseDevtoolsFunction);
};

class DevtoolsPrivateToggleDevtoolsFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("devtoolsPrivate.toggleDevtools",
                             DEVTOOLSPRIVATE_TOGGLEDEVTOOLS)
  DevtoolsPrivateToggleDevtoolsFunction() = default;

 protected:
  ~DevtoolsPrivateToggleDevtoolsFunction() override = default;
  // ExtensionFunction:
  bool RunAsync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DevtoolsPrivateToggleDevtoolsFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_DEVTOOLS_DEVTOOLS_PRIVATE_API_H_
