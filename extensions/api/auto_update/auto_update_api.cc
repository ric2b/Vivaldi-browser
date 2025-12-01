// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/auto_update/auto_update_api.h"
#include "components/update_client/crx_update_item.h"

#include "extensions/schema/autoupdate.h"
#include "extensions/tools/vivaldi_tools.h"

#include "app/vivaldi_version_info.h"
#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/i18n/message_formatter.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/version/version_ui.h"
#include "chrome/grit/branded_strings.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "extensions/browser/event_router.h"
#include "ui/base/l10n/l10n_util.h"
#include "update/update_service_factory.h"
#include "v8/include/v8-version-string.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/browser_process.h"
#include "chrome/updater/constants.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/ui/webui/version/version_handler_win.h"
#include "chrome/browser/ui/webui/version/version_util_win.h"
#endif

namespace auto_update = extensions::vivaldi::auto_update;
namespace OnUpdateProgress = auto_update::OnUpdateProgress;

using auto_update::UpdateOperationStatusEnum;
using update::UpdateService;
using update::UpdateServiceFactory;

namespace extensions {

namespace {

#if !BUILDFLAG(IS_ANDROID)
static constexpr char kWidevineComponentID[] =
    "oimompecagnajdejgnnjijobebaeigek";
#endif

std::string GetVersionString(const base::Version& version) {
  if (!version.IsValid())
    return std::string();
  return version.GetString();
}

}  // namespace

UpdateEventRouter::UpdateEventRouter(Profile* profile,
                                     UpdateService* update_service)
    : profile_(profile) {
  DCHECK(profile);
  update_service_observation_.Observe(update_service);
}

UpdateEventRouter::~UpdateEventRouter() {}

UpdateOperationStatusEnum ToUpdateStatus(const AutoUpdateStatus& status) {
  if (status == AutoUpdateStatus::kNoUpdate) {
    return auto_update::UpdateOperationStatusEnum::kNoUpdate;
  } else if (status == AutoUpdateStatus::kDidAbortWithError) {
    return auto_update::UpdateOperationStatusEnum::kDidAbortWithError;
  } else if (status == AutoUpdateStatus::kDidFindValidUpdate) {
    return auto_update::UpdateOperationStatusEnum::kDidFindValidUpdate;
  } else if (status == AutoUpdateStatus::kWillDownloadUpdate) {
    return auto_update::UpdateOperationStatusEnum::kWillDownloadUpdate;
  } else if (status == AutoUpdateStatus::kDidDownloadUpdate) {
    return auto_update::UpdateOperationStatusEnum::kDidDownloadUpdate;
  } else if (status == AutoUpdateStatus::kWillInstallUpdateOnQuit) {
    return UpdateOperationStatusEnum::kWillInstallUpdateOnQuit;
  } else if (status == AutoUpdateStatus::kUpdaterDidRelaunchApplication) {
    return UpdateOperationStatusEnum::kUpdaterDidRelaunchApplication;
  } else if (status == AutoUpdateStatus::kError) {
    return auto_update::UpdateOperationStatusEnum::kError;
  } else {
    NOTREACHED();
  }
}

void UpdateEventRouter::OnUpdateProgress(UpdateService* service,
                                         const AutoUpdateStatus& status,
                                         const std::string& reason,
                                         const int progress) {
  DispatchEvent(
      profile_, OnUpdateProgress::kEventName,
      OnUpdateProgress::Create(ToUpdateStatus(status), reason, progress));
}

// Helper to actually dispatch an event to extension listeners.
void UpdateEventRouter::DispatchEvent(Profile* profile,
                                      const std::string& event_name,
                                      base::Value::List event_args) {
  if (profile && EventRouter::Get(profile)) {
    EventRouter* event_router = EventRouter::Get(profile);
    if (event_router) {
      event_router->BroadcastEvent(base::WrapUnique(
          new extensions::Event(extensions::events::VIVALDI_EXTENSION_EVENT,
                                event_name, std::move(event_args))));
    }
  }
}

AutoUpdateAPI::AutoUpdateAPI(content::BrowserContext* context)
    : browser_context_(context) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  InitUpgradeDetection();
#endif
#if !BUILDFLAG(IS_ANDROID)
  InitWidevineMonitoring();
#endif

