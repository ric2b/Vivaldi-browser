// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/auto_update/auto_update_api.h"

#include "extensions/schema/autoupdate.h"
#include "extensions/tools/vivaldi_tools.h"

namespace auto_update = extensions::vivaldi::auto_update;

namespace extensions {

namespace {

std::string GetVersionString(const base::Version& version) {
  if (!version.IsValid())
    return std::string();
  return version.GetString();
}

}  // namespace

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
            vivaldi::auto_update::UPDATE_OPERATION_STATUS_ENUM_NOUPDATE;
        break;
      case AutoUpdateStatus::kDidAbortWithError:
        status_object.status = vivaldi::auto_update::
            UPDATE_OPERATION_STATUS_ENUM_DIDABORTWITHERROR;
        break;
      case AutoUpdateStatus::kDidFindValidUpdate:
        status_object.status = vivaldi::auto_update::
            UPDATE_OPERATION_STATUS_ENUM_DIDFINDVALIDUPDATE;
        break;
      case AutoUpdateStatus::kWillDownloadUpdate:
        status_object.status = vivaldi::auto_update::
            UPDATE_OPERATION_STATUS_ENUM_WILLDOWNLOADUPDATE;
        break;
      case AutoUpdateStatus::kDidDownloadUpdate:
        status_object.status = vivaldi::auto_update::
            UPDATE_OPERATION_STATUS_ENUM_DIDDOWNLOADUPDATE;
        break;
      case AutoUpdateStatus::kWillInstallUpdateOnQuit:
        status_object.status = vivaldi::auto_update::
            UPDATE_OPERATION_STATUS_ENUM_WILLINSTALLUPDATEONQUIT;
        break;
      case AutoUpdateStatus::kUpdaterDidRelaunchApplication:
        status_object.status = vivaldi::auto_update::
            UPDATE_OPERATION_STATUS_ENUM_UPDATERDIDRELAUNCHAPPLICATION;
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
