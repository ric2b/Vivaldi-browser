// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/auto_update/auto_update_api.h"

#include "extensions/schema/autoupdate.h"
#include "extensions/tools/vivaldi_tools.h"

namespace auto_update = extensions::vivaldi::auto_update;

namespace extensions {

/* static */
void AutoUpdateAPI::SendDidFindValidUpdate(const std::string& url) {
  ::vivaldi::BroadcastEventToAllProfiles(
      auto_update::OnDidFindValidUpdate::kEventName,
      auto_update::OnDidFindValidUpdate::Create(url));
}

/* static */
void AutoUpdateAPI::SendWillDownloadUpdate() {
  ::vivaldi::BroadcastEventToAllProfiles(
      auto_update::OnWillDownloadUpdate::kEventName);
}

/* static */
void AutoUpdateAPI::SendDidDownloadUpdate() {
  ::vivaldi::BroadcastEventToAllProfiles(
      auto_update::OnDidDownloadUpdate::kEventName);
}

/* static */
void AutoUpdateAPI::SendWillInstallUpdateOnQuit() {
  ::vivaldi::BroadcastEventToAllProfiles(
      auto_update::OnWillInstallUpdateOnQuit::kEventName);
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
void AutoUpdateAPI::SendDidAbortWithError() {
  ::vivaldi::BroadcastEventToAllProfiles(
      auto_update::OnDidAbortWithError::kEventName);
}

}  // namespace extensions
