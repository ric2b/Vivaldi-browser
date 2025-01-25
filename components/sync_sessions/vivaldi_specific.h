#ifndef COMPONENTS_SYNC_SESSIONS_VIVALDI_SPECIFIC_H_
#define COMPONENTS_SYNC_SESSIONS_VIVALDI_SPECIFIC_H_

#include <string>
#include <optional>
#include <vector>

namespace sync_pb {
class VivaldiSpecific;
}

namespace sync_sessions {

// Editing VivaldiSpecific?
// Follow the checklist:
//
//  For reading VivaldiSpecific from prefs.
//  - vivaldi/extensions/api/sync/sync_api.cc GetPanels() and GetWorkspaces()
//
//  To extend sync-server message.
//  - chromium/components/sync/protocol/session_specifics.proto
//
//  For sync_pb::VivaldiSpecific <=> sync_sessions::VivaldiSpecific conversion.
//  - vivaldi/components/sync_sessions/vivaldi_synced_session.cc
//
//  If you want to extend getDevices() output
//  - chromium/chrome/browser/extensions/api/sessions/sessions_api.cc
//  - chromium/chrome/common/extensions/api/sessions.json

struct VivaldiSpecific {
  struct Panel {
    std::string url;
    std::optional<std::string> id;
    std::optional<std::string> title;
    std::optional<std::string> initial_favicon_url;
  };

  struct Workspace {
    double id = 0;
    std::string name;
    std::optional<int> icon_id;
    std::optional<std::string> emoji;
    std::optional<std::string> icon;
  };

  using Panels = std::vector<Panel>;
  using Workspaces = std::vector<Workspace>;

  std::optional<Panels> panels;
  std::optional<Workspaces> workspaces;
};

void SetVivaldiSpecificFromSyncData(const sync_pb::VivaldiSpecific& sync_data,
    sync_sessions::VivaldiSpecific *);

void SetSyncDataFromVivaldiSpecific(const VivaldiSpecific& data,
    sync_pb::VivaldiSpecific *viv_specific);
} // namespace sync_sessions

#endif // COMPONENTS_SYNC_SESSIONS_VIVALDI_SPECIFIC_H_
