// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_AUTO_UPDATE_AUTO_UPDATE_API_H_
#define EXTENSIONS_API_AUTO_UPDATE_AUTO_UPDATE_API_H_

#include <memory>

#include "extensions/browser/extension_function.h"

#include "extensions/schema/autoupdate.h"

namespace extensions {

class AutoUpdateAPI {
 public:
static void Init() {
#ifdef OS_WIN
    InitUpgradeDetection();
#endif
  }

  static void Shutdown() {
#ifdef OS_WIN
    ShutdownUpgradeDetection();
#endif
  }

  static void SendDidFindValidUpdate(const std::string& url);
  static void SendWillDownloadUpdate();
  static void SendDidDownloadUpdate();
  static void SendWillInstallUpdateOnQuit();
  static void SendUpdaterWillRelaunchApplication();
  static void SendUpdaterDidRelaunchApplication();
  static void SendDidAbortWithError();

 private:
#ifdef OS_WIN
  static void InitUpgradeDetection();
  static void ShutdownUpgradeDetection();
#endif
};

class AutoUpdateCheckForUpdatesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autoUpdate.checkForUpdates",
                             AUTOUPDATE_CHECKFORUPDATES)
  AutoUpdateCheckForUpdatesFunction() = default;

 private:
  ~AutoUpdateCheckForUpdatesFunction() override = default;
  void DeliverResult();

  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(AutoUpdateCheckForUpdatesFunction);
};

class AutoUpdateIsUpdateNotifierEnabledFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autoUpdate.isUpdateNotifierEnabled",
                             AUTOUPDATE_ISUPDATENOTIFIERENABLED)
  AutoUpdateIsUpdateNotifierEnabledFunction() = default;

 private:
  ~AutoUpdateIsUpdateNotifierEnabledFunction() override = default;
  void DeliverResult(bool enabled);

  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(AutoUpdateIsUpdateNotifierEnabledFunction);
};

class AutoUpdateEnableUpdateNotifierFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autoUpdate.enableUpdateNotifier",
                             AUTOUPDATE_ENABLEUPDATENOTIFIER)
  AutoUpdateEnableUpdateNotifierFunction() = default;

 private:
  ~AutoUpdateEnableUpdateNotifierFunction() override = default;
  void DeliverResult(bool success);

  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(AutoUpdateEnableUpdateNotifierFunction);
};

class AutoUpdateDisableUpdateNotifierFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autoUpdate.disableUpdateNotifier",
                             AUTOUPDATE_DISABLEUPDATENOTIFIER)
  AutoUpdateDisableUpdateNotifierFunction() = default;

 private:
  ~AutoUpdateDisableUpdateNotifierFunction() override = default;
  void DeliverResult(bool success);

  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(AutoUpdateDisableUpdateNotifierFunction);
};

class AutoUpdateGetAutoInstallUpdatesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autoUpdate.getAutoInstallUpdates",
                             AUTOUPDATE_GETAUTOINSTALLUPDATES)
  AutoUpdateGetAutoInstallUpdatesFunction() = default;

 private:
  ~AutoUpdateGetAutoInstallUpdatesFunction() override = default;
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(AutoUpdateGetAutoInstallUpdatesFunction);
};

class AutoUpdateSetAutoInstallUpdatesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autoUpdate.setAutoInstallUpdates",
                             AUTOUPDATE_SETAUTOINSTALLUPDATES)
  AutoUpdateSetAutoInstallUpdatesFunction() = default;

 private:
  ~AutoUpdateSetAutoInstallUpdatesFunction() override = default;
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(AutoUpdateSetAutoInstallUpdatesFunction);
};

}  // namespace extensions
#endif  // EXTENSIONS_API_AUTO_UPDATE_AUTO_UPDATE_API_H_
