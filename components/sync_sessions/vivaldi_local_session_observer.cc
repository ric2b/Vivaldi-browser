// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/sync_sessions/vivaldi_local_session_observer.h"

#include "app/vivaldi_apptools.h"
#include "base/lazy_instance.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/local_device_info_util.h"
#include "components/sync_sessions/vivaldi_specific.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

#if BUILDFLAG(IS_IOS)
#include "components/sync_sessions/session_sync_service_impl.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#include "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#include "ios/chrome/browser/tabs/model/ios_chrome_local_session_event_router.h"
#else
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router.h"
#endif

namespace {
void GetPanels(PrefService* prefs, sync_sessions::VivaldiSpecific& specific) {
  const base::Value& value = prefs->GetValue("vivaldi.panels.web.elements");
  auto* list = value.GetIfList();
  if (!list)
    return;
  for (auto& it : *list) {
    auto* dict = it.GetIfDict();
    if (!dict)
      continue;

    sync_sessions::VivaldiSpecific::Panel panel;

    const std::string* id = dict->FindString("id");
    if (id) {
      panel.id = *id;
    }

    const std::string* url = dict->FindString("url");
    if (url) {
      panel.url = *url;
    }

    const std::string* title = dict->FindString("title");
    if (title) {
      panel.title = *title;
    }

    const std::string* favicon_url = dict->FindString("faviconUrl");
    if (favicon_url) {
      panel.initial_favicon_url = *favicon_url;
    }

    if (!specific.panels) {
      specific.panels = sync_sessions::VivaldiSpecific::Panels();
    }

    specific.panels->push_back(panel);
  }
}

void GetWorkspaces(PrefService* prefs,
                   sync_sessions::VivaldiSpecific& specific) {
  const base::Value& value = prefs->GetValue("vivaldi.workspaces.list");
  auto* list = value.GetIfList();
  if (!list)
    return;

  for (auto& it : *list) {
    sync_sessions::VivaldiSpecific::Workspace workspace;
    auto* dict = it.GetIfDict();
    if (!dict)
      continue;

    std::optional<double> workspaceId = dict->FindDouble("id");
    if (!workspaceId)
      continue;

    workspace.id = *workspaceId;

    const std::string* name = dict->FindString("name");
    if (name) {
      workspace.name = *name;
    }

    const std::string* emoji = dict->FindString("emoji");
    if (emoji) {
      workspace.emoji = *emoji;
    }

    const std::string* icon = dict->FindString("icon");
    if (icon) {
      workspace.icon = *icon;
    }

    workspace.icon_id = dict->FindInt("iconId");
    if (!specific.workspaces) {
      specific.workspaces = sync_sessions::VivaldiSpecific::Workspaces();
    }

    specific.workspaces->push_back(workspace);
  }
}
}  // namespace