  EventRouter* event_router = EventRouter::Get(context);
  event_router->RegisterObserver(this, OnUpdateProgress::kEventName);
}

AutoUpdateAPI::~AutoUpdateAPI() {}

void AutoUpdateAPI::Shutdown() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  ShutdownUpgradeDetection();
#endif
}

Profile* UpdateAsyncFunction::GetProfile() const {
  return Profile::FromBrowserContext(browser_context());
}

/* static */
void AutoUpdateAPI::HandleRestartPreconditions() {
#if BUILDFLAG(IS_LINUX)
  HandleCodecRestartPreconditions();
#endif  // BUILDFLAG(IS_LINUX)
}

BrowserContextKeyedAPIFactory<AutoUpdateAPI>*
AutoUpdateAPI::GetFactoryInstance() {
  static base::LazyInstance<
      BrowserContextKeyedAPIFactory<AutoUpdateAPI>>::DestructorAtExit factory =
      LAZY_INSTANCE_INITIALIZER;
  return factory.Pointer();
}

void AutoUpdateAPI::OnListenerAdded(const EventListenerInfo& details) {
  Profile* profile = Profile::FromBrowserContext(browser_context_);

  update_event_router_ = std::make_unique<UpdateEventRouter>(
      profile, UpdateServiceFactory::GetForProfile(profile));

  EventRouter::Get(browser_context_)->UnregisterObserver(this);
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
void AutoUpdateAPI::SendUpdaterDidNotFindUpdate(const std::string& reason) {
  ::vivaldi::BroadcastEventToAllProfiles(
      auto_update::OnUpdaterDidNotFindUpdate::kEventName,
      auto_update::OnUpdaterDidNotFindUpdate::Create(
          auto_update::ParseUpdateNotFoundReason(reason)));
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
void AutoUpdateAPI::SendNeedRestartToReloadCodecs() {
  LOG(INFO) << "A/V support updated";
  ::vivaldi::BroadcastEventToAllProfiles(
      auto_update::OnNeedRestartToReloadCodecs::kEventName);
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

/* static */
void AutoUpdateAPI::SendUpdateFinished() {
  ::vivaldi::BroadcastEventToAllProfiles(
      auto_update::OnUpdateFinished::kEventName);
}

void AutoUpdateAPI::SendUpdateProgress(const AutoUpdateStatus& status,
                                       const std::string& reason,
                                       const int progress) {
  ::vivaldi::BroadcastEventToAllProfiles(
      auto_update::OnUpdateProgress::kEventName,
      auto_update::OnUpdateProgress::Create(ToUpdateStatus(status), reason,
                                            progress));
}

#if !BUILDFLAG(IS_ANDROID)
void AutoUpdateAPI::InitWidevineMonitoring() {
  auto* component_updater = g_browser_process->component_updater();

  if (!component_updater) {
    LOG(ERROR) << "Could not get component updater. Widevine update monitoring "
                  "not available.";
    return;
  }

  // Already available, no need to do anything here...
  widevine_was_available_ = false;

  for (const auto& ci : component_updater->GetComponents()) {
    if (ci.id == kWidevineComponentID) {
      widevine_was_available_ =
          (ci.version.IsValid() &&
           (ci.version != base::Version{updater::kNullVersion}));
      break;
    }
  }

  // Attach a scoped observer to component updater.
  observer_.Observe(component_updater);
}

// Called on component_updater events.
void AutoUpdateAPI::OnEvent(const update_client::CrxUpdateItem& item) {
  if (item.id == kWidevineComponentID) {
    if (item.state == update_client::ComponentState::kUpdated) {
      LOG(INFO) << "AutoUpdateAPI: Informing widevine was updated.";
      HandleWidevineUpdated();
      return;
    }
  }
}

void AutoUpdateAPI::HandleWidevineUpdated() {
  widevine_was_updated_ = true;

  // We can de-register the observer now.
  observer_.Reset();

  // Consider if we need a restart - Linux only.
  HandleRequestedWidevineUpdate();
}

void AutoUpdateAPI::HandleRequestedWidevineUpdate() {
#if BUILDFLAG(IS_LINUX)
  // Note: This handles Restart to reload on linux. For other platforms, we
  // reload the tab in DRMContentTabHelper
  if (widevine_was_available_ || !widevine_was_updated_ ||
      !widevine_was_requested_)
    return;

  SendNeedRestartToReloadCodecs();
#endif
}

void AutoUpdateAPI::HandleWidevineRequested() {
  // This gets called from DRMContentTabHelper and handles needed steps to
  // install widevine CDM if that wasn't yet available.
  if (widevine_was_requested_) {
    // We already saw a request to install widevine.
    // We handle this case here in case the update happened before we were
    // able to signal it to user.
    HandleRequestedWidevineUpdate();
    return;
  }

  widevine_was_requested_ = true;

  auto* component_updater = g_browser_process->component_updater();
  if (!component_updater) {
    LOG(ERROR)
        << "Could not get component updater. Widevine update not possible.";
    return;
  }
  auto& on_demand_updater = component_updater->GetOnDemandUpdater();

  // In time, this will invoke OnEvent for installed update.
  on_demand_updater.OnDemandUpdate(
      kWidevineComponentID,
      component_updater::OnDemandUpdater::Priority::BACKGROUND,
      base::BindOnce([](update_client::Error error) {
      }));  // We're listening to component changes, no need to handle
            // here...

  // For all situations we look if the conditions are right for restart
  // notification.
  HandleRequestedWidevineUpdate();
}

#endif  // !BUILDFLAG(IS_ANDROID)

void AutoUpdateGetUpdateStatusFunction::SendResult(
    std::optional<AutoUpdateStatus> status,
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
        status_object.status =
            vivaldi::auto_update::UpdateOperationStatusEnum::kDidAbortWithError;
        break;
      case AutoUpdateStatus::kDidFindValidUpdate:
        status_object.status = vivaldi::auto_update::UpdateOperationStatusEnum::
            kDidFindValidUpdate;
        break;
      case AutoUpdateStatus::kWillDownloadUpdate:
        status_object.status = vivaldi::auto_update::UpdateOperationStatusEnum::
            kWillDownloadUpdate;
        break;
      case AutoUpdateStatus::kDidDownloadUpdate:
        status_object.status =
            vivaldi::auto_update::UpdateOperationStatusEnum::kDidDownloadUpdate;
        break;
      case AutoUpdateStatus::kWillInstallUpdateOnQuit:
        status_object.status = vivaldi::auto_update::UpdateOperationStatusEnum::
            kWillInstallUpdateOnQuit;
        break;
      case AutoUpdateStatus::kUpdaterDidRelaunchApplication:
        status_object.status = vivaldi::auto_update::UpdateOperationStatusEnum::
            kUpdaterDidRelaunchApplication;
        break;
      case AutoUpdateStatus::kError:
        status_object.status =
            vivaldi::auto_update::UpdateOperationStatusEnum::kError;
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

void AutoUpdateNeedsCodecRestartFunction::DeliverResult(bool enabled) {
  namespace Results = vivaldi::auto_update::NeedsCodecRestart::Results;
  Respond(ArgumentList(Results::Create(enabled)));
}

void AutoUpdateStartUpdateFunction::StartUpdateCB() {
  Respond(NoArguments());
}

std::string getAnnotatedVersion() {
  return base::StrCat(
      {::vivaldi::GetVivaldiVersionString(), " (",

       l10n_util::GetStringUTF8(version_info::IsOfficialBuild()
                                    ? IDS_VERSION_UI_OFFICIAL
                                    : IDS_VERSION_UI_UNOFFICIAL),
       ") ", l10n_util::GetStringUTF8(VersionUI::VersionProcessorVariation())});
}

ExtensionFunction::ResponseAction AutoUpdateGetAboutInfoFunction::Run() {
  namespace Results = vivaldi::auto_update::GetAboutInfo::Results;

  vivaldi::auto_update::About response;
  response.user_agent = embedder_support::GetUserAgent();

#if BUILDFLAG(IS_WIN)
  response.command_line = base::WideToUTF8(
      base::CommandLine::ForCurrentProcess()->GetCommandLineString());
#else
  // from version_ui.cc
  std::string command_line;
  using ArgvList = std::vector<std::string>;
  const ArgvList& argv = base::CommandLine::ForCurrentProcess()->argv();
  for (const auto& iter : argv) {
    command_line += " " + iter;
  }
  response.command_line = command_line;
#endif

  response.copyright =
      base::UTF16ToUTF8(base::i18n::MessageFormatter::FormatWithNumberedArgs(
          l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_COPYRIGHT),
          base::Time::Now()));
  response.js_version = V8_VERSION_STRING;
  response.js_engine = "V8";

  response.company_name =
      l10n_util::GetStringUTF8(IDS_ABOUT_VERSION_COMPANY_NAME);
  response.annotated_version = getAnnotatedVersion();
  response.channel = l10n_util::GetStringUTF8(version_info::IsOfficialBuild()
                                                  ? IDS_VERSION_UI_OFFICIAL
                                                  : IDS_VERSION_UI_UNOFFICIAL);

  response.cl = version_info::GetLastChange();

#if BUILDFLAG(IS_MAC)
  response.mac_linker = CHROMIUM_LINKER_NAME;
#endif  // BUILDFLAG(IS_MAC)

  return RespondNow(ArgumentList(Results::Create(response)));
}

FilePathResult GetFilePaths(Profile* profile) {
  FilePathResult result;

  base::FilePath executable_path = base::MakeAbsoluteFilePath(
      base::CommandLine::ForCurrentProcess()->GetProgram());

  result.executable_path =
      base::UTF16ToUTF8(executable_path.LossyDisplayName());
  result.profile_path =
      base::UTF16ToUTF8(profile->GetPath().LossyDisplayName());

  return result;
}

ExtensionFunction::ResponseAction AutoUpdateGetAboutPathsInfoFunction::Run() {
  base::RepeatingClosure barrier = base::BarrierClosure(
      2, base::BindOnce(&AutoUpdateGetAboutPathsInfoFunction::OnDone, this));

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&GetFilePaths, GetProfile()),
      base::BindOnce(&AutoUpdateGetAboutPathsInfoFunction::OnFilePathCallback,
                     this, barrier));

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(
          &AutoUpdateGetAboutPathsInfoFunction::GetPlatformOSVersion),
      base::BindOnce(&AutoUpdateGetAboutPathsInfoFunction::OnVersion, this,
                     barrier));

  return RespondLater();
}

void AutoUpdateGetAboutPathsInfoFunction::OnFilePathCallback(
    base::RepeatingClosure done_closure,
    const FilePathResult& paths) {
  paths_result_ = std::move(paths);

  std::move(done_closure).Run();
}

void AutoUpdateGetAboutPathsInfoFunction::OnDone() {
  vivaldi::auto_update::AboutPaths response;
  response.exe_path = paths_result_.executable_path;
  response.profile_path = paths_result_.profile_path;
  response.full_os_version = os_full_version_;

  namespace Results = vivaldi::auto_update::GetAboutPathsInfo::Results;
  Respond(ArgumentList(Results::Create(response)));
}

void AutoUpdateGetAboutPathsInfoFunction::OnVersion(
    base::RepeatingClosure done_closure,
    const std::string& version) {
  os_full_version_ = std::move(version);

  std::move(done_closure).Run();
}

}  // namespace extensions
