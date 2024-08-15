// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/auto_update/auto_update_api.h"

#include "extensions/schema/autoupdate.h"
#include "extensions/tools/vivaldi_tools.h"

#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/files/file_path.h"
#include "base/task/thread_pool.h"

namespace auto_update = extensions::vivaldi::auto_update;

namespace extensions {

namespace {

std::string GetVersionString(const base::Version& version) {
  if (!version.IsValid())
    return std::string();
  return version.GetString();
}

}  // namespace

AutoUpdateAPI::AutoUpdateAPI(content::BrowserContext* context)
    : browser_context_(context) {
  LOG(INFO) << "AutoUpdateAPI::Init";
#if BUILDFLAG(IS_WIN)
  InitUpgradeDetection();
#endif

#if BUILDFLAG(IS_LINUX)
  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_VISIBLE, base::MayBlock()});

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(base::BindOnce(
          [](AutoUpdateAPI* api) {
            base::FilePath executable_path;
            if (base::PathService::Get(base::FILE_EXE, &executable_path)) {
              LOG(INFO) << "got executable";
              api->executable_file_watcher_ =
                  std::make_unique<base::FilePathWatcher>();
              api->executable_file_watcher_->Watch(
                  executable_path, base::FilePathWatcher::Type::kNonRecursive,
                  base::BindRepeating([](const base::FilePath&, bool) {
                    content::GetUIThreadTaskRunner()->PostTask(
                        FROM_HERE, base::BindOnce([]() {
                          LOG(INFO) << "Binary changed";
                          ::vivaldi::BroadcastEventToAllProfiles(
                              auto_update::OnWillInstallUpdateOnQuit::
                                  kEventName,
                              auto_update::OnWillInstallUpdateOnQuit::Create(
                                  ""));
                        }));
                  }));
            } else {
              LOG(INFO) << "executable not found";
            }
          },
          this)));
#endif
}

AutoUpdateAPI::~AutoUpdateAPI() {}

void AutoUpdateAPI::Shutdown() {
#if BUILDFLAG(IS_LINUX)
  task_runner_->PostTask(FROM_HERE, base::BindOnce(
                                        [](AutoUpdateAPI* api) {
                                          api->executable_file_watcher_.reset();
                                        },
                                        this));
#endif
#if BUILDFLAG(IS_WIN)
  ShutdownUpgradeDetection();
#endif
}

BrowserContextKeyedAPIFactory<AutoUpdateAPI>*
AutoUpdateAPI::GetFactoryInstance() {
  static base::LazyInstance<
      BrowserContextKeyedAPIFactory<AutoUpdateAPI>>::DestructorAtExit factory =
      LAZY_INSTANCE_INITIALIZER;
  return factory.Pointer();
}

/* static */
void AutoUpdateAPI::SendDidFindValidUpdate(const std::string& url,
                                           const base::Version& version) {
  ::vivaldi::BroadcastEventToAllProfiles(
      auto_update::OnDidFindValidUpdate::kEventName,
      auto_update::OnDidFindValidUpdate::Create(url,
                                                GetVersionString(version)));
}

/* static */
void AutoUpdateAPI::SendUpdaterDidNotFindUpdate() {
  ::vivaldi::BroadcastEventToAllProfiles(
      auto_update::OnUpdaterDidNotFindUpdate::kEventName);
}

/* static */
void AutoUpdateAPI::SendWillDownloadUpdate(const base::Version& version) {
  ::vivaldi::BroadcastEventToAllProfiles(
      auto_update::OnWillDownloadUpdate::kEventName,
      auto_update::OnWillDownloadUpdate::Create(GetVersionString(version)));
}

/* static */
void AutoUpdateAPI::SendDidDownloadUpdate(const base::Version& version) {
  ::vivaldi::BroadcastEventToAllProfiles(
      auto_update::OnDidDownloadUpdate::kEventName,
      auto_update::OnDidDownloadUpdate::Create(GetVersionString(version)));
}

/* static */
void AutoUpdateAPI::SendWillInstallUpdateOnQuit(const base::Version& version) {
  std::string version_string =
      version.IsValid() ? version.GetString() : std::string();
  LOG(INFO) << "Pending update, version=" << version_string;
  ::vivaldi::BroadcastEventToAllProfiles(
      auto_update::OnWillInstallUpdateOnQuit::kEventName,
      auto_update::OnWillInstallUpdateOnQuit::Create(version_string));
}

/* static */
void AutoUpdateAPI::SendUpdaterWillRelaunchApplication() {
  ::vivaldi::BroadcastEventToAllProfiles(
      auto_update::OnUpdaterWillRelaunchApplication::kEventName);
}

/* static */
void AutoUpdateAPI::SendUpdaterDidRelaunchApplication() {
  ::vivaldi::BroadcastEventToAllProfiles(
      auto_update::OnUpdaterDidRelaunchApplication::kEventName);
}

/* static */
void AutoUpdateAPI::SendDidAbortWithError(const std::string& desc,
                                          const std::string& reason) {
  ::vivaldi::BroadcastEventToAllProfiles(
      auto_update::OnDidAbortWithError::kEventName,
      auto_update::OnDidAbortWithError::Create(desc, reason));
}

void AutoUpdateGetUpdateStatusFunction::SendResult(
    absl::optional<AutoUpdateStatus> status,
    std::string version,
    std::string release_notes_url) {
  namespace Results = vivaldi::auto_update::GetUpdateStatus::Results;

  vivaldi::auto_update::UpdateOperationStatus status_object;
  if (status) {
    switch (*status) {
      case AutoUpdateStatus::kNoUpdate:
        status_object.status =
            vivaldi::auto_update::UpdateOperationStatusEnum::kNoUpdate;
        break;
      case AutoUpdateStatus::kDidAbortWithError:
        status_object.status = vivaldi::auto_update::UpdateOperationStatusEnum::kDidAbortWithError;
        break;
      case AutoUpdateStatus::kDidFindValidUpdate:
        status_object.status = vivaldi::auto_update::UpdateOperationStatusEnum::kDidFindValidUpdate;
        break;
      case AutoUpdateStatus::kWillDownloadUpdate:
        status_object.status = vivaldi::auto_update::UpdateOperationStatusEnum::kWillDownloadUpdate;
        break;
      case AutoUpdateStatus::kDidDownloadUpdate:
        status_object.status = vivaldi::auto_update::UpdateOperationStatusEnum::kDidDownloadUpdate;
        break;
      case AutoUpdateStatus::kWillInstallUpdateOnQuit:
        status_object.status = vivaldi::auto_update::UpdateOperationStatusEnum::kWillInstallUpdateOnQuit;
        break;
      case AutoUpdateStatus::kUpdaterDidRelaunchApplication:
        status_object.status = vivaldi::auto_update::UpdateOperationStatusEnum::kUpdaterDidRelaunchApplication;
        break;
    }
  }
  status_object.version = std::move(version);
  status_object.release_notes_url = std::move(release_notes_url);

  Respond(ArgumentList(Results::Create(status_object)));
}

ExtensionFunction::ResponseAction AutoUpdateHasAutoUpdatesFunction::Run() {
  namespace Results = vivaldi::auto_update::HasAutoUpdates::Results;

  bool has_auto_updates = HasAutoUpdates();
  return RespondNow(ArgumentList(Results::Create(has_auto_updates)));
}

}  // namespace extensions
