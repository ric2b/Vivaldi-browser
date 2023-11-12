// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

// included from components/sessions/core/session_service_commands.cc

using IdToSessionTab = std::map<SessionID, std::unique_ptr<SessionTab>>;
using IdToSessionWindow = std::map<SessionID, std::unique_ptr<SessionWindow>>;
using TokenToSessionTabGroup =
std::map<tab_groups::TabGroupId, std::unique_ptr<SessionTabGroup>>;

void VivaldiCreateTabsAndWindows(
    const std::vector<std::unique_ptr<sessions::SessionCommand>>& data,
    IdToSessionTab* tabs,
    TokenToSessionTabGroup* tab_groups,
    IdToSessionWindow* windows,
    SessionID* active_window_id) {
  CreateTabsAndWindows(data, tabs, tab_groups, windows, active_window_id);
}