namespace vivaldi {
VivaldiLocalSessionObserver::~VivaldiLocalSessionObserver() {
  DeregisterDeviceInfoObservers();
}

#if BUILDFLAG(IS_IOS)
VivaldiLocalSessionObserver::VivaldiLocalSessionObserver(
    ProfileClass* profile,
    sync_sessions::SyncSessionsClient* sessions_client)
    : profile_(profile),
      device_info_service_(
          vivaldi::IsVivaldiRunning()
              ? DeviceInfoSyncServiceFactory::GetForProfile(profile_)
              : nullptr),
      sessions_client_(sessions_client) {
#else
VivaldiLocalSessionObserver::VivaldiLocalSessionObserver(ProfileClass* profile)
    : profile_(profile),
      device_info_service_(
          vivaldi::IsVivaldiRunning()
              ? DeviceInfoSyncServiceFactory::GetForProfile(profile_)
              : nullptr) {
#endif
  if (!vivaldi::IsVivaldiRunning())
    return;

  CHECK(device_info_service_);
#if BUILDFLAG(IS_IOS)
  CHECK(sessions_client_);
#endif

  device_info_service_->GetDeviceInfoTracker()->AddObserver(this);

  specific_prefs_registrar_.Init(profile_->GetPrefs());
  // vivaldi.panels.web.elements
  specific_prefs_registrar_.Add(
      vivaldiprefs::kPanelsWebElements,
      base::BindRepeating(&VivaldiLocalSessionObserver::OnSpecificPrefsChanged,
                          base::Unretained(this)));
  // vivaldi.workspaces.list
  specific_prefs_registrar_.Add(
      vivaldiprefs::kWorkspacesList,
      base::BindRepeating(&VivaldiLocalSessionObserver::OnSpecificPrefsChanged,
                          base::Unretained(this)));

  session_name_prefs_registrar_.Init(profile_->GetPrefs());
  // vivaldi.sync.session_name
  session_name_prefs_registrar_.Add(
      vivaldiprefs::kSyncSessionName,
      base::BindRepeating(
          &VivaldiLocalSessionObserver::OnSessionNamePrefsChanged,
          base::Unretained(this)));

  // Prepare fallback name. It will never change and is the same as the
  // default name in SessionStore. It will be used if user clears a custom name
  // that has earlier modified the name in SessionStore.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&syncer::GetPersonalizableDeviceNameBlocking),
      base::BindOnce(
          &VivaldiLocalSessionObserver::SetFallbackDeviceName,
          weak_factory_.GetWeakPtr()));
}

void VivaldiLocalSessionObserver::OnDeviceInfoChange() {
  UpdateSession();
}

void VivaldiLocalSessionObserver::OnDeviceInfoShutdown() {
  DeregisterDeviceInfoObservers();
}

void VivaldiLocalSessionObserver::DeregisterDeviceInfoObservers() {
  if (!device_info_service_) {
    return;
  }
  device_info_service_->GetDeviceInfoTracker()->RemoveObserver(this);
  session_name_prefs_registrar_.RemoveAll();
  device_info_service_ = nullptr;
}

void VivaldiLocalSessionObserver::UpdateSession() {
#if BUILDFLAG(IS_IOS)
  IOSChromeLocalSessionEventRouter* router =
      static_cast<IOSChromeLocalSessionEventRouter*>(
        sessions_client_->GetLocalSessionEventRouter());
#else
  sync_sessions::SyncSessionsWebContentsRouter* router =
      sync_sessions::SyncSessionsWebContentsRouterFactory::GetForProfile(
          profile_);
#endif

  if (router) {
    PrefService* prefs = profile_->GetOriginalProfile()->GetPrefs();
    std::string candidate = prefs->GetString(vivaldiprefs::kSyncSessionName);

    std::string device_name;
    base::TrimWhitespaceASCII(candidate, base::TrimPositions::TRIM_ALL,
                              &device_name);

    if (!device_name.empty()) {
      router->UpdateDeviceName(device_name);
    } else if (!fallback_device_name_.empty()) {
      router->UpdateDeviceName(fallback_device_name_);
    }
  }
}

void VivaldiLocalSessionObserver::SetFallbackDeviceName(
  const std::string& device_name) {
  fallback_device_name_ = device_name;
}

void VivaldiLocalSessionObserver::TriggerSync() {
  if (!vivaldi::IsVivaldiRunning())
    return;
  CHECK(profile_);

#if BUILDFLAG(IS_IOS)
  IOSChromeLocalSessionEventRouter* router =
      static_cast<IOSChromeLocalSessionEventRouter*>(
        sessions_client_->GetLocalSessionEventRouter());
#else
  sync_sessions::SyncSessionsWebContentsRouter* router =
      sync_sessions::SyncSessionsWebContentsRouterFactory::GetForProfile(
          profile_);
#endif

  if (!router)
    return;

  PrefService* prefs = profile_->GetOriginalProfile()->GetPrefs();

  if (!prefs)
    return;

  sync_sessions::VivaldiSpecific specific;
  GetPanels(prefs, specific);
  GetWorkspaces(prefs, specific);
  router->UpdateVivExtData(specific);
}

void VivaldiLocalSessionObserver::OnSessionNamePrefsChanged(
    const std::string& path) {
  CHECK(path == vivaldiprefs::kSyncSessionName);
  if (device_info_service_)
    device_info_service_->RefreshLocalDeviceInfo();
}

void VivaldiLocalSessionObserver::OnSpecificPrefsChanged(
    const std::string& path) {
  CHECK(path == vivaldiprefs::kWorkspacesList ||
        path == vivaldiprefs::kPanelsWebElements);
  TriggerSync();
}

}  // namespace vivaldi
