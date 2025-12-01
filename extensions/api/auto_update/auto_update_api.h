// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_AUTO_UPDATE_AUTO_UPDATE_API_H_
#define EXTENSIONS_API_AUTO_UPDATE_AUTO_UPDATE_API_H_

#include <memory>

#include "base/task/cancelable_task_tracker.h"
#include "base/version.h"
#include "build/build_config.h"
#include "extensions/browser/extension_function.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#include "base/files/file_path_watcher.h"
#include "extensions/api/auto_update/auto_update_status.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "update/update_model_observer.h"
#include "vivaldi/extensions/schema/autoupdate.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/scoped_observation.h"
#include "components/component_updater/component_updater_service.h"
#endif

// Forward for friendship.
namespace drm_helper {
class DRMContentTabHelper;
}  // namespace drm_helper

class Profile;
using update::UpdateModelObserver;
using update::UpdateService;
namespace extensions {

struct FilePathResult {
  std::string executable_path;
  std::string profile_path;
  std::string full_os_version;
};

// Observes UpdateModel and then routes notifications as
// events to the extension system.
class UpdateEventRouter : public UpdateModelObserver {
 public:
  explicit UpdateEventRouter(Profile* profile, UpdateService* update_service);
  ~UpdateEventRouter() override;

 private:
  // Helper to actually dispatch an event to extension listeners.
  void DispatchEvent(Profile* profile,
                     const std::string& event_name,
                     base::Value::List event_args);

  // UpdateModelObserver
  void OnUpdateProgress(UpdateService* service,
                        const AutoUpdateStatus& status,
                        const std::string& reason,
                        const int progress) override;

  const raw_ptr<Profile> profile_;
  base::ScopedObservation<update::UpdateService, UpdateModelObserver>
      update_service_observation_{this};
};

class AutoUpdateAPI : public BrowserContextKeyedAPI,
                      public update_client::UpdateClient::Observer,
                      public EventRouter::Observer {
 public:
  explicit AutoUpdateAPI(content::BrowserContext* context);
  ~AutoUpdateAPI() override;

  void Shutdown() override;

  // Handles cleanup of ENVIRONMENT variables before restarting the browser
  static void HandleRestartPreconditions();

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<AutoUpdateAPI>* GetFactoryInstance();

  static void SendDidFindValidUpdate(const std::string& url,
                                     const base::Version& version);
  static void SendUpdaterDidNotFindUpdate(const std::string& reason);
  static void SendWillDownloadUpdate(const base::Version& version);
  static void SendDidDownloadUpdate(const base::Version& version);
  static void SendWillInstallUpdateOnQuit(const base::Version& version);
  static void SendNeedRestartToReloadCodecs();
  static void SendUpdaterWillRelaunchApplication();
  static void SendUpdaterDidRelaunchApplication();
  static void SendDidAbortWithError(const std::string& error,
                                    const std::string& reason);

  static void SendUpdateProgress(const AutoUpdateStatus& status,
                                 const std::string& version,
                                 const int progress);
  static void SendUpdateFinished();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

#if !BUILDFLAG(IS_ANDROID)
  bool WasWidevineAvailable() { return widevine_was_available_; }
#endif

 private:
  friend class BrowserContextKeyedAPIFactory<AutoUpdateAPI>;
  friend class drm_helper::DRMContentTabHelper;

#if BUILDFLAG(IS_WIN)
  static void InitUpgradeDetection();
  static void ShutdownUpgradeDetection();
#endif

  static const char* service_name() { return "AutoUpdateAPI"; }
  const raw_ptr<content::BrowserContext> browser_context_;

#if !BUILDFLAG(IS_ANDROID)
  void InitWidevineMonitoring();
  void HandleWidevineRequested();
  void HandleWidevineUpdated();

  // For linux this handles restart notification.
  void HandleRequestedWidevineUpdate();

  // True if we started browaer with widevine support.
  bool widevine_was_available_ = false;
  // True if any tab tried to play widevine protected content.
  bool widevine_was_requested_ = false;
  // True if the Widevine module updated.
  bool widevine_was_updated_ = false;

  void OnEvent(const update_client::CrxUpdateItem& item) override;

  base::ScopedObservation<component_updater::ComponentUpdateService,
                          component_updater::ComponentUpdateService::Observer>
      observer_{this};
#endif

#if BUILDFLAG(IS_LINUX)
  void InitUpgradeDetection();
  void ShutdownUpgradeDetection();

  // handles environment for codec restart conditions
  // implemented in auto_update_api_linux.cc
  static void HandleCodecRestartPreconditions();

  std::unique_ptr<base::FilePathWatcher> executable_file_watcher_;
  std::unique_ptr<base::FilePathWatcher> ffmpeg_file_watcher_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
#endif

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<UpdateEventRouter> update_event_router_;
};

class UpdateAsyncFunction : public ExtensionFunction {
 public:
  UpdateAsyncFunction() = default;

 protected:
  Profile* GetProfile() const;
  ~UpdateAsyncFunction() override {}
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
  void SendResult(std::optional<AutoUpdateStatus> status,
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

class AutoUpdateNeedsCodecRestartFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autoUpdate.needsCodecRestart",
                             AUTOUPDATE_NEEDSCODECRESTART)
  AutoUpdateNeedsCodecRestartFunction() = default;

 private:
  ~AutoUpdateNeedsCodecRestartFunction() override = default;
  void DeliverResult(bool enabled);
  ResponseAction Run() override;
};

class AutoUpdateRunStartupChecksFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autoUpdate.runStartupChecks",
                             AUTOUPDATE_RUNSTARTUPCHECKS)
  AutoUpdateRunStartupChecksFunction() = default;

 private:
  ~AutoUpdateRunStartupChecksFunction() override = default;
  ResponseAction Run() override;
};

class AutoUpdateStartUpdateFunction : public UpdateAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autoUpdate.startUpdate", AUTOUPDATE_STARTUPDATE)
  AutoUpdateStartUpdateFunction() = default;

 private:
  ~AutoUpdateStartUpdateFunction() override = default;

  ResponseAction Run() override;
  void StartUpdateCB();
  base::CancelableTaskTracker task_tracker_;
};

class AutoUpdateGetAboutInfoFunction : public UpdateAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autoUpdate.getAboutInfo", AUTOUPDATE_GETABOUTINFO)
  AutoUpdateGetAboutInfoFunction() = default;

 private:
  ~AutoUpdateGetAboutInfoFunction() override = default;

  ResponseAction Run() override;
  base::CancelableTaskTracker task_tracker_;
};

class AutoUpdateGetAboutPathsInfoFunction : public UpdateAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autoUpdate.getAboutPathsInfo",
                             AUTOUPDATE_GETABOUTFILEPATHSINFO)
  AutoUpdateGetAboutPathsInfoFunction() = default;

 private:
  ~AutoUpdateGetAboutPathsInfoFunction() override = default;
  void OnFilePathCallback(base::RepeatingClosure done_closure,
                          const FilePathResult& paths);
  void OnVersion(base::RepeatingClosure done_closure,
                 const std::string& version);
  static std::string GetPlatformOSVersion();
  void OnDone();
  FilePathResult paths_result_;
  std::string os_full_version_;

  ResponseAction Run() override;
  base::CancelableTaskTracker task_tracker_;
};

}  // namespace extensions
#endif  // EXTENSIONS_API_AUTO_UPDATE_AUTO_UPDATE_API_H_
