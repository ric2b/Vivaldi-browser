// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/auto_update/auto_update_api.h"
#include "components/update_client/crx_update_item.h"

#include "extensions/schema/autoupdate.h"
#include "extensions/tools/vivaldi_tools.h"

#include "base/lazy_instance.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/browser_process.h"
#include "chrome/updater/constants.h"
#endif

namespace auto_update = extensions::vivaldi::auto_update;

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

AutoUpdateAPI::AutoUpdateAPI(content::BrowserContext* context)
    : browser_context_(context) {
  LOG(INFO) << "AutoUpdateAPI::Init";
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  InitUpgradeDetection();
#endif
#if !BUILDFLAG(IS_ANDROID)
  InitWidevineMonitoring();
#endif
}

AutoUpdateAPI::~AutoUpdateAPI() {}

void AutoUpdateAPI::Shutdown() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  ShutdownUpgradeDetection();
#endif
}

/* static */
void AutoUpdateAPI::HandleRestartPreconditions() {
#if BUILDFLAG(IS_LINUX)
  HandleCodecRestartPreconditions();
#endif // BUILDFLAG(IS_LINUX)
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
  // Note: This handles Restart to reload on linux. For other platforms, we reload the tab in DRMContentTabHelper
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
    // We handle this case here in case the update happened before we were able
    // to signal it to user.
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
      }));  // We're listening to component changes, no need to handle here...

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

void AutoUpdateNeedsCodecRestartFunction::DeliverResult(bool enabled) {
  namespace Results = vivaldi::auto_update::NeedsCodecRestart::Results;
  Respond(ArgumentList(Results::Create(enabled)));
}

}  // namespace extensions
