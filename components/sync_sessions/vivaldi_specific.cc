// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync_sessions/synced_session.h"
#include "components/sync_sessions/vivaldi_specific.h"

namespace sync_sessions {

// This code implements conversions:
// sync_pb::VivaldiSpecific <=> sync_sessions::VivaldiSpecific
//
// sync_pb::VivaldiSpecific => sync_sessions::VivaldiSpecific
// It's used when a message (protobuf) with the VivaldiSpecific data arrives.
//
// sync_sessions::VivaldiSpecific => sync_pb::VivaldiSpecific
// Is used when the data on the vivaldi side has changed and we want to
// update the sync server.

void SetVivaldiSpecificFromSyncData(const sync_pb::VivaldiSpecific& sync_data,
    sync_sessions::VivaldiSpecific *data)
{
  DCHECK(data);

  if (sync_data.vivaldi_panels_size() > 0) {
    sync_sessions::VivaldiSpecific::Panels panels;
    for (auto &sync_panel: sync_data.vivaldi_panels()) {
      sync_sessions::VivaldiSpecific::Panel panel;
      panel.url = sync_panel.initial_url();

      if (sync_panel.has_title())
        panel.title = sync_panel.title();

      if (sync_panel.has_initial_favicon_url())
        panel.initial_favicon_url = sync_panel.initial_favicon_url();

      if (sync_panel.has_panel_id()) {
        panel.id = sync_panel.panel_id();
      }

      panels.push_back(panel);
    }

    data->panels = panels;
  }

  if (sync_data.vivaldi_workspaces_size() > 0) {
    sync_sessions::VivaldiSpecific::Workspaces workspaces;
    for (auto &sync_workspace: sync_data.vivaldi_workspaces()) {
      if (!sync_workspace.has_id()) {
        LOG(WARNING) << "missing VivaldiWorkspace::id in sync_pb::VivaldiSpecific";
        continue;
      }

      sync_sessions::VivaldiSpecific::Workspace workspace;
      workspace.id = sync_workspace.id();

      if (sync_workspace.has_name())
        workspace.name = sync_workspace.name();

      if (sync_workspace.has_icon_id())
        workspace.icon_id = sync_workspace.icon_id();

      if (sync_workspace.has_emoji())
        workspace.emoji = sync_workspace.emoji();

      // We sync the icon SVG only if we don't have the iconId or an emoji.
      if (sync_workspace.has_icon() && !sync_workspace.has_icon_id() &&
          !sync_workspace.has_emoji())
        workspace.icon = sync_workspace.icon();

      if (!data->workspaces)
        data->workspaces = sync_sessions::VivaldiSpecific::Workspaces();

      data->workspaces->push_back(workspace);
    }
  }
}

void SetSyncDataFromVivaldiSpecific(const VivaldiSpecific& data,
                                    sync_pb::VivaldiSpecific* vivaldi_specific) {
  DCHECK(vivaldi_specific);
  if (data.panels) {
    for (auto& panel : *data.panels) {
      auto* viv_panel = vivaldi_specific->add_vivaldi_panels();
      viv_panel->set_initial_url(panel.url);
      if (panel.title)
        viv_panel->set_title(*panel.title);
      if (panel.initial_favicon_url)
        viv_panel->set_initial_favicon_url(*panel.initial_favicon_url);
      if (panel.id)
        viv_panel->set_panel_id(*panel.id);
    }
  }

  if (data.workspaces) {
    for (auto& workspace : *data.workspaces) {
      auto* viv_workspace = vivaldi_specific->add_vivaldi_workspaces();
      viv_workspace->set_id(workspace.id);
      viv_workspace->set_name(workspace.name);
      if (workspace.icon_id)
        viv_workspace->set_icon_id(*workspace.icon_id);
      if (workspace.emoji)
        viv_workspace->set_emoji(*workspace.emoji);
      if (workspace.icon)
        viv_workspace->set_icon(*workspace.icon);
    }
  }
}

} // namespace sync_sessions
