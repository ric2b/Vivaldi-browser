// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_AUTO_UPDATE_AUTO_UPDATE_API_H_
#define EXTENSIONS_API_AUTO_UPDATE_AUTO_UPDATE_API_H_

#include <memory>

#include "base/synchronization/waitable_event.h"
#include "extensions/browser/extension_function.h"
#include "extensions/schema/autoupdate.h"

namespace extensions {

class AutoUpdateCheckForUpdatesFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autoUpdate.checkForUpdates",
                             AUTOUPDATE_CHECKFORUPDATES)
  AutoUpdateCheckForUpdatesFunction() = default;

 private:
  ~AutoUpdateCheckForUpdatesFunction() override = default;
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(AutoUpdateCheckForUpdatesFunction);
};

class AutoUpdateIsUpdateNotifierEnabledFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autoUpdate.isUpdateNotifierEnabled",
                             AUTOUPDATE_ISUPDATENOTIFIERENABLED)
  AutoUpdateIsUpdateNotifierEnabledFunction() = default;

 private:
  ~AutoUpdateIsUpdateNotifierEnabledFunction() override = default;
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(AutoUpdateIsUpdateNotifierEnabledFunction);
};

class AutoUpdateEnableUpdateNotifierFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autoUpdate.enableUpdateNotifier",
                             AUTOUPDATE_ENABLEUPDATENOTIFIER)
  AutoUpdateEnableUpdateNotifierFunction() = default;

 private:
  ~AutoUpdateEnableUpdateNotifierFunction() override = default;
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(AutoUpdateEnableUpdateNotifierFunction);
};

class AutoUpdateDisableUpdateNotifierFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autoUpdate.disableUpdateNotifier",
                             AUTOUPDATE_DISABLEUPDATENOTIFIER)
  AutoUpdateDisableUpdateNotifierFunction() = default;

 private:
  ~AutoUpdateDisableUpdateNotifierFunction() override = default;
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(AutoUpdateDisableUpdateNotifierFunction);
};

}  // namespace extensions
#endif  // EXTENSIONS_API_AUTO_UPDATE_AUTO_UPDATE_API_H_
