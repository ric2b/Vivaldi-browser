// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_AUTO_UPDATE_AUTO_UPDATE_API_H_
#define EXTENSIONS_API_AUTO_UPDATE_AUTO_UPDATE_API_H_

#include <memory>

#include "base/version.h"
#include "build/build_config.h"
#include "extensions/browser/extension_function.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#include "extensions/api/auto_update/auto_update_status.h"
#include "vivaldi/extensions/schema/autoupdate.h"

namespace extensions {

class AutoUpdateAPI {
 public:
  static void Init() {
#if BUILDFLAG(IS_WIN)
    InitUpgradeDetection();
#endif
  }

  static void Shutdown() {
#if BUILDFLAG(IS_WIN)
    ShutdownUpgradeDetection();
#endif
  }

  static void SendDidFindValidUpdate(const std::string& url,
                                     const base::Version& version);
  static void SendUpdaterDidNotFindUpdate();
  static void SendWillDownloadUpdate(const base::Version& version);
  static void SendDidDownloadUpdate(const base::Version& version);
  static void SendWillInstallUpdateOnQuit(const base::Version& version);
  static void SendUpdaterWillRelaunchApplication();
  static void SendUpdaterDidRelaunchApplication();
  static void SendDidAbortWithError(const std::string& error,
                                    const std::string& reason);

 private:
#if BUILDFLAG(IS_WIN)
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
};

class AutoUpdateInstallUpdateAndRestartFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autoUpdate.installUpdateAndRestart",
                             AUTOUPDATE_INSTALLUPDATEANDRESTART)
  AutoUpdateInstallUpdateAndRestartFunction() = default;

 private:
  ~AutoUpdateInstallUpdateAndRestartFunction() override = default;
  ResponseAction Run() override;
};

class AutoUpdateGetAutoInstallUpdatesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autoUpdate.getAutoInstallUpdates",
                             AUTOUPDATE_GETAUTOINSTALLUPDATES)
  AutoUpdateGetAutoInstallUpdatesFunction() = default;

 private:
  ~AutoUpdateGetAutoInstallUpdatesFunction() override = default;
  ResponseAction Run() override;
};

class AutoUpdateSetAutoInstallUpdatesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autoUpdate.setAutoInstallUpdates",
                             AUTOUPDATE_SETAUTOINSTALLUPDATES)
  AutoUpdateSetAutoInstallUpdatesFunction() = default;

 private:
  ~AutoUpdateSetAutoInstallUpdatesFunction() override = default;
  ResponseAction Run() override;
};

class AutoUpdateGetLastCheckTimeFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autoUpdate.getLastCheckTime",
                             AUTOUPDATE_GETLASTCHECKTIME)
  AutoUpdateGetLastCheckTimeFunction() = default;

 private:
  ~AutoUpdateGetLastCheckTimeFunction() override = default;
  ResponseAction Run() override;
};

class AutoUpdateGetUpdateStatusFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autoUpdate.getUpdateStatus",
                             AUTOUPDATE_GETUPDATESTATUS)

  AutoUpdateGetUpdateStatusFunction() = default;

  // Wrap the status for delivery to JS. Use nullopt for status on errors.
  void SendResult(absl::optional<AutoUpdateStatus> status,
                  std::string version,
                  std::string release_notes_url);

 private:
  ~AutoUpdateGetUpdateStatusFunction() override = default;
  ResponseAction Run() override;
};

class AutoUpdateHasAutoUpdatesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autoUpdate.hasAutoUpdates",
                             AUTOUPDATE_HASAUTOUPDATES)
  AutoUpdateHasAutoUpdatesFunction() = default;

 private:
  ~AutoUpdateHasAutoUpdatesFunction() override = default;
  ResponseAction Run() override;

  bool HasAutoUpdates();
};

}  // namespace extensions
#endif  // EXTENSIONS_API_AUTO_UPDATE_AUTO_UPDATE_API_H_
