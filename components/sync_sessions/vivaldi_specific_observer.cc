// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "app/vivaldi_apptools.h"
#include "base/lazy_instance.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"
#include "chromium/components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/sync_sessions/vivaldi_specific.h"
#include "content/public/browser/browser_context.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

#include "components/sync_sessions/vivaldi_specific_observer.h"

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

void GetWorkspaces(PrefService* prefs, sync_sessions::VivaldiSpecific& specific) {
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
} // namespace

namespace vivaldi {
VivSpecificObserver::~VivSpecificObserver() {}

VivSpecificObserver::VivSpecificObserver(Profile* profile) : profile_(profile) {
  if (!vivaldi::IsVivaldiRunning())
    return;

  prefs_registrar_.Init(profile_->GetPrefs());
  // vivaldi.panels.web.elements
  prefs_registrar_.Add(vivaldiprefs::kPanelsWebElements,
                       base::BindRepeating(&VivSpecificObserver::OnPrefsChanged,
                                           base::Unretained(this)));
  // vivaldi.workspaces.list
  prefs_registrar_.Add(vivaldiprefs::kWorkspacesList,
                       base::BindRepeating(&VivSpecificObserver::OnPrefsChanged,
                                           base::Unretained(this)));
}

void VivSpecificObserver::TriggerSync() {
  if (!vivaldi::IsVivaldiRunning())
    return;
  DCHECK(profile_);
  sync_sessions::SyncSessionsWebContentsRouter* router =
      sync_sessions::SyncSessionsWebContentsRouterFactory::GetForProfile(
          profile_);
  PrefService* prefs = profile_->GetOriginalProfile()->GetPrefs();
  DCHECK(router && prefs);

  if (!router || !prefs) {
    return;
  }

  sync_sessions::VivaldiSpecific specific;
  GetPanels(prefs, specific);
  GetWorkspaces(prefs, specific);
  router->UpdateVivExtData(specific);
}

void VivSpecificObserver::OnPrefsChanged(const std::string& path) {
  DCHECK(path == vivaldiprefs::kWorkspacesList ||
         path == vivaldiprefs::kPanelsWebElements);
  TriggerSync();
}

}  // namespace vivaldi
